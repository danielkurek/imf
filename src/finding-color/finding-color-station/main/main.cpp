#include <stdio.h>
#include <inttypes.h>
#include "esp_log.h"
#include "driver/gpio.h"
#include "nvs_flash.h"
#include "esp_check.h"
#include "esp_event.h"
#include "esp_random.h"

#include "interactive-mesh-framework.hpp"
#include "driver/uart.h"

#include <vector>
#include <memory>

#ifdef CONFIG_IDF_TARGET_ESP32C3
#define CONFIG_BUTTON GPIO_NUM_9
#else
#define CONFIG_BUTTON GPIO_NUM_0
#endif

static const char *TAG = "APP";

using namespace imf;

static std::unique_ptr<IMF> s_imf;

static const std::vector<button_gpio_config_t> buttons = {
    {.gpio_num = GPIO_NUM_46, .active_level = 1},
    {.gpio_num = GPIO_NUM_47, .active_level = 1},
    {.gpio_num = GPIO_NUM_48, .active_level = 1},
    {.gpio_num = GPIO_NUM_45, .active_level = 1},
    {.gpio_num = GPIO_NUM_0 , .active_level = 0},
};

typedef struct {
    std::string mac;
    uint16_t ble_addr;
    uint16_t wifi_channel;
    DeviceType type;
} device_conf_t;

extern "C" void event_handler(void* event_handler_arg, esp_event_base_t event_base,
                   int32_t event_id, void* event_data){
    if(event_base == DM_EVENT){
        dm_measurement_data_t *dm_measurement;
        dm_nearest_device_change_t *dm_nearest_dev;
        switch(event_id){
            case DM_MEASUREMENT_DONE:
                dm_measurement = (dm_measurement_data_t*) event_data;
                if(dm_measurement->valid){
                    LOGGER_I(TAG, "DM_MEASUREMENT_DONE, id=%" PRIu32 ", distance_cm=%" PRIu32 " rssi=%" PRId8, dm_measurement->point_id, dm_measurement->measurement.distance_cm, dm_measurement->measurement.rssi);
                } else{
                    LOGGER_I(TAG, "DM_MEASUREMENT_DONE, id=%" PRIu32 ", INVALID", dm_measurement->point_id);
                }
                break;
            case DM_NEAREST_DEVICE_CHANGE:
                dm_nearest_dev = (dm_nearest_device_change_t*) event_data;
                LOGGER_I(TAG, "DM_NEAREST_DEVICE_CHANGE, from: %" PRIx32 " | to: %" PRIx32, dm_nearest_dev->old_point_id, dm_nearest_dev->new_point_id);

                break;
            default:
                LOGGER_I(TAG, "Unknown event of DM, id=%" PRId32, event_id);
        }
    }
}

extern "C" void button_cb(uint8_t button_num){
    ESP_LOGI(TAG, "Button no. %d was pressed", button_num);
    if(button_num == 0){
        if(s_imf){
            LOGGER_I(TAG, "Starting location topology");
            s_imf->startLocalization();
        }
    } else if(button_num == 1){
        int16_t level = 0;
        if(Device::this_device && ESP_OK != Device::this_device->getLevel(level)){
            level = 0;
        }
        if(ESP_OK != Device::setLevelAll(level+1)){
            LOGGER_E(TAG, "Could not set level!");
        }
    }
    else if(button_num == 4){
        int16_t level = 0;
        if(Device::this_device){
            if(ESP_OK != Device::this_device->getLevel(level)){
                level = 0;
            }
            Device::this_device->setLevel(level+1);
        } else{
            LOGGER_E(TAG, "This device is not initialized!");
        }
    }
}

// start configuration mode if conditions are met
bool web_config(){
    gpio_config_t config;

    config.intr_type    = GPIO_INTR_DISABLE;
    config.mode         = GPIO_MODE_INPUT;
    config.pin_bit_mask = (1ull << CONFIG_BUTTON);
    config.pull_down_en = GPIO_PULLDOWN_DISABLE;
    config.pull_up_en   = GPIO_PULLUP_ENABLE;

    gpio_config(&config);

    vTaskDelay(1000 / portTICK_PERIOD_MS);

    if(gpio_get_level(CONFIG_BUTTON) == 0){
        LOGGER_I(TAG, "Booting to web config");
        s_imf->startWebConfig();
        return true;
    }
    return false;
}

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "Startup!");
    esp_err_t err;
    
    s_imf = std::make_unique<IMF>(buttons);
    if(!s_imf){
        ESP_LOGE(TAG, "Could not init IMF!");
        return;
    }

    if(web_config()){
        return;
    }

    ESP_LOGI(TAG, "init IMF");

    ESP_LOGI(TAG, "IMF add devices");
    const device_conf_t devices_confs[] = {
        {"F4:12:FA:EA:0B:91", 0x0002, 1, DeviceType::Station},
        {"F4:12:FA:EA:0B:85", 0x0005, 1, DeviceType::Station},
        {"F4:12:FA:EA:0B:65", 0x0008, 1, DeviceType::Station},
        {"F4:12:FA:EA:0B:75", 0x000B, 1, DeviceType::Station},
        {"F4:12:FA:EA:0B:A5", 0x0011, 1, DeviceType::Mobile },
        {"F4:12:FA:EA:0B:81", 0x0014, 1, DeviceType::Station},
        {"F4:12:FA:EA:0B:F9", 0x0017, 1, DeviceType::Station},
        {"F4:12:FA:E8:D2:6D", 0x001A, 1, DeviceType::Station},
        {"F4:12:FA:EA:0B:89", 0x001D, 1, DeviceType::Station},
        {"F4:12:FA:E8:D3:6D", 0x0020, 1, DeviceType::Mobile },
    };
    const size_t devices_len = sizeof(devices_confs) / sizeof(devices_confs[0]);
    for(size_t i = 0; i < devices_len; i++){
        const device_conf_t *d_conf = &devices_confs[i];
        uint32_t id = s_imf->addDevice(d_conf->type, d_conf->mac, d_conf->wifi_channel, d_conf->ble_addr);
    }
    ESP_LOGI(TAG, "IMF register callbacks");
    s_imf->registerCallbacks(button_cb, event_handler, NULL, nullptr, nullptr);

    ESP_LOGI(TAG, "IMF start");
    s_imf->start();
}