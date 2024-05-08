/**
 * @file color.h
 * @author Daniel Kurek (daniel.kurek.dev@gmail.com)
 * @brief HSL and RGB definitions and helper functions
 * @version 0.1
 * @date 2023-08-19
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#ifndef COLOR_H_
#define COLOR_H_

#include <inttypes.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C"{
#endif

typedef struct{
    uint8_t red;    /**< red channel */
    uint8_t green;  /**< gren channel */
    uint8_t blue;   /**< blue channel */
} rgb_t;

typedef struct{
    uint16_t lightness; /**< color lightness channel */
    uint16_t hue;       /**< hue of the color */
    uint16_t saturation; /**< color saturation */
} hsl_t;

/**
 * @brief length of RGB string representation
 */
#define RGB_STR_LEN 6+1

/**
 * @brief converts RGB to hexadecimal string representation (format 'xxxxxx')
 * 
 * @param color input color
 * @param buf_len length of buf array
 * @param buf output buffer
 * @return esp_err_t returns ESP_OK if conversion is successful, otherwise ESP_FAIL
 */
esp_err_t rgb_to_str(rgb_t color, size_t buf_len, char buf[]);

/**
 * @brief parses hexadecimal RGB string (format 'xxxxxx')
 * 
 * @param buf input char array (or c_str string)
 * @param color parsed output
 * @return esp_err_t returns ESP_OK if conversion is successful, otherwise ESP_FAIL
 */
esp_err_t str_to_rgb(const char *buf, rgb_t *color);

#ifdef __cplusplus
}
#endif

#endif