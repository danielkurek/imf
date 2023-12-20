#ifndef COLOR_H_
#define COLOR_H_

#include <inttypes.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C"{
#endif

typedef struct{
    uint8_t red;
    uint8_t green;
    uint8_t blue;
} rgb_t;

typedef struct{
    uint16_t lightness;
    uint16_t hue;
    uint16_t saturation;
} hsl_t;

#define RGB_STR_LEN 6+1

esp_err_t rgb_to_str(rgb_t color, size_t buf_len, char buf[]);
esp_err_t str_to_rgb(const char *buf, rgb_t *color);

#ifdef __cplusplus
}
#endif

#endif