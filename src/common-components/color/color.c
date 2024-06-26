/**
 * @file color.c
 * @author Daniel Kurek (daniel.kurek.dev@gmail.com)
 * @brief Implementation of @ref color.h
 * @version 0.1
 * @date 2023-08-19
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#include <stdio.h>
#include "color/color.h"
#include <inttypes.h>
#include <string.h>

esp_err_t rgb_to_str(rgb_t color, size_t buf_len, char buf[]){
    int ret = snprintf(buf, buf_len, "%02" PRIx8 "%02" PRIx8 "%02" PRIx8, color.red, color.green, color.blue);
    if(ret > 0){
        return ESP_OK;
    }
    return ESP_FAIL;
}

esp_err_t str_to_rgb(const char *buf, rgb_t *color){
    if(strlen(buf) != 6) return ESP_FAIL;

    int ret = sscanf(buf, "%02" SCNx8 "%02" SCNx8 "%02" SCNx8, &(color->red), &(color->green), &(color->blue));
    if(ret == 3){
        return ESP_OK;
    }
    return ESP_FAIL;
}