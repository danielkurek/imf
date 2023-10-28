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

void server(){
    wifi_init_ap_simple("TestingWifi", "SuPerdUpeRStrongPaSswoRdTHatNoOneWillguess", 1);

    vTaskDelete(NULL);
}

void client(){
    esp_err_t err;

    //34:b4:72:69:e6:13
    uint8_t mac[6] = {0x34,0xb4,0x72,0x69,0xe6,0x13};
    uint8_t channel = 1;
    
    err = DistancePoint::initDistanceMeasurement();
    if(err != ESP_OK){
        ESP_LOGI(TAG, "Error initiliazing DM, err: %s", esp_err_to_name(err));
    }
    
    DistancePoint point{mac, channel};

    while(true){
        uint32_t distance_cm = point.measureDistance();
        ESP_LOGI(TAG, "Distance to point is %" PRIu32, distance_cm);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }

    // DistanceMeter meter{};
    // meter.addPoint(mac, 1);

    // meter.startTask();
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
    xTaskCreate(startup, "distance-test", 1024*8, NULL, configMAX_PRIORITIES, NULL);
}