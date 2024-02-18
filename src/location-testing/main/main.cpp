#include <stdio.h>
#include <inttypes.h>
#include "esp_log.h"
#include "driver/gpio.h"
#include "nvs_flash.h"
#include "esp_check.h"
#include "esp_event.h"

#include "interactive-mesh-framework.hpp"
#include "location_common.h"
#include "driver/uart.h"

#include <vector>

#ifdef CONFIG_IDF_TARGET_ESP32C3
#define CONFIG_BUTTON GPIO_NUM_9
#else
#define CONFIG_BUTTON GPIO_NUM_0
#endif

static const char *TAG = "APP";

using namespace imf;

void location_testing(void *param){
    std::vector<std::shared_ptr<Device>> devices;
    Device::this_device = std::make_shared<Device>(0, DeviceType::Station, "00:00:00:00:00:00", 1, 0x0001, (rgb_t){0,0,0}, (location_local_t){0,0,0,0,0}, 0, 0); // J
    devices.emplace_back(std::make_shared<Device>(1, DeviceType::Station, "00:00:00:00:00:01", 1, 0x0004, (rgb_t){0,0,0}, (location_local_t){80,-50,0,0,0}, 0, 3)); // B (distance=3.16)
    devices.emplace_back(std::make_shared<Device>(2, DeviceType::Station, "00:00:00:00:00:02", 1, 0x0007, (rgb_t){0,0,0}, (location_local_t){90,-10,0,0,0}, 0,  4)); // C (distance=3.60)
    devices.emplace_back(std::make_shared<Device>(3, DeviceType::Station, "00:00:00:00:00:03", 1, 0x000a, (rgb_t){0,0,0}, (location_local_t){120,-10,0,0,0}, 0, 3)); // K (distance=3.16)
    devices.emplace_back(std::make_shared<Device>(4, DeviceType::Station, "00:00:00:00:00:04", 1, 0x000d, (rgb_t){0,0,0}, (location_local_t){140,-30,0,0,0}, 0, 3)); // L (distance=3.16)
    devices.emplace_back(std::make_shared<Device>(5, DeviceType::Station, "00:00:00:00:00:05", 1, 0x0010, (rgb_t){0,0,0}, (location_local_t){110,-70,0,0,0}, 0, 3)); // M (distance=3.00)
    // TODO: add other devices with unknown distance
    LocationTopology topology {"major", Device::this_device, devices, 2};
    topology.start();
    while(1){
        vTaskDelay(10000 / portTICK_PERIOD_MS);
        location_local_t location {0,0,0,0,0};
        esp_err_t err;
        err = Device::this_device->getLocation(location);
        if(err != ESP_OK){
            ESP_LOGE(TAG, "could not get location");
            continue;
        }
        char buf[SIMPLE_LOC_STR_LEN];
        if(ESP_OK == simple_loc_to_str(&location, SIMPLE_LOC_STR_LEN, buf)){
            ESP_LOGI(TAG, "location: %s", buf);
        } else{
            ESP_LOGE(TAG, "could not convert location to str");
        }
    }
    vTaskDelete(NULL);
}

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "Startup!");
    esp_err_t err;
    
    ESP_LOGI(TAG, "NVS flash init...");
    err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    ESP_ERROR_CHECK(esp_event_loop_create_default());

    xTaskCreate(location_testing, "location-test", 1024*24, NULL, configMAX_PRIORITIES, NULL);
}