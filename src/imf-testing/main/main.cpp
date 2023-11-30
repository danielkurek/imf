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

void button_cb(uint8_t button_num){
    ESP_LOGI(TAG, "Button no. %d was pressed", button_num);
}

void event_handler(void* event_handler_arg, esp_event_base_t event_base,
                   int32_t event_id, void* event_data){
    if(event_base == DM_EVENT){
        dm_measurement_data_t *dm_data;
        char *mac_str;
        switch(event_id){
            case DM_MEASUREMENT_DONE:
                dm_data = (dm_measurement_data_t*) event_data;
                if(dm_data->valid)
                    ESP_LOGI(TAG, "DM_MEASUREMENT_DONE, id=%" PRIu32 ", distance_cm=%" PRIu32, dm_data->point_id, dm_data->distance_cm);
                else
                    ESP_LOGI(TAG, "DM_MEASUREMENT_DONE, id=%" PRIu32 ", INVALID", dm_data->point_id);
                break;
            case DM_NEAREST_DEVICE_ENTER:
                if(event_data == NULL){
                    ESP_LOGI(TAG, "DM_NEAREST_DEVICE_ENTER, NONE");
                    break;
                }
                mac_str = (char *) event_data;
                ESP_LOGI(TAG, "DM_NEAREST_DEVICE_ENTER, %s", mac_str);
                break;
            case DM_NEAREST_DEVICE_LEAVE:
                if(event_data == NULL){
                    ESP_LOGI(TAG, "DM_NEAREST_DEVICE_LEAVE, NONE");
                    break;
                }
                mac_str = (char *) event_data;
                ESP_LOGI(TAG, "DM_NEAREST_DEVICE_LEAVE, %s", mac_str);
                break;
            default:
                ESP_LOGE(TAG, "Unknown event of DM, id=%" PRId32, event_id);
        }
    }
}

void testapp(void* param){
    ESP_LOGI(TAG, "Testapp start");
    IMF imf;
    uint8_t mac[6] = {0x34,0xb4,0x72,0x6a,0x77,0xc1};
    
    ESP_LOGI(TAG, "imf add device");
    imf.addDevice(DeviceType::Station, mac, 1, 0xffff);
    
    ESP_LOGI(TAG, "imf register callbacks");
    imf.registerCallbacks(button_cb, event_handler, NULL);

    ESP_LOGI(TAG, "imf start");
    imf.start();

    rgb_t seq[] {{0,0,0},{10,0,0},{0,10,0},{0,0,10}};
    size_t seq_len = sizeof(seq) / sizeof(seq[0]);
    size_t i = 0;
    ESP_LOGI(TAG, "loop start");
    while(1){
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        ESP_LOGI(TAG, "set %d", i);
        imf.devices[0]->setRgb(seq[i]);
        i++;
        if(i >= seq_len){
            i = 0;
        }
    }

    vTaskDelete(NULL);
}

void startup(void* param){
    gpio_config_t config {};
    config.pin_bit_mask = 1ull << CONFIG_BUTTON;
    config.mode = GPIO_MODE_INPUT;
    config.pull_up_en = GPIO_PULLUP_ENABLE;

    gpio_config(&config);

    vTaskDelay(1000 / portTICK_PERIOD_MS);

    if(gpio_get_level(CONFIG_BUTTON) == 0){
        // server(param);
    } else{
        // client(param);
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

    xTaskCreate(testapp, "imf-test", 1024*24, NULL, configMAX_PRIORITIES, NULL);
}