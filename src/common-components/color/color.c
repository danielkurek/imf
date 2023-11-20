#include <stdio.h>
#include "color.h"
#include <inttypes.h>

esp_err_t rgb_to_str(rgb_t color, size_t buf_len, char buf[]){
    int ret = snprintf(buf, buf_len, "%4" PRIx16 "%4" PRIx16 "%4" PRIx16, color.red, color.green, color.blue);
    if(ret > 0){
        return ESP_OK;
    }
    return ESP_FAIL;
}

esp_err_t str_to_rgb(const char *buf, rgb_t *color){
    int ret = sscanf(buf, "%4" SCNx16 "%4" SCNx16 "%4" SCNx16, &(color->red), &(color->green), &(color->blue));
    if(ret == 3){
        return ESP_OK;
    }
    return ESP_FAIL;
}