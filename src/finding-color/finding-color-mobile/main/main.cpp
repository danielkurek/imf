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

size_t random_number(size_t max){
    uint32_t rnd = esp_random();
    return rnd % max;
}

bool new_color(){
    if(colors.size() == 0){
        LOGGER_E(TAG, "No colors to choose from!");
        return false;
    }
    current_color_index = random_number(colors.size());
    LOGGER_I(TAG, "Choosing index=%d as new color", current_color_index);
    char rgb_str[6+1];
    esp_err_t err = rgb_to_str(colors[current_color_index], 6+1, rgb_str);
    if(err != ESP_OK){
        LOGGER_E(TAG, "Could not convert new color to string! Err: %d", err);
        return false;
    } else{
        LOGGER_I(TAG, "Setting new color to %s", rgb_str);
    }
    if(Device::this_device == nullptr){
        LOGGER_E(TAG, "Local device is not initialized, could not set RGB");
        return false;
    }
    err = Device::this_device->setRgb(colors[current_color_index]);
    return err == ESP_OK;
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

bool check_color(){
    if(closest_device == nullptr){
        return false;
    }
    rgb_t remote_color;
    esp_err_t err = closest_device->getRgb(remote_color);
    if(err != ESP_OK){
        LOGGER_E(TAG, "Could not get RGB of closest device 0x%04" PRIx16, closest_device->ble_mesh_addr);
        return false;
    }
    if(colors_equal(&remote_color, &colors[current_color_index])){
        return new_color();
    }
    return false;
}

void update_cb(TickType_t diff_ms){
    ESP_LOGI(TAG, "Time since last update: %lu", diff_ms);
    TickType_t now_ms = pdTICKS_TO_MS(xTaskGetTickCount());
    if((now_ms - closest_timestamp_ms) >= closest_time_limit){
        // check_color();
        if(check_color()){
            closest_timestamp_ms = now_ms;
        }
    }
}

extern "C" void button_cb(uint8_t button_num){
    ESP_LOGI(TAG, "Button no. %d was pressed", button_num);
    if(button_num == 4){
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
        std::shared_ptr<Device> device = s_imf->getDevice(id);
        if(device != nullptr){
            rgb_t color;
            esp_err_t err = device->getRgb(color);
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
    LOGGER_I(TAG, "Using colors: %s", colors_str.c_str());
    ESP_LOGI(TAG, "IMF register callbacks");
    s_imf->registerCallbacks(button_cb, event_handler, NULL, update_cb, nullptr);

    ESP_LOGI(TAG, "IMF start");
    s_imf->start();
    
    new_color();
}