/* board.c - Board-specific hooks */

/*
 * SPDX-FileCopyrightText: 2017 Intel Corporation
 * SPDX-FileContributor: 2018-2021 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>

#include "driver/gpio.h"
#include "esp_log.h"

#include "iot_button.h"
#include "esp_timer.h"
#include "board.h"

#define LED_VALUE(x) ((x == LED_ON) ? 30 : 0)

#define TAG "BOARD"

#define BUTTON_IO_NUM           1
#define BUTTON_ACTIVE_LEVEL     0

extern void example_ble_mesh_send_gen_onoff_set(void);

led_strip_handle_t led_strip;

struct _led_state led_state = {
    LED_ON, LED_OFF, &led_strip, 0, 0, 0
};

esp_timer_handle_t blink_timer;


static void update_led(){
    if(led_state.state == LED_ON){
        led_strip_set_pixel(*(led_state.led_strip), 0, led_state.red, led_state.green, led_state.blue);
        led_strip_refresh(*(led_state.led_strip));
    } else{
        led_strip_clear(*(led_state.led_strip));
    }
}

void board_led_operation(color_t color, uint8_t onoff)
{
    switch(color){
        case LED_RED:
            led_state.red   = LED_VALUE(onoff);
            break;
        case LED_GREEN:
            led_state.green = LED_VALUE(onoff);
            break;
        case LED_BLUE:
            led_state.blue  = LED_VALUE(onoff);
            break;
        default:
            ESP_LOGE(TAG, "LED color not found");
    }

    update_led();

    // ESP_LOGE(TAG, "LED is not found!");
}

void board_led_set_rgb(rgb_t value){
    led_state.red = value.red;
    led_state.green = value.green;
    led_state.blue = value.blue;

    update_led();
}

void blink_cb(){
    if(led_state.blink_state == LED_ON){
        led_state.blink_state = LED_OFF;
        led_strip_clear(*(led_state.led_strip));
    } else{
        led_state.blink_state = LED_ON;
        led_strip_set_pixel(*(led_state.led_strip), 0, LED_VALUE(LED_ON), led_state.green, led_state.blue);
        led_strip_refresh(*(led_state.led_strip));
    }
}

void board_start_blinking(){
    esp_timer_start_periodic(blink_timer, 200 * 1000); // time in microseconds
}

void board_stop_blinking(){
    esp_timer_stop(blink_timer);
    update_led();
}

void board_blink_init(){
    esp_timer_create_args_t args = {
        .callback = blink_cb,
        .name = "BlinkTimer",
    };
    esp_timer_create(&args, &blink_timer);
}

static void board_led_init(void)
{
    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_PIN, // The GPIO that connected to the LED strip's data line
        .max_leds = 1, // The number of LEDs in the strip,
        .led_pixel_format = LED_PIXEL_FORMAT_GRB, // Pixel format of your LED strip
        .led_model = LED_MODEL_SK6812, // LED strip model
        .flags.invert_out = false, // whether to invert the output signal (useful when your hardware has a level inverter)
    };
    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT, // different clock source can lead to different power consumption
        .resolution_hz = 10 * 1000 * 1000, // 10MHz
        .flags.with_dma = false, // whether to enable the DMA feature
    };
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
}

static void button_tap_cb(void* arg)
{
    ESP_LOGI(TAG, "tap cb (%s)", (char *)arg);

    example_ble_mesh_send_gen_onoff_set();
}



static void board_button_init(void)
{
    button_handle_t btn_handle = iot_button_create(BUTTON_IO_NUM, BUTTON_ACTIVE_LEVEL);
    if (btn_handle) {
        iot_button_set_evt_cb(btn_handle, BUTTON_CB_RELEASE, button_tap_cb, "RELEASE");
    }
}

void board_init(void)
{
    board_led_init();
    board_button_init();
    board_blink_init();
}
