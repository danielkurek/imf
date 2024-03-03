#include <stdio.h>
#include <inttypes.h>
#include "esp_log.h"
#include "driver/gpio.h"
#include "nvs_flash.h"
#include "esp_check.h"
#include "esp_event.h"

#include "serial_comm_client.hpp"
#include "serial_comm_server.hpp"
#include "driver/uart.h"

#ifdef CONFIG_IDF_TARGET_ESP32C3
#define CONFIG_BUTTON GPIO_NUM_9
#else
#define CONFIG_BUTTON GPIO_NUM_0
#endif

static const char *TAG = "APP";

void server(void* param){
    SerialCommSrv serial_srv {UART_NUM_1, GPIO_NUM_16, GPIO_NUM_17};
    serial_srv.startReadTask();

    vTaskDelete(NULL);
}

void client(void* param){
    SerialCommCli cli {UART_NUM_1, GPIO_NUM_17, GPIO_NUM_16};
    std::string response;
    response = GetStatusName(cli.GetStatus());
    ESP_LOGI(TAG, "Get status: %s", response.c_str());

    // vTaskDelay(1000 / portTICK_PERIOD_MS);

    response = cli.GetField(0x0000, "test");
    ESP_LOGI(TAG, "Get \"test\": %s", response.c_str());

    // vTaskDelay(1000 / portTICK_PERIOD_MS);

    response = cli.PutField(0x0000, "test", "112233");
    ESP_LOGI(TAG, "Put \"test\"=\"112233\": %s", response.c_str());

    // vTaskDelay(1000 / portTICK_PERIOD_MS);

    response = cli.GetField(0x0000, "test");
    ESP_LOGI(TAG, "Get \"test\": %s", response.c_str());

    // vTaskDelay(1000 / portTICK_PERIOD_MS);

    response = cli.GetField(0x0001, "test");
    ESP_LOGI(TAG, "Get \"test\": %s", response.c_str());

    // vTaskDelay(1000 / portTICK_PERIOD_MS);

    response = cli.PutField(0x0001, "test", "445566");
    ESP_LOGI(TAG, "Put \"test\"=\"112233\": %s", response.c_str());

    // vTaskDelay(1000 / portTICK_PERIOD_MS);

    response = cli.GetField(0x0001, "test");
    ESP_LOGI(TAG, "Get \"test\": %s", response.c_str());

    // vTaskDelay(1000 / portTICK_PERIOD_MS);

    response = cli.GetField(0x0000, "rgb");
    ESP_LOGI(TAG, "Get \"rgb\": %s", response.c_str());

    // vTaskDelay(1000 / portTICK_PERIOD_MS);

    response = cli.PutField(0x0000, "rgb", "af31bd");
    ESP_LOGI(TAG, "Put \"rgb\"=\"af31bd\": %s", response.c_str());

    // vTaskDelay(1000 / portTICK_PERIOD_MS);

    response = cli.GetField(0x0000,"rgb");
    ESP_LOGI(TAG, "Get \"rgb\": %s", response.c_str());

    // vTaskDelay(1000 / portTICK_PERIOD_MS);

    vTaskDelete(NULL);
}

void startup(void* param){
    gpio_config_t config {};
    config.pin_bit_mask = 1ull << CONFIG_BUTTON;
    config.mode = GPIO_MODE_INPUT;
    config.pull_up_en = GPIO_PULLUP_ENABLE;

    gpio_config(&config);

    vTaskDelay(1000 / portTICK_PERIOD_MS);

    if(gpio_get_level(CONFIG_BUTTON) == 0){
        server(param);
    } else{
        client(param);
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

    xTaskCreate(startup, "serial-comm-test", 1024*24, NULL, configMAX_PRIORITIES, NULL);
    // xTaskCreate(server, "serial-comm-server", 1024*24, NULL, configMAX_PRIORITIES, NULL);
    // xTaskCreate(client, "serial-comm-client", 1024*24, NULL, configMAX_PRIORITIES, NULL);
}