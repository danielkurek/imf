/**
 * @file main.cpp
 * @author Daniel Kurek (daniel.kurek.dev@gmail.com)
 * @brief Firmware for Bluetooth mesh communication device (secondary module)
 * @version 0.1
 * @date 2023-06-26
 * 
 * @copyright Copyright (c) 2023
 * 
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
#define SERIAL_TX_GPIO CONFIG_SERIAL_TX_GPIO
#define SERIAL_RX_GPIO CONFIG_SERIAL_RX_GPIO

using namespace com;


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
    if(logger_init(ESP_LOG_INFO)){
        logger_output_to_default(true);
        // logger_init_storage();

        // logger_output_to_file("/logs/log.txt", 2000);
    }
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
        err = ble_mesh_get_rgb(addr);
        if(err != ESP_OK){
            LOGGER_E(TAG, "Could not get RGB value for 0x%04" PRIx16, addr);
            return;
        }
    }
    if(field == "loc"){
        err = ble_mesh_get_loc_local(addr);
        if(err != ESP_OK){
            LOGGER_E(TAG, "Could not get local location value for 0x%04" PRIx16, addr);
            return;
        }
    }
    if(field == "onoff"){
        err = ble_mesh_get_onoff(addr);
        if(err != ESP_OK){
            LOGGER_E(TAG, "Could not get onoff value for 0x%04" PRIx16, addr);
            return;
        }
    }
    if(field == "level"){
        err = ble_mesh_get_level(addr);
        if(err != ESP_OK){
            LOGGER_E(TAG, "Could not get level value for 0x%04" PRIx16, addr);
            return;
        }
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
    serialSrv = std::make_unique<SerialCommSrv>(UART_NUM_1, SERIAL_TX_GPIO, SERIAL_RX_GPIO, primary_addr);
    serialSrv->RegisterChangeCallback(serial_comm_change_callback, serial_comm_get_callback);
    serialSrv->startReadTask();

    return ESP_OK;
} 

void button_release_callback(uint8_t button_num){
    return;
}

extern "C" void value_change_cb(ble_mesh_value_change_data_t event_data){
    esp_err_t err;
    LOGGER_I(TAG, "BLE-Mesh value change callback! Type=%d Addr=0x%04" PRIx16, event_data.type, event_data.addr);
    if(event_data.type == LOC_LOCAL_CHANGE){
        char buf[SIMPLE_LOC_STR_LEN];
        err = simple_loc_to_str(&event_data.loc_local, SIMPLE_LOC_STR_LEN, buf);
        if(err != ESP_OK){
            LOGGER_E(TAG, "Could not convert RGB value to str! 0x%04" PRIx16, event_data.addr);
            return;
        }
        serialSrv->SetField(event_data.addr, "loc", buf, false);
    }
    if(event_data.type == RGB_CHANGE){
        size_t buf_len = 6+1;
        char buf[buf_len];
        err = rgb_to_str(event_data.rgb, buf_len, buf);
        if(err != ESP_OK){
            LOGGER_E(TAG, "Could not convert RGB value to str! 0x%04" PRIx16, event_data.addr);
            return;
        }
        serialSrv->SetField(event_data.addr, "rgb", buf, false);
    }
    if(event_data.type == ONOFF_CHANGE){
        if(event_data.onoff){
            serialSrv->SetField(event_data.addr, "onoff", "ON", false);
        } else{
            serialSrv->SetField(event_data.addr, "onoff", "OFF", false);
        }
    }
    if(event_data.type == LEVEL_CHANGE){
        char buf[7];
        int ret = snprintf(buf, 7, "%" PRId16, event_data.level);
        if(ret <= 0){
            LOGGER_E(TAG, "Could not convert level value to str 0x%04" PRIx16, event_data.addr);
            return;
        }

        serialSrv->SetField(event_data.addr, "level", buf, false);
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

    err = serial_comm_init();
    if (err){
        LOGGER_E(TAG, "Serial communication init failed (err %d)", err);
    }

    ble_mesh_register_cb(value_change_cb);
}
