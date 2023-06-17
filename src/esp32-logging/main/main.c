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
    logger_output_to_uart(UART_NUM_1, 17, 18, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    const char* filename = "/logs/log.txt";
    logger_output_to_file(filename);
    logger_dump_log_file();
    
    int i=0;
    while(1){
        // if(i % 10 == 0){
        //     logger_close();
        //     bool res = logger_delete_log(filename);
        //     ESP_LOGI("DEL", "delete status %d, out %d", res, logger_output_to_file(filename));
        // }
        LOGGER_E("LOGGER_TEST", "Hello world %d!", i);
        vTaskDelay(50 / portTICK_PERIOD_MS);
        if(i%100 == 0)
            logger_dump_log_file();
        vTaskDelay(50 / portTICK_PERIOD_MS);
        i++;
        if(i >= 10){
            break;
        }
    }
    logger_dump_log_file();
    logger_close();
}
