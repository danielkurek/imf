#include "ota.h"
#include <string.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "wifi_connect.h"
#include "esp_netif.h"
#include "esp_wifi.h"

ESP_EVENT_DEFINE_BASE(OTA_EVENT);

static const char *TAG = "advanced_https_ota";
extern const uint8_t server_cert_pem_start[] asm("_binary_ca_cert_pem_start");
extern const uint8_t server_cert_pem_end[] asm("_binary_ca_cert_pem_end");

#define OTA_URL_SIZE 256

static esp_err_t validate_image_header(esp_app_desc_t *new_app_info)
{
    if (new_app_info == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_app_desc_t running_app_info;
    if (esp_ota_get_partition_description(running, &running_app_info) == ESP_OK) {
        ESP_LOGI(TAG, "Running firmware version: %s", running_app_info.version);
    }

#ifndef CONFIG_OTA_SKIP_VERSION_CHECK
    if (memcmp(new_app_info->version, running_app_info.version, sizeof(new_app_info->version)) == 0) {
        ESP_LOGW(TAG, "Current running version is the same as a new. We will not continue the update.");
        return ESP_FAIL;
    }
#endif

    return ESP_OK;
}

static esp_err_t _http_client_init_cb(esp_http_client_handle_t http_client)
{
    esp_err_t err = ESP_OK;
    /* Uncomment to add custom headers to HTTP request */
    // err = esp_http_client_set_header(http_client, "Custom-Header", "Value");
    return err;
}

esp_err_t ota_start(esp_https_ota_config_t *ota_config)
{
    ESP_LOGI(TAG, "Starting Advanced OTA example");

    esp_err_t ota_finish_err = ESP_OK;
    // esp_err_t ret = esp_https_ota(&ota_config);
    // if (ret == ESP_OK) {
    //     esp_restart();
    // } else {
    //     ESP_LOGE(TAG, "FAIL");
    //     // vTaskDelete(NULL);
    //     return ESP_FAIL;
    // }
    // ESP_LOGI(TAG, "Success");
    // // vTaskDelete(NULL);
    // return ESP_OK;

    esp_https_ota_handle_t https_ota_handle = NULL;
    esp_err_t err = esp_https_ota_begin(ota_config, &https_ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ESP HTTPS OTA Begin failed");
        return ESP_FAIL;
    }
    
    int bin_size = esp_https_ota_get_image_size(https_ota_handle);
    ESP_LOGI(TAG, "Total image size: %d", bin_size);

    // esp_event copies the event_data, so no need to worry about it not being valid
    if(esp_event_post(OTA_EVENT, OTA_IMG_SIZE, (void *)(&bin_size), sizeof(int), portMAX_DELAY) != ESP_OK){
        ESP_LOGE(TAG, "Could not post event: OTA_IMG_SIZE");
    }

    esp_app_desc_t app_desc;
    err = esp_https_ota_get_img_desc(https_ota_handle, &app_desc);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_https_ota_read_img_desc failed");
        goto ota_end;
    }
    err = validate_image_header(&app_desc);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "image header verification failed");
        goto ota_end;
    }

    while (1) {
        err = esp_https_ota_perform(https_ota_handle);
        if (err != ESP_ERR_HTTPS_OTA_IN_PROGRESS) {
            break;
        }
        // esp_https_ota_perform returns after every read operation which gives user the ability to
        // monitor the status of OTA upgrade by calling esp_https_ota_get_image_len_read, which gives length of image
        // data read so far.
        ESP_LOGD(TAG, "Image bytes read: %d", esp_https_ota_get_image_len_read(https_ota_handle));
    }

    if (esp_https_ota_is_complete_data_received(https_ota_handle) != true) {
        // the OTA image was not completely received and user can customise the response to this situation.
        ESP_LOGE(TAG, "Complete data was not received.");
    } else {
        ota_finish_err = esp_https_ota_finish(https_ota_handle);
        if ((err == ESP_OK) && (ota_finish_err == ESP_OK)) {
            ESP_LOGI(TAG, "ESP_HTTPS_OTA upgrade successful. Rebooting ...");
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            esp_restart();
        } else {
            if (ota_finish_err == ESP_ERR_OTA_VALIDATE_FAILED) {
                ESP_LOGE(TAG, "Image validation failed, image is corrupted");
            }
            ESP_LOGE(TAG, "ESP_HTTPS_OTA upgrade failed 0x%x", ota_finish_err);
            return ESP_OK;
        }
    }

ota_end:
    esp_https_ota_abort(https_ota_handle);
    ESP_LOGE(TAG, "ESP_HTTPS_OTA upgrade failed");
    return ESP_FAIL;
}

esp_https_ota_config_t ota_config_default(const char* url){
    esp_http_client_config_t config = {
        .url = url,
        .cert_pem = (char *)server_cert_pem_start,
        .timeout_ms = CONFIG_OTA_OTA_RECV_TIMEOUT,
        .keep_alive_enable = true,
    };
    ESP_LOGI(TAG, "Certificate:\n%s\n\n", server_cert_pem_start);
#ifdef CONFIG_OTA_SKIP_COMMON_NAME_CHECK
    config.skip_cert_common_name_check = true;
#endif

    esp_https_ota_config_t ota_config = {
        .http_config = &config,
        .http_client_init_cb = _http_client_init_cb, // Register a callback to be invoked after esp_http_client is initialized
#ifdef CONFIG_OTA_ENABLE_PARTIAL_HTTP_DOWNLOAD
        .partial_http_download = true,
        .max_http_request_size = CONFIG_OTA_HTTP_REQUEST_SIZE,
#endif
    };
    return ota_config;
}

esp_err_t ota_task(){
    esp_https_ota_config_t ota_config = ota_config_default(CONFIG_OTA_FIRMWARE_UPGRADE_URL);
    return ota_start(&ota_config);
}

void ota_rollback_checkpoint(){
    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_ota_img_states_t ota_state;
    if (esp_ota_get_state_partition(running, &ota_state) == ESP_OK) {
        if (ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
            if (esp_ota_mark_app_valid_cancel_rollback() == ESP_OK) {
                ESP_LOGI(TAG, "App is valid, rollback cancelled successfully");
            } else {
                ESP_LOGE(TAG, "Failed to cancel rollback");
            }
        }
    }
}

void ota_register_events(esp_event_handler_t event_handler){
    ESP_ERROR_CHECK(esp_event_handler_register(ESP_HTTPS_OTA_EVENT, ESP_EVENT_ANY_ID, event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(OTA_EVENT, ESP_EVENT_ANY_ID, event_handler, NULL));
}

void ota_init(esp_event_handler_t event_handler)
{
    ESP_LOGI(TAG, "OTA example app_main start");
    // Initialize NVS.
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK( err );

    ESP_ERROR_CHECK(esp_netif_init());
    err = esp_event_loop_create_default();
    // ESP_ERR_INVALID_STATE is returned when default loop was already created
    if(err != ESP_OK && err != ESP_ERR_INVALID_STATE){
        ESP_LOGE(TAG, "Could not create default event loop! err=%d", err);
        return;
    }

    ota_register_events(event_handler);
    
    // connect to wifi according to menuconfig
    ESP_ERROR_CHECK(wifi_connect_default());

    esp_wifi_set_ps(WIFI_PS_NONE);
}

void ota_deinit(){
    wifi_shutdown();
    ESP_ERROR_CHECK(esp_event_loop_delete_default());
}