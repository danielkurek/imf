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
#include "location_common.h"
#include <memory>

// tag for logging
static const char* TAG = "MESH-FMW";

// GPIO pin that the button is connected to
#define CONFIG_BUTTON GPIO_NUM_1


// NVS handle for reading the web_config options
static nvs_handle_t config_nvs;

static std::unique_ptr<SerialCommSrv> serialSrv;

static button_gpio_config_t buttons[] = {
    {
        .gpio_num = GPIO_NUM_0,
        .active_level = 0,
    },
    {
        .gpio_num = GPIO_NUM_1,
        .active_level = 0,
    },
};

static size_t buttons_len = sizeof(buttons) / sizeof(buttons[0]);

// initialize custom logging library for persistent logs
void log_init(){
    logger_init(ESP_LOG_INFO);
    logger_output_to_default();
    logger_init_storage();

    logger_output_to_file("/logs/log.txt", 2000);
}

void serial_comm_change_callback(uint16_t addr, const std::string& field, const std::string& value){
    if(field == "rgb"){
        rgb_t color;
        esp_err_t err = str_to_rgb(value.c_str(), &color);
        if(err != ESP_OK){
            LOGGER_E(TAG, "Invalid RGB value: %s", value.c_str());
            return;
        }
        ESP_LOGI(TAG, "Setting 0x%04" PRIx16 " to RGB R=%d, G=%d, B=%d", addr, color.red, color.green, color.blue);
        ble_mesh_set_rgb(addr, color, false);
    }
    if(field == "loc"){
        location_local_t loc_local {0,0,0,0,0};
        esp_err_t err = simple_str_to_loc(value.c_str(), &loc_local);
        if(err != ESP_OK){
            LOGGER_E(TAG, "Invalid Location value: %s", value.c_str());
            return;
        }
        ESP_LOGI(TAG, "Setting 0x%04" PRIx16 " to North: %" PRId16 " East: %" PRId16, addr, loc_local.local_north, loc_local.local_east);
        ble_mesh_set_loc_local(addr, &loc_local);
    }
    if(field == "onoff"){
        bool onoff;
        if(value == "ON"){
            onoff = true;
        } else if(value == "OFF"){
            onoff = false;
        } else {
            LOGGER_E(TAG, "Invalid OnOff value: %s", value.c_str());
            return;
        }
        ESP_LOGI(TAG, "Setting 0x%04" PRIx16 " to %d", addr, onoff);
        ble_mesh_set_onoff(addr, onoff);
    }
    if(field == "level"){
        int16_t level;
        int ret = sscanf(value.c_str(), "%" SCNd16, &level);
        if(ret != 1){
            LOGGER_E(TAG, "Invalid Level value: %s", value.c_str());
            return;
        }
        ESP_LOGI(TAG, "Setting 0x%04" PRIx16 " to level: %" PRId16, addr, level);
        ble_mesh_set_level(addr, level);
    }
}

void serial_comm_get_callback(uint16_t addr, const std::string& field){
    esp_err_t err;
    LOGGER_I(TAG, "Serial comm Get callback! Addr=0x%04" PRIx16 " Field=%s", addr, field.c_str());
    if(field == "rgb"){
        rgb_t color;
        err = ble_mesh_get_rgb(addr, &color);
        if(err != ESP_OK){
            LOGGER_E(TAG, "Could not get RGB value for 0x%04" PRIx16, addr);
            return;
        }
        size_t buf_len = 6+1;
        char buf[buf_len];
        err = rgb_to_str(color, buf_len, buf);
        if(err != ESP_OK){
            LOGGER_E(TAG, "Could not convert RGB value to str! 0x%04" PRIx16, addr);
            return;
        }
        serialSrv->SetField(addr, field, buf, false);
    }
    if(field == "loc"){
        location_local_t loc_local {0,0,0,0,0};
        err = ble_mesh_get_loc_local(addr, &loc_local);
        if(err != ESP_OK){
            LOGGER_E(TAG, "Could not get local location value for 0x%04" PRIx16, addr);
            return;
        }
        size_t buf_len = 10+1;
        char buf[buf_len];
        err = simple_loc_to_str(&loc_local, buf_len, buf);
        if(err != ESP_OK){
            LOGGER_E(TAG, "Could not convert RGB value to str! 0x%04" PRIx16, addr);
            return;
        }
        serialSrv->SetField(addr, field, buf, false);
    }
    if(field == "onoff"){
        bool onoff;
        err = ble_mesh_get_onoff(addr, &onoff);
        if(err != ESP_OK){
            LOGGER_E(TAG, "Could not get onoff value for 0x%04" PRIx16, addr);
            return;
        }
        if(onoff){
            serialSrv->SetField(addr, field, "ON", false);
        } else{
            serialSrv->SetField(addr, field, "OFF", false);
        }
    }
    if(field == "level"){
        int16_t level;
        err = ble_mesh_get_level(addr, &level);
        if(err != ESP_OK){
            LOGGER_E(TAG, "Could not get level value for 0x%04" PRIx16, addr);
            return;
        }
        
        char buf[7];
        int ret = snprintf(buf, 7, "%" PRId16, level);
        if(ret <= 0){
            LOGGER_E(TAG, "Could not convert level value to str 0x%04" PRIx16, addr);
            return;
        }

        serialSrv->SetField(addr, field, buf, false);
    }
    if(field == "addr"){
        std::string addr_str;
        esp_err_t err = AddrToStr(addr, addr_str);
        if(err != ESP_OK){
            LOGGER_E(TAG, "Could not convert addr to string");
            return;
        }
        serialSrv->SetField(addr, field, addr_str, false);
    }
}

esp_err_t serial_comm_init(){
    uint16_t primary_addr;
    uint8_t  addresses;
    esp_err_t err = ble_mesh_get_addresses(&primary_addr, &addresses);
    if (err != ESP_OK){
        LOGGER_E(TAG, "Could not get addresses of this node");
    }
    serialSrv = std::make_unique<SerialCommSrv>(UART_NUM_1, GPIO_NUM_10, GPIO_NUM_1, primary_addr);
    serialSrv->RegisterChangeCallback(serial_comm_change_callback, serial_comm_get_callback);
    serialSrv->StartTask();

    return ESP_OK;
} 

void button_release_callback(uint8_t button_num){
    if(button_num == 0){
        ble_mesh_set_rgb(0xffff, (rgb_t){120, 255, 0}, false);
    }
    if(button_num == 1){
        rgb_t color;
        uint16_t addr = 0xffff;
        esp_err_t err = ble_mesh_get_rgb(addr, &color);
        if(err != ESP_OK){
            LOGGER_E(TAG, "Could not get RGB value for 0x%04" PRIx16, addr);
        }
        size_t buf_len = 6+1;
        char buf[buf_len];
        err = rgb_to_str(color, buf_len, buf);
        if(err != ESP_OK){
            LOGGER_E(TAG, "Could not convert RGB value to str! 0x%04" PRIx16, addr);
        } else{
            LOGGER_I(TAG, "0x%04" PRIx16" - color=%s", addr, buf);
        }
    }
}

// entry point of program
// first initialize everything then start BLE-mesh
extern "C" void app_main(void)
{
    esp_err_t err;
    
    log_init();

    LOGGER_I(TAG, "Start up");

    // if(web_config()){
    //     return;
    // }

    LOGGER_I(TAG, "Initializing...");

    err = board_init(buttons_len, buttons);
    if(err != ESP_OK){
        LOGGER_E(TAG, "Error occurred during board init! %d", err);
    }

    board_buttons_release_register_callback(button_release_callback);

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

    uint16_t primary_addr;
    uint8_t  addresses;
    err = ble_mesh_get_addresses(&primary_addr, &addresses);
    if (err != ESP_OK){
        LOGGER_E(TAG, "Could not get addresses of this node");
    } else{
        LOGGER_I(TAG, "Addresses of this node are: ");
        for(uint8_t i = 0; i < addresses; i++){
            LOGGER_I(TAG, "    0x%04" PRIx16, primary_addr + i);
        }
    }

    // wait for bluetooth initialization to complete
    // TODO: find better way to handle this
    vTaskDelay(10000 / portTICK_PERIOD_MS);

    err = serial_comm_init();
    if (err){
        LOGGER_E(TAG, "Serial communication init failed (err %d)", err);
    }
}
