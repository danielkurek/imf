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
static std::shared_ptr<Device> closest_device;
static TickType_t closest_timestamp_ms;

static std::vector<rgb_t> colors;
static size_t current_color_index;

constexpr uint32_t closest_time_limit = 10000;

constexpr int16_t color_cmp_threshold = 10;

typedef struct {
    std::string mac;
    uint16_t ble_addr;
    uint16_t wifi_channel;
} device_conf_t;

size_t random_number(size_t max){
    uint32_t rnd = esp_random();
    return rnd % max;
}

void new_color(){
    if(colors.size() == 0){
        LOGGER_E(TAG, "No colors to choose from!");
        return;
    }
    current_color_index = random_number(colors.size());
    LOGGER_I(TAG, "Choosing index=%d as new color", current_color_index);
    char rgb_str[6+1];
    esp_err_t err = rgb_to_str(colors[current_color_index], 6+1, rgb_str);
    if(err != ESP_OK){
        LOGGER_E(TAG, "Could not convert new color to string! Err: %d", err);
    } else{
        LOGGER_I(TAG, "Setting new color to %s", rgb_str);
    }
    if(Device::this_device == nullptr){
        LOGGER_E(TAG, "Local device is not initialized, could not set RGB");
        return;
    }
    Device::this_device->setRgb(colors[current_color_index]);
}

bool colors_equal(const rgb_t *c1, const rgb_t *c2){
    if(abs((int16_t) c1->red - (int16_t) c2->red) > color_cmp_threshold){
        return false;
    }
    if(abs((int16_t) c1->green - (int16_t) c2->green) > color_cmp_threshold){
        return false;
    }
    if(abs((int16_t) c1->blue - (int16_t) c2->blue) > color_cmp_threshold){
        return false;
    }
    return true;
}

void check_color(){
    if(closest_device == nullptr){
        return;
    }
    rgb_t remote_color;
    esp_err_t err = closest_device->getRgb(&remote_color);
    if(err != ESP_OK){
        LOGGER_E(TAG, "Could not get RGB of closest device 0x%04" PRIx16, closest_device->ble_mesh_addr);
        return;
    }
    if(colors_equal(&remote_color, &colors[current_color_index])){
        new_color();
    }
}

void update_cb(TickType_t diff_ms){
    ESP_LOGI(TAG, "Time since last update: %lu", diff_ms);
    TickType_t now_ms = pdTICKS_TO_MS(xTaskGetTickCount());
    if((now_ms - closest_timestamp_ms) >= closest_time_limit){
        check_color();
    }
}

extern "C" void button_cb(uint8_t button_num){
    ESP_LOGI(TAG, "Button no. %d was pressed", button_num);
}

extern "C" void event_handler(void* event_handler_arg, esp_event_base_t event_base,
                   int32_t event_id, void* event_data){
    if(event_base == DM_EVENT){
        dm_measurement_data_t *dm_measurement;
        dm_nearest_device_change_t *dm_nearest_dev;
        switch(event_id){
            case DM_MEASUREMENT_DONE:
                dm_measurement = (dm_measurement_data_t*) event_data;
                if(dm_measurement->valid)
                    ESP_LOGI(TAG, "DM_MEASUREMENT_DONE, id=%" PRIu32 ", distance_cm=%" PRIu32, dm_measurement->point_id, dm_measurement->distance_cm);
                else
                    ESP_LOGI(TAG, "DM_MEASUREMENT_DONE, id=%" PRIu32 ", INVALID", dm_measurement->point_id);
                break;
            case DM_NEAREST_DEVICE_CHANGE:
                dm_nearest_dev = (dm_nearest_device_change_t*) event_data;
                ESP_LOGI(TAG, "DM_NEAREST_DEVICE_CHANGE, from: %" PRIx32 " | to: %" PRIx32, dm_nearest_dev->old_point_id, dm_nearest_dev->new_point_id);

                // check if the event is newer than what is saved
                // does not hold when timestamp overflows 
                // but it is ok, since it happens after more than 1000 hours (way more than expected runtime)
                if(dm_nearest_dev->timestamp_ms >= closest_timestamp_ms){
                    if(dm_nearest_dev->new_point_id == UINT32_MAX){
                        closest_device = nullptr;
                    } else{
                        closest_device = s_imf->getDevice(dm_nearest_dev->new_point_id);
                    }
                    closest_timestamp_ms = dm_nearest_dev->timestamp_ms;
                }
                break;
            default:
                ESP_LOGE(TAG, "Unknown event of DM, id=%" PRId32, event_id);
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

    s_imf = std::make_unique<IMF>();
    if(!s_imf){
        ESP_LOGE(TAG, "Could not init IMF!");
        return;
    }

    if(web_config()){
        return;
    }

    ESP_ERROR_CHECK(esp_event_loop_create_default());

    ESP_LOGI(TAG, "init IMF");

    ESP_LOGI(TAG, "IMF add devices");
    const device_conf_t devices_confs[] = {
        {"34:b4:72:69:cb:9d", 0xc000, 1},
        {"34:b4:72:69:e6:13", 0xc001, 1},
        {"34:b4:72:6a:76:ed", 0xc002, 1},
        {"34:b4:72:6a:77:c1", 0xc003, 1},
        {"34:b4:72:6a:84:59", 0xc004, 1},
    };
    const size_t devices_len = sizeof(devices_confs) / sizeof(devices_confs[0]);
    for(size_t i = 0; i < devices_len; i++){
        const device_conf_t *d_conf = &devices_confs[i];
        uint32_t id = s_imf->addDevice(DeviceType::Station, d_conf->mac, d_conf->wifi_channel, d_conf->ble_addr);
        std::shared_ptr<Device> device = s_imf->getDevice(id);
        if(device != nullptr){
            rgb_t color;
            esp_err_t err = device->getRgb(&color);
            if(err == ESP_OK){
                bool add_color = true;
                for(auto &&_color : colors){
                    if(_color.red == color.red && _color.green == color.green && _color.blue == color.blue){
                        add_color = false;
                        break;
                    }
                }
                if(add_color){
                    colors.push_back(color);
                }
            }
        }
    }
    std::string colors_str;
    bool first = true;
    for(auto && color : colors){
        char rgb_str[RGB_STR_LEN];
        rgb_to_str(color, RGB_STR_LEN, rgb_str);
        if(first){
            colors_str += std::string(rgb_str);
            first = false;
        } else
            colors_str += "," + std::string(rgb_str);
    }
    ESP_LOGI(TAG, "Using colors: %s", colors_str.c_str());
    ESP_LOGI(TAG, "IMF register callbacks");
    s_imf->registerCallbacks(button_cb, event_handler, NULL, update_cb);

    ESP_LOGI(TAG, "IMF start");
    s_imf->start();
    
    new_color();
}