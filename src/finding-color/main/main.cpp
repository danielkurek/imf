#include <stdio.h>
#include <inttypes.h>
#include "esp_log.h"
#include "driver/gpio.h"
#include "nvs_flash.h"
#include "esp_check.h"
#include "esp_event.h"

#include "interactive-mesh-framework.hpp"
#include "driver/uart.h"

#ifdef CONFIG_IDF_TARGET_ESP32C3
#define CONFIG_BUTTON GPIO_NUM_9
#else
#define CONFIG_BUTTON GPIO_NUM_0
#endif

static const char *TAG = "APP";

using namespace imf;

void update_cb(TickType_t diff_ms){
    ESP_LOGI(TAG, "Time since last update: %lu", diff_ms);
}

extern "C" void button_cb(uint8_t button_num){
    ESP_LOGI(TAG, "Button no. %d was pressed", button_num);
}

extern "C" void event_handler(void* event_handler_arg, esp_event_base_t event_base,
                   int32_t event_id, void* event_data){
    if(event_base == DM_EVENT){
        dm_measurement_data_t *dm_measurement;
        dm_event_data_t *dm_event_data;
        uint32_t *device_id;
        switch(event_id){
            case DM_MEASUREMENT_DONE:
                dm_measurement = (dm_measurement_data_t*) event_data;
                if(dm_measurement->valid)
                    ESP_LOGI(TAG, "DM_MEASUREMENT_DONE, id=%" PRIu32 ", distance_cm=%" PRIu32, dm_measurement->point_id, dm_measurement->distance_cm);
                else
                    ESP_LOGI(TAG, "DM_MEASUREMENT_DONE, id=%" PRIu32 ", INVALID", dm_measurement->point_id);
                break;
            case DM_NEAREST_DEVICE_ENTER:
                dm_event_data = (dm_event_data_t*) event_data;
                if(dm_event_data->point_id == UINT32_MAX){
                    ESP_LOGI(TAG, "DM_NEAREST_DEVICE_ENTER, NONE");
                    break;
                }
                ESP_LOGI(TAG, "DM_NEAREST_DEVICE_ENTER, %" PRIu32, dm_event_data->point_id);
                break;
            case DM_NEAREST_DEVICE_LEAVE:
                dm_event_data = (dm_event_data_t*) event_data;
                if(dm_event_data->point_id == UINT32_MAX){
                    ESP_LOGI(TAG, "DM_NEAREST_DEVICE_LEAVE, NONE");
                    break;
                }
                ESP_LOGI(TAG, "DM_NEAREST_DEVICE_LEAVE, %" PRIu32, dm_event_data->point_id);
                break;
            default:
                ESP_LOGE(TAG, "Unknown event of DM, id=%" PRId32, event_id);
        }
    }
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

    ESP_LOGI(TAG, "init IMF");
    IMF imf;
    
    ESP_LOGI(TAG, "IMF add devices");
    imf.addDevice(DeviceType::Station, "34:b4:72:6a:77:c1", 1, 0xc000);
    
    ESP_LOGI(TAG, "IMF register callbacks");
    imf.registerCallbacks(button_cb, event_handler, NULL, update_cb);

    ESP_LOGI(TAG, "IMF start");
    imf.start();
}