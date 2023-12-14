#ifndef HSL_H_
#define HSL_H_

#include "color/color.h"

#ifdef __cplusplus
extern "C"{
#endif

rgb_t hsl_to_rgb(hsl_t hsl);

hsl_t rgb_to_hsl(rgb_t rgb);

#ifdef __cplusplus
}
#endif

#endif