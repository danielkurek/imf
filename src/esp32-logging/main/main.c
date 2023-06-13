#include <stdio.h>
#include "logger.h"
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

void app_main(void)
{
    logger_init(ESP_LOG_INFO);
    while(1){
        LOGGER_E("LOGGER_TEST", "Hello world %d!\n", 42);
        ESP_LOGI("TEST", "Hello world %d!\n", 42);
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }
}
