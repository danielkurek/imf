/* board.h - Board-specific hooks */

/*
 * SPDX-FileCopyrightText: 2017 Intel Corporation
 * SPDX-FileContributor: 2018-2021 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _BOARD_H_
#define _BOARD_H_

#include "driver/gpio.h"
#include "esp_ble_mesh_defs.h"
#include "led_strip.h"

#if defined(CONFIG_BLE_MESH_ESP32C3_DEV)
#define LED_PIN GPIO_NUM_8
#elif defined(CONFIG_BLE_MESH_ESP32S3_DEV)
#define LED_PIN GPIO_NUM_47
#endif

#define LED_ON 1
#define LED_OFF 0

typedef enum {
    LED_RED = 0,
    LED_GREEN = 1,
    LED_BLUE = 2
} color_t;

struct _led_state {
    led_strip_handle_t *led_strip;
    uint8_t red;
    uint8_t green;
    uint8_t blue;
};

void board_led_operation(color_t color, uint8_t onoff);

void board_init(void);

#endif
