#include <stdio.h>
#include "logger.h"
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <stdbool.h>
#include "ota.h"
#include "esp_event.h"
#include "esp_system.h"
#include "web_config.h"

static const char *TAG = "MAIN";

/* Event handler for catching system events */
static void event_handler(void* arg, esp_event_base_t event_base,
                          int32_t event_id, void* event_data)
{
    if (event_base == ESP_HTTPS_OTA_EVENT) {
        switch (event_id) {
            case ESP_HTTPS_OTA_START:
                ESP_LOGI(TAG, "OTA started");
                break;
            case ESP_HTTPS_OTA_CONNECTED:
                ESP_LOGI(TAG, "Connected to server");
                break;
            case ESP_HTTPS_OTA_GET_IMG_DESC:
                ESP_LOGI(TAG, "Reading Image Description");
                break;
            case ESP_HTTPS_OTA_VERIFY_CHIP_ID:
                ESP_LOGI(TAG, "Verifying chip id of new image: %d", *(esp_chip_id_t *)event_data);
                break;
            case ESP_HTTPS_OTA_DECRYPT_CB:
                ESP_LOGI(TAG, "Callback to decrypt function");
                break;
            case ESP_HTTPS_OTA_WRITE_FLASH:
                ESP_LOGD(TAG, "Writing to flash: %d written", *(int *)event_data);
                break;
            case ESP_HTTPS_OTA_UPDATE_BOOT_PARTITION:
                ESP_LOGI(TAG, "Boot partition updated. Next Partition: %d", *(esp_partition_subtype_t *)event_data);
                break;
            case ESP_HTTPS_OTA_FINISH:
                ESP_LOGI(TAG, "OTA finish");
                break;
            case ESP_HTTPS_OTA_ABORT:
                ESP_LOGI(TAG, "OTA abort");
                break;
        }
    } else if (event_base == OTA_EVENT){
        switch(event_id){
            case OTA_IMG_SIZE:
                ESP_LOGI(TAG, "Total image size: %d", *(int *)event_data);
                break;
        }
    }
}

void test_logging(void *pvParameters){
    ota_init(&event_handler);
    ota_task();

    logger_init(ESP_LOG_INFO);
    logger_output_to_default();
    logger_init_storage();
    
    logger_output_to_uart(UART_NUM_1, 17, 18, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    const char* filename = "/logs/log.txt";
    logger_output_to_file(filename, 2000);
    
    ota_rollback_checkpoint();
    ota_deinit();

    for(int i = 0; i < 20; i++){
        LOGGER_I(TAG, "#####new OTA image %d", i);
    }

    vTaskDelay(5000 / portTICK_PERIOD_MS);
    logger_dump_log_file();
    logger_stop();
    
    vTaskDelete(NULL);
}


void app_main(void)
{
    // avoid watchdog for IDLE task (needs to run once in a while)
    xTaskCreate(test_logging, "Logging Test", 1024 * 8, NULL, tskIDLE_PRIORITY, NULL);
}
