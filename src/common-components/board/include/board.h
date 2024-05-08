/**
 * @file board.h
 * @author Daniel Kurek (daniel.kurek.dev@gmail.com)
 * @brief Library for controlling internal hardware
 * @version 0.1
 * @date 2023-11-09
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#ifndef BOARD_H_
#define BOARD_H_

#include "color/color.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "led_strip.h"
#include "iot_button.h"

#ifdef __cplusplus
extern "C"{
#endif

typedef void (*board_button_callback_t)(uint8_t button_num);

typedef struct{
    gpio_num_t pin;
    rgb_t color;
    uint16_t number_of_leds;
    led_strip_handle_t led_strip;
    esp_timer_handle_t timer;
    bool on_off;
} rgb_conf_t;

typedef struct{
    gpio_num_t pin;
    uint8_t button_num;
    board_button_callback_t callback;
    button_handle_t handle;
} button_conf_t;

extern rgb_t NONE;
extern rgb_t RED;
extern rgb_t GREEN;
extern rgb_t BLUE;
extern rgb_t WHITE;

/**
 * @brief Internal addressable RGB LED configuration
 * 
 */
extern rgb_conf_t internal_rgb_conf;

/**
 * @brief Initialize addressable RGB LED (like WS2812 or SK6812)
 * 
 * @param conf LED configuration with internal state
 * @return esp_err_t return ESP_OK if initialization succeeds
 */
esp_err_t board_led_init(rgb_conf_t *conf);

/**
 * @brief Set RGB color of LED
 * 
 * @param conf configuration of LED (must be initialized)
 * @param new_color new RGB color
 * @return esp_err_t returns ESP_OK on success
 */
esp_err_t board_set_rgb(rgb_conf_t *conf, rgb_t new_color);

/**
 * @brief Turn LED on/off while keeping color
 * 
 * @param conf configuration of LED (must be initialized)
 * @param onoff on/off state
 * @return esp_err_t returns ESP_OK on success
 */
esp_err_t board_set_onoff(rgb_conf_t *conf, bool onoff);

/**
 * @brief Control power to external board
 * 
 * @param onoff on/off state
 * @return esp_err_t returns ESP_OK on success
 */
esp_err_t board_set_ext_pwr(bool onoff);

/**
 * @brief Start blinking LED with current color
 * 
 * @param conf configuration of LED (must be initialized)
 * @param period_us blinking period (in us)
 * @return esp_err_t returns ESP_OK on success
 */
esp_err_t board_start_blinking(rgb_conf_t *conf, uint64_t period_us);

/**
 * @brief Stop LED blinking
 * 
 * @param conf configuration of LED (must be initialized)
 * @return esp_err_t returns ESP_OK on success
 */
esp_err_t board_stop_blinking(rgb_conf_t *conf);

/**
 * @brief Register callback for button presses (must be called only after board_init())
 * 
 * @param callback callback function
 */
void board_buttons_release_register_callback(board_button_callback_t callback);

/**
 * @brief Initialize board hardware and all of its functions
 * 
 * @param buttons_len length of @p buttons array
 * @param buttons array of button configuration
 * @return esp_err_t returns ESP_OK on success
 */
esp_err_t board_init(size_t buttons_len, const button_gpio_config_t *buttons);

#ifdef __cplusplus
}
#endif

#endif
