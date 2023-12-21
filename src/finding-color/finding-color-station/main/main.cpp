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

typedef struct {
    std::string mac;
    uint16_t ble_addr;
    uint16_t wifi_channel;
} device_conf_t;

size_t random_number(size_t max){
    uint32_t rnd = esp_random();
    return rnd % max;
}

extern "C" void button_cb(uint8_t button_num){
    ESP_LOGI(TAG, "Button no. %d was pressed", button_num);
}

// start configuration mode if conditions are met
bool web_config(){
    gpio_config_t config;

    config.pin_bit_mask = 1ull << CONFIG_BUTTON;
    config.mode = GPIO_MODE_INPUT;
    config.pull_up_en = GPIO_PULLUP_ENABLE;

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
        uint32_t id = s_imf->addDevice(DeviceType::Mobile, d_conf->mac, d_conf->wifi_channel, d_conf->ble_addr);
    }
    ESP_LOGI(TAG, "IMF register callbacks");
    s_imf->registerCallbacks(button_cb, nullptr, NULL, nullptr);

    ESP_LOGI(TAG, "IMF start");
    s_imf->start();
}