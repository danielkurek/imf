/**
 * @file wifi_connect.h
 * @author Daniel Kurek (daniel.kurek.dev@gmail.com)
 * @brief Helping function for managing WiFi connection in ESP-IDF
 * @version 0.1
 * @date 2023-06-20
 * 
 * @copyright Copyright (c) 2023
 * 
 */
#ifndef _WIFI_CONNECT_H
#define _WIFI_CONNECT_H

#include <esp_wifi.h>
#include <esp_log.h>

#ifdef __cplusplus
extern "C"{
#endif

/**
 * @brief Initialize WiFi
 */
void wifi_start(void);
/**
 * @brief Connect to WiFi configured in menuconfig
 * 
 * @return esp_err_t ESP_OK if connected successfully
 */
esp_err_t wifi_connect_default(void);

/**
 * @brief Connect to WiFi with given config
 * 
 * @param wifi_config wifi configuration of the connection
 * @return esp_err_t ESP_OK if connected successfully
 */
esp_err_t wifi_connect(wifi_config_t wifi_config);
/**
 * @brief Connect to WiFi using simple SSID and password
 * 
 * @param ssid WiFi SSID
 * @param password password of WiFi (empty string if WiFi has no password)
 * @return esp_err_t ESP_OK if connected successfully
 */
esp_err_t wifi_connect_simple(const char* ssid, const char* password);

/**
 * @brief Default configuration for connecting to WiFi
 * 
 * @return wifi_config_t defualt configuration
 */
wifi_config_t wifi_sta_config_default();

/**
 * @brief Initialize WiFi AP
 * 
 * @param wifi_config configuration of WiFi AP
 * @return esp_err_t ESP_OK if successfully created
 */
esp_err_t wifi_init_ap(wifi_config_t wifi_config);
/**
 * @brief Create WiFi using simple interface
 * 
 * @param ssid SSID string
 * @param password password string
 * @param channel channel number
 * @return esp_err_t ESP_OK if successfully created
 */
esp_err_t wifi_init_ap_simple(const char* ssid, const char* password, uint8_t channel);
/**
 * @brief Create WiFi AP using credentials from menuconfig
 * 
 * @return esp_err_t ESP_OK if successfully created
 */
esp_err_t wifi_init_ap_default();

/**
 * @brief Stop WiFi connections (disconnect and deinitialization)
 * 
 */
void wifi_shutdown(void);

#ifdef __cplusplus
}
#endif

#endif