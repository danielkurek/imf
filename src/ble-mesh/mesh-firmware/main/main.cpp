/* main.c - Application main entry point */

/*
 * SPDX-FileCopyrightText: 2017 Intel Corporation
 * SPDX-FileContributor: 2018-2021 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */


// General imports

#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include "esp_log.h"
#include "nvs_flash.h"

// Additional functions for this project

#include "board.h"

#include "ble_mesh.h"

#include "driver/gpio.h"
#include "web_config.h"

#include "logger.h"

#include "serial_comm_server.hpp"

// tag for logging
static const char* TAG = "MESH-FMW";

// GPIO pin that the button is connected to
#define CONFIG_BUTTON GPIO_NUM_1

// custom web_config options
static config_option_t options[] = {
    {
        "rgb/client/addr", // key
        NVS_TYPE_U16, // type
        true, //value_to_log
    }
};

static const size_t options_len = sizeof(options) / sizeof(options[0]);

// NVS handle for reading the web_config options
static nvs_handle_t config_nvs;

static SerialCommSrv serialSrv;

// start configuration mode if conditions are met
bool web_config(){
    gpio_config_t config;

    config.pin_bit_mask = 1ull << CONFIG_BUTTON;
    config.mode = GPIO_MODE_INPUT;
    config.pull_up_en = 1;

    gpio_config(&config);

    vTaskDelay(1000 / portTICK_PERIOD_MS);

    if(gpio_get_level(CONFIG_BUTTON) == 0){
        LOGGER_I(TAG, "Booting to web config");
        web_config_set_custom_options(options_len, options);
        web_config_start();
        return true;
    }
    return false;
}

// initialize custom logging library for persistent logs
void log_init(){
    logger_init(ESP_LOG_INFO);
    logger_output_to_default();
    logger_init_storage();

    logger_output_to_file("/logs/log.txt", 2000);
}

esp_err_t serial_comm_init(){
    serialSrv = {UART_NUM_1, GPIO_NUM_16, GPIO_NUM_17};
    serialSrv.StartTask();

    return ESP_OK;
} 

void button_release_callback(uint8_t button_num){
    if(button_num == 0){
        ble_mesh_set_rgb(0xffff, (rgb_t){120, 255, 0}, false);
    }
    if(button_num == 1){
        ble_mesh_get_rgb(0xffff);
    }
}

// entry point of program
// first initialize everything then start BLE-mesh
void app_main(void)
{
    esp_err_t err;

    LOGGER_I(TAG, "Start up");

    if(web_config()){
        return;
    }

    LOGGER_I(TAG, "Initializing...");

    board_init();

    board_buttons_release_register_callback(button_release_callback);

    log_init();

    LOGGER_V(TAG, "init NVS");
    err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    // open nvs namespace that stores web config values
    err = nvs_open("config", NVS_READONLY, &config_nvs);
    if (err) {
        ESP_LOGI(TAG, "Could not open NVS for config");
    }

    /* Initialize the Bluetooth Mesh */
    LOGGER_V(TAG, "mesh init");
    err = ble_mesh_init();
    if (err) {
        LOGGER_E(TAG, "Bluetooth mesh init failed (err %d)", err);
    }

    err = serial_comm_init();
    if (err){
        LOGGER_E(TAG, "Serial communication init failed (err %d)", err);
    }
}
