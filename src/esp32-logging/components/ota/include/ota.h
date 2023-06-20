#ifndef _OTA_H
#define _OTA_H

void ota_init();
void ota_rollback_checkpoint();
void ota_task(void *pvParameter);

#endif