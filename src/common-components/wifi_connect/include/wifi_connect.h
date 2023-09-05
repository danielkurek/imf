#ifndef _WIFI_CONNECT_H
#define _WIFI_CONNECT_H

#include <esp_wifi.h>
#include <esp_log.h>

void wifi_start(void);
esp_err_t wifi_connect_default(void);
esp_err_t wifi_connect(wifi_config_t wifi_config);
esp_err_t wifi_connect_simple(const char* ssid, const char* password);
esp_err_t wifi_init_ap(wifi_config_t wifi_config);
wifi_config_t wifi_sta_config_default();
esp_err_t wifi_init_ap_simple(const char* ssid, const char* password, uint8_t channel);
esp_err_t wifi_init_ap_default();
void wifi_shutdown(void);

#endif