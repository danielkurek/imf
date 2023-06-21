#ifndef _OTA_H
#define _OTA_H

#include "esp_https_ota.h"

void ota_init(void * event_handler_arg);
void ota_rollback_checkpoint();
esp_err_t ota_task();

#endif