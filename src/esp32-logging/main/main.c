#include <stdio.h>
#include "logger.h"
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <stdbool.h>

void app_main(void)
{
    logger_init(ESP_LOG_INFO);
    logger_init_storage();
    const char* filename = LOGGER_FILE("log.txt");
    int i=0;
    while(1){
        if(i % 10 == 0){
            logger_close();
            bool res = logger_delete_log(filename);
            ESP_LOGI("DEL", "delete status %d, out %d", res, logger_output_to_file(filename));
        }
        LOGGER_E("LOGGER_TEST", "Hello world %d!", i);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
        logger_dump_log_file();
        vTaskDelay(2000 / portTICK_PERIOD_MS);
        i++;
    }
}
