#include <stdio.h>
#include "logger.h"
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <stdbool.h>

void test_logging(void *pvParameters){
    logger_init(ESP_LOG_INFO);
    logger_output_to_default();
    logger_init_storage();

    logger_output_to_uart(UART_NUM_1, 17, 18, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    const char* filename = "/logs/log.txt";
    logger_output_to_file(filename);

    vTaskDelay(10000 / portTICK_PERIOD_MS);
    logger_dump_log_file();
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    
    for(int i = 0; i < 568; i++){
        LOGGER_E("LOGGER_TEST", "%d Hello world %d!", i, i);
        vTaskDelay(50 / portTICK_PERIOD_MS);
    }

    vTaskDelay(2000 / portTICK_PERIOD_MS);
    logger_dump_log_file();
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    logger_close();
    vTaskDelete(NULL);
}

void app_main(void)
{
    // avoid watchdog for IDLE task (needs to run once in a while)
    xTaskCreate(test_logging, "Logging Test", 8192, NULL, tskIDLE_PRIORITY, NULL);
}
