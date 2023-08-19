#include "hsl.h"
#include "hsl_github.h"

rgb_t hsl_to_rgb(hsl_t hsl){
    float h = ((float) hsl.hue) / UINT16_MAX;
    float s = ((float) hsl.saturation) / UINT16_MAX;
    float l = ((float) hsl.lightness) / UINT16_MAX;
    RGB result = hsl2rgb(h, s, l);
    rgb_t rgb = {
        .red = (uint16_t) result.r,
        .green = (uint16_t) result.g,
        .blue = (uint16_t) result.b,
    };
    return rgb;
}

hsl_t rgb_to_hsl(rgb_t rgb){
    float r = (((float) rgb.red) / UINT16_MAX) * 255;
    float g = (((float) rgb.green) / UINT16_MAX) * 255;
    float b = (((float) rgb.blue) / UINT16_MAX) * 255;
    HSL result = rgb2hsl(r, g, b);
    hsl_t hsl = {
        .hue = (uint16_t) (result.h * UINT16_MAX),
        .saturation = (uint16_t) (result.s * UINT16_MAX),
        .lightness = (uint16_t) (result.l * UINT16_MAX),
    };
    return hsl;
}