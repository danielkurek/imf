#ifndef BOARD_H_
#define BOARD_H_

#include "color.h"
#include "driver/gpio.h"
#include "led_strip.h"
#include "iot_button.h"

typedef void (*board_button_callback_t)(uint8_t button_num);

typedef struct{
    gpio_num_t pin;
    rgb_t color;
    uint32_t number_of_leds;
    led_strip_handle_t led_strip;
} rgb_conf_t;

typedef struct{
    gpio_num_t pin;
    uint8_t button_num;
    board_button_callback_t callback;
    button_handle_t handle;
} button_conf_t;


rgb_conf_t internal_rgb_conf;

esp_err_t board_set_rgb(rgb_conf_t *conf, rgb_t new_color);

esp_err_t board_button_task();

esp_err_t board_register_buttons_release_callback(board_button_callback_t callback);

esp_err_t board_init();

#endif
