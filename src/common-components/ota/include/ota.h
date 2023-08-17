#ifndef _OTA_H
#define _OTA_H

#include "esp_https_ota.h"

ESP_EVENT_DECLARE_BASE(OTA_EVENT);
typedef enum {
    OTA_START,                    /*!< OTA started */
    OTA_IMG_SIZE,
} ota_event_t;

void ota_register_events(void * event_handler_arg);
void ota_init(void * event_handler_arg);
void ota_rollback_checkpoint();
esp_https_ota_config_t ota_config_default(const char*);
esp_err_t ota_start(esp_https_ota_config_t *ota_config);
esp_err_t ota_task();
void ota_deinit();

#endif