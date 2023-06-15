#include <stdio.h>
#include "logger.h"
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

void app_main(void)
{
    logger_init(ESP_LOG_INFO);
    logger_init_output_to_file();
    logger_output_to_file(LOGGER_FILE("log.txt"));
    int i=0;
    while(1){
        logger_output_to_file(LOGGER_FILE("log.txt"));
        LOGGER_E("LOGGER_TEST", "Hello world %d!", i);
        ESP_LOGI("TEST", "Hello world %d!", i);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
        logger_close();
        logger_dump_log_file(LOGGER_FILE("log.txt"));
        vTaskDelay(2000 / portTICK_PERIOD_MS);
        i++;
    }
}
