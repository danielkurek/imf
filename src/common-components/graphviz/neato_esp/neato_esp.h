#ifndef NEATO_ESP_H_
#define NEATO_ESP_H_

#include "gvc.h"
#include "types.h"

#ifdef __cplusplus
extern "C"{
#endif

int neato_esp_layout(GVC_t *gvc, graph_t *g);
int neato_esp_render(GVC_t *gvc, graph_t *g, FILE *out);

#ifdef __cplusplus
}
#endif

#endif