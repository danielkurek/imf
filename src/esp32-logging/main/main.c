#include <stdio.h>
#include "logger.h"
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <stdbool.h>

void test_logging(void *pvParameters){
    logger_init(ESP_LOG_INFO);
    logger_init_storage();
    logger_output_to_uart(UART_NUM_1, 17, 18, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    const char* filename = "/logs/log.txt";
    logger_output_to_file(filename);
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    logger_dump_log_file();
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    
    int i=0;
    while(1){
        LOGGER_E("LOGGER_TEST", "Hello world %d!", i);
        vTaskDelay(50 / portTICK_PERIOD_MS);
        // if(i%100 == 0){
        //     logger_dump_log_file();
        //     vTaskDelay(500 / portTICK_PERIOD_MS);
        // }
        i++;
        if(i >= 500){
            break;
        }
    }
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    logger_dump_log_file();
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    logger_close();
    vTaskDelete(NULL);
}

void app_main(void)
{
    xTaskCreate(test_logging, "DumpLogFile", 8192, NULL, tskIDLE_PRIORITY, NULL);
}
