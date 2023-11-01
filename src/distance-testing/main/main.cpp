#include <stdio.h>
#include <inttypes.h>
#include "distance_meter.hpp"
#include "wifi_connect.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "nvs_flash.h"
#include "esp_check.h"
#include "esp_event.h"

#ifdef CONFIG_IDF_TARGET_ESP32C3
#define CONFIG_BUTTON GPIO_NUM_9
#else
#define CONFIG_BUTTON GPIO_NUM_0
#endif

static const char *TAG = "APP";

typedef struct{
    uint8_t bssid[6];
    uint8_t channel;
} dm_point_info;

void server(){
    wifi_init_ap_simple("TestingWifi", "SuPerdUpeRStrongPaSswoRdTHatNoOneWillguess", 1);

    vTaskDelete(NULL);
}

void client(){
    esp_err_t err;

    dm_point_info points_info[] = {
        {{0x34,0xb4,0x72,0x69,0xcb,0x9d}, 1},
        {{0x34,0xb4,0x72,0x69,0xe6,0x13}, 1},
        {{0x34,0xb4,0x72,0x6a,0x76,0xed}, 1},
        {{0x34,0xb4,0x72,0x6a,0x77,0xc1}, 1},
        {{0x34,0xb4,0x72,0x6a,0x84,0x59}, 1}
    };

    size_t points_info_len = sizeof(points_info) / sizeof(points_info[0]);
    
    err = DistancePoint::initDistanceMeasurement();
    if(err != ESP_OK){
        ESP_LOGI(TAG, "Error initiliazing DM, err: %s", esp_err_to_name(err));
    }
    
    // DistancePoint point{mac, channel};

    // while(true){
    //     uint32_t distance_cm = point.measureDistance();
    //     ESP_LOGI(TAG, "Distance to point is %" PRIu32, distance_cm);
    //     vTaskDelay(1000 / portTICK_PERIOD_MS);
    // }

    DistanceMeter meter{};
    for(size_t i = 0; i < points_info_len; i++){
        meter.addPoint(points_info[i].bssid, points_info[i].channel);
    }

    meter.startTask();

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
        server();
    } else{
        client();
    }
}

extern "C" void app_main(void)
{
    esp_err_t err;

    err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_start();
    xTaskCreate(startup, "distance-test", 1024*24, NULL, configMAX_PRIORITIES, NULL);
}