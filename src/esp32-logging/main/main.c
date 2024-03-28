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
#include <inttypes.h>
#include "driver/gpio.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <errno.h>

#ifdef CONFIG_IDF_TARGET_ESP32C3
#define CONFIG_BUTTON GPIO_NUM_9
#else
#define CONFIG_BUTTON GPIO_NUM_0
#endif

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
    if(logger_init(ESP_LOG_INFO)){
        logger_output_to_default(false);
        logger_init_storage();
        
        logger_output_to_uart(UART_NUM_1, 17, 18, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
        const char* filename = "/logs/log.txt";
        logger_output_to_file(filename, 5000);
    }
    // ota_init(&event_handler);
    // ota_task();
    
    // ota_rollback_checkpoint();
    // ota_deinit();

    // This should fill 0xF0000 partition at least once
    ESP_LOGI(TAG, "Start writing to log");
    for(uint32_t i = 0; i < 16000; i++){
        LOGGER_I(TAG, "Informative Msg %0" PRIu32, i);
        LOGGER_W(TAG, "Warning Msg %0" PRIu32, i);
        if(i % 400 == 0){
            vTaskDelay(500 / portTICK_PERIOD_MS);
            ESP_LOGI(TAG, "Written %" PRIu32" messages", i);
        }
    }
    ESP_LOGI(TAG, "Stopped writing to log");

    vTaskDelay(5000 / portTICK_PERIOD_MS);
    logger_dump_log_file();
    logger_stop();
    
    vTaskDelete(NULL);
}

void test_storage(void *pvParameters){
    logger_init_storage();
    FILE *f;
    struct stat st;
    if (stat("/logs/log.txt", &st) == 0) {
        // File exists
        ESP_LOGI(TAG, "file already exists, opening in r+ mode");
        f = fopen("/logs/log.txt", "r+");
    } else {
        f = fopen("/logs/log.txt", "w+");
    }
    fprintf(f, "1:Testing ksd;lajdlksnhkla ndkjs kdsjkl sjdbn jksajkd nsaklj\r\n");
    fprintf(f, "2:djklsahldhkjsahkdhskjahdkjhs kjah dkjshakj hksajhkjsahkjdk\r\n");
    fprintf(f, "3:pogfjkfiodjgiohfdjghjkfdhgkj hdfjk ghkjfdh kjgfdhkj hgfkjd\r\n");
    fprintf(f, "4:tuirywuitgfnbvmncxvjkshikuf hdksjhf hbmncxbvmn bcxnmb v iu\r\n");
    fflush(f);
    fsync(fileno(f));
    
    if(0 != fseek(f, 0, SEEK_SET)){
        ESP_LOGE(TAG, "Could not seek to beginning of log file");
        return;
    }
    char line[128];
    char *pos;
    ESP_LOGI(TAG, "#########################\nSTART OF DUMP\n");
    while(fgets(line, sizeof(line), f) != NULL){
        pos = strchr(line, '\n');
        if (pos) {
                *pos = '\0';
        }
        ESP_LOGI(TAG, "File read: %s", line);
    }

    if(0 != fseek(f, 0, SEEK_SET)){
        ESP_LOGE(TAG, "Could not seek to beginning of log file");
    }
    fprintf(f, "65478913246578912346579812346578\r\n");

    if(0 != fseek(f, 0, SEEK_SET)){
        ESP_LOGE(TAG, "Could not seek to beginning of log file");
    }

    ESP_LOGI(TAG, "#########################\nSTART OF DUMP\n");
    while(fgets(line, sizeof(line), f) != NULL){
        pos = strchr(line, '\n');
        if (pos) {
                *pos = '\0';
        }
        ESP_LOGI(TAG, "File read: %s", line);
    }
    vTaskDelete(NULL);
}


bool web_config(){
    gpio_config_t config;

    config.intr_type    = GPIO_INTR_DISABLE;
    config.mode         = GPIO_MODE_INPUT;
    config.pin_bit_mask = (1ull << CONFIG_BUTTON);
    config.pull_down_en = GPIO_PULLDOWN_DISABLE;
    config.pull_up_en   = GPIO_PULLUP_ENABLE;

    gpio_config(&config);

    vTaskDelay(1000 / portTICK_PERIOD_MS);

    if(gpio_get_level(CONFIG_BUTTON) == 0){
        LOGGER_I(TAG, "Booting to web config");
        web_config_start();
        return true;
    }
    return false;
}

void app_main(void)
{
    if(web_config()){
        return;
    }
    // avoid watchdog for IDLE task (needs to run once in a while)
    xTaskCreate(test_logging, "Logging Test", 1024 * 8, NULL, tskIDLE_PRIORITY, NULL);
    // xTaskCreate(test_storage, "Logging Test", 1024 * 8, NULL, tskIDLE_PRIORITY, NULL);
}
