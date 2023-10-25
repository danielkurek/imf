#include <stdio.h>
#include "distance_meter.hpp"
#include "wifi_connect.h"

#define CONFIG_BUTTON 9

static const char *TAG = "APP";

void server(){
    wifi_init_ap_simple("TestingWifi", "SuPerdUpeRStrongPaSswoRdTHatNoOneWillguess", 1);
}

void client(){
    uint8_t mac[6] = {0,0,0,0,0,0};
    uint8_t channel = 1;
    DistancePoint::initDistanceMeasurement();
    
    DistancePoint point{mac, channel};

    while(true){
        uint32_t distance_cm = point.measureDistance();
        LOG_I(APP, "Distance to point is %ll", distance_cm);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }

    // DistanceMeter meter{};
    // meter.addPoint(mac, 1);

    // meter.startTask();
}

void startup(void* param){
    gpio_config_t config = {
        .pin_bit_mask = 1ull << CONFIG_BUTTON,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = 1,
    };

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
    wifi_start();
    xTaskCreate(startup, "distance-test", 1024*8, NULL, configMAX_PRIORITIES, NULL);
}