#include "neato_esp.h"
#include "gvc.h"
#include "common/color.h"
#include "gvcjob.h"
#include "gvcint.h"
#include "gvplugin_layout.h"
#include <stdio.h>
#include "cgraph.h"
#include "alloc.h"
#include "gvcproc.h"
#include "const.h"

typedef enum { LAYOUT_NEATO,
	} layout_type;

typedef enum {
	FORMAT_DOT,
} format_type;

extern void neato_layout(graph_t * g);
extern void neato_cleanup(graph_t * g);

static gvlayout_engine_t neatogen_engine = {
    neato_layout,
    neato_cleanup,
};

static gvlayout_features_t neatogen_features = {
        0,
};

// replaces gvLayout to get rid of dynamic library loading
int neato_esp_layout(GVC_t *gvc, graph_t *g){
    char buf[256];
    int rc;

    // instead of gvlayout_select
    gvc->layout.type = "neato";
	gvc->layout.engine = &neatogen_engine;
	gvc->layout.id = LAYOUT_NEATO;
	gvc->layout.features = &neatogen_features;

    if (gvLayoutJobs(gvc, g) == -1)
	return -1;

    /* set bb attribute for basic layout.
 * doesn't yet include margins, scaling or page sizes because
 * those depend on the renderer being used. */
    if (GD_drawing(g)->landscape)
        snprintf(buf, sizeof(buf), "%d %d %d %d",
                 ROUND(GD_bb(g).LL.y), ROUND(GD_bb(g).LL.x),
                 ROUND(GD_bb(g).UR.y), ROUND(GD_bb(g).UR.x));
    else
        snprintf(buf, sizeof(buf), "%d %d %d %d",
                 ROUND(GD_bb(g).LL.x), ROUND(GD_bb(g).LL.y),
                 ROUND(GD_bb(g).UR.x), ROUND(GD_bb(g).UR.y));
    agsafeset(g, "bb", buf, "");

    return 0;
}


extern gvplugin_installed_t gvdevice_dot_types[];
extern gvplugin_installed_t gvrender_dot_types[];

int neato_esp_render(GVC_t *gvc, graph_t *g, FILE *out){
    int rc;
    GVJ_t *job;
    gvplugin_installed_t *typeptr;

    /* create a job for the required format */
    // gvjobs_output_langname without gvplugin_load
	
    gvc->job = gvc->jobs = gv_alloc(sizeof(GVJ_t));
    
    gvc->job->output_langname = "dot";
    gvc->job->gvc = gvc;
    job = gvc->job;

    typeptr = &gvdevice_dot_types[0];
    job->device.engine = typeptr->engine;
	job->device.features = typeptr->features;
	job->device.id = typeptr->id;
	job->device.type = "dot:dot";

	job->flags |= job->device.features->flags;

    typeptr = &gvrender_dot_types[0];
    job->render.engine = typeptr->engine;
	job->render.features = typeptr->features;
	job->render.type = "dot";

	job->flags |= job->render.features->flags;

	if (job->device.engine)
	    job->render.id = typeptr->id;
	else
	    /* A null device engine indicates that the device id is also the renderer id
	     * and that the renderer doesn't need "device" functions.
	     * Device "features" settings are still available */
	    job->render.id = job->device.id;
    job->output_lang = GVRENDER_PLUGIN;
    if (!LAYOUT_DONE(g) && !(job->flags & LAYOUT_NOT_REQUIRED)) {
	agerrorf( "Layout was not done\n");
	return -1;
    }
    job->output_file = out;
    if (out == NULL)
	job->flags |= OUTPUT_NOT_REQUIRED;
    rc = gvRenderJobs(gvc, g);
    gvrender_end_job(job);
    gvjobs_delete(gvc);

    return rc;
}
