#ifndef BOARD_H_
#define BOARD_H_

#include "color.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "led_strip.h"
#include "iot_button.h"

typedef void (*board_button_callback_t)(uint8_t button_num);

typedef struct{
    gpio_num_t pin;
    rgb_t color;
    uint32_t number_of_leds;
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

extern rgb_conf_t internal_rgb_conf;

esp_err_t board_set_rgb(rgb_conf_t *conf, rgb_t new_color);

esp_err_t board_start_blinking(rgb_conf_t *conf, uint64_t period_us);

esp_err_t board_stop_blinking(rgb_conf_t *conf);

esp_err_t board_button_task();

esp_err_t board_buttons_release_register_callback(board_button_callback_t callback);

esp_err_t board_init();

#endif
