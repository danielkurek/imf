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

void update_cb(TickType_t diff){
    ESP_LOGI(TAG, "Ticks from last update: %" PRIu32, diff);
}

void button_cb(uint8_t button_num){
    ESP_LOGI(TAG, "Button no. %d was pressed", button_num);
}

void event_handler(void* event_handler_arg, esp_event_base_t event_base,
                   int32_t event_id, void* event_data){
    if(event_base == DM_EVENT){
        dm_measurement_data_t *dm_measurement;
        dm_nearest_device_change_t *dm_nearest_dev;
        switch(event_id){
            case DM_MEASUREMENT_DONE:
                dm_measurement = (dm_measurement_data_t*) event_data;
                if(dm_measurement->valid)
                    ESP_LOGI(TAG, "DM_MEASUREMENT_DONE, id=%" PRIu32 ", distance_cm=%" PRIu32 " rssi=%" PRId8, dm_measurement->point_id, dm_measurement->measurement.distance_cm, dm_measurement->measurement.rssi);
                else
                    ESP_LOGI(TAG, "DM_MEASUREMENT_DONE, id=%" PRIu32 ", INVALID", dm_measurement->point_id);
                break;
            case DM_NEAREST_DEVICE_CHANGE:
                dm_nearest_dev = (dm_nearest_device_change_t*) event_data;
                ESP_LOGI(TAG, "DM_NEAREST_DEVICE_CHANGE, from: %" PRIx32 " | to: %" PRIx32, dm_nearest_dev->old_point_id, dm_nearest_dev->new_point_id);
            default:
                ESP_LOGE(TAG, "Unknown event of DM, id=%" PRId32, event_id);
        }
    }
}

void testapp(void* param){
    ESP_LOGI(TAG, "Testapp start");
    static const std::vector<button_gpio_config_t> buttons {};
    IMF imf{buttons};
    
    ESP_LOGI(TAG, "imf add device");
    uint32_t id = imf.addDevice(DeviceType::Station, "34:b4:72:6a:77:c1", 1, 0xffff);

    std::shared_ptr<Device> device = imf.getDevice(id);
    
    ESP_LOGI(TAG, "imf register callbacks");
    imf.registerCallbacks(button_cb, event_handler, NULL, update_cb);

    ESP_LOGI(TAG, "imf start");
    imf.start();

    rgb_t seq[] {{0,0,0},{10,0,0},{0,10,0},{0,0,10}};
    size_t seq_len = sizeof(seq) / sizeof(seq[0]);
    size_t i = 0;
    ESP_LOGI(TAG, "loop start");
    while(1){
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        ESP_LOGI(TAG, "set %d", i);
        device->setRgb(seq[i]);
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