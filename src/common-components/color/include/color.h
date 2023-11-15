#ifndef COLOR_H_
#define COLOR_H_

#include <inttypes.h>

#ifdef __cplusplus
extern "C"{
#endif
typedef struct{
    uint16_t red;
    uint16_t green;
    uint16_t blue;
} rgb_t;

typedef struct {
    uint16_t lightness;
    uint16_t hue;
    uint16_t saturation;
} hsl_t;

#ifdef __cplusplus
}
#endif

#endif