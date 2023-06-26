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
#include "board.h"
#include "led_strip.h"

#define LED_VALUE(x) ((x == LED_ON) ? 128 : 0)

#define TAG "BOARD"

led_strip_handle_t led_strip;

struct _led_state led_state = {
    &led_strip, 0, 0, 0
};

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
    led_strip_set_pixel(*led_state.led_strip, 0, led_state.red, led_state.green, led_state.blue);
    led_strip_refresh(*led_state.led_strip);

    // ESP_LOGE(TAG, "LED is not found!");
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

void board_init(void)
{
    board_led_init();
}