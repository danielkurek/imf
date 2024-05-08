/**
 * @file web_config.h
 * @author Daniel Kurek (daniel.kurek.dev@gmail.com)
 * @brief Configuration through web interface
 * @version 0.1
 * @date 2023-08-11
 * 
 * @copyright Copyright (c) 2023
 * 
 */
#ifndef WEB_CONFIG_H_
#define WEB_CONFIG_H_

#include "esp_err.h"
#include "nvs_flash.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C"{
#endif

/**
 * @brief validation function
 * 
 * @param received_value received value from website interface
 * @return esp_err_t ESP_OK if value is valid and should be saved
 */
typedef esp_err_t (*web_config_validate_function_t) (const char *received_value);

typedef struct {
    const char *key; /**< option key (will be displayed in web interface and is used as key in NVS)*/
    nvs_type_t type; /**< NVS type that will be stored (supported types: NVS_TYPE_STR, NVS_TYPE_I16, NVS_TYPE_U16)*/
    bool value_to_log; /**< can the value be written to log when value changes? */
    web_config_validate_function_t validate_function; /**< validation function, value will be saved only if it is valid */
} config_option_t;

/**
 * @brief Start web server with website interface for configuration
 */
void web_config_start();

/**
 * @brief Stop web server (it is recommended to reboot the device instead of stopping the web server)
 */
void web_config_stop();

/**
 * @brief Set custom options for web configuration
 * Caller is responsible for ensuring that \p options_arr will be valid for the whole duration of web config
 * 
 * @param size size of array @p options_arr
 * @param options_arr array custom options
 * @return esp_err_t 
 */
esp_err_t web_config_set_custom_options(size_t size, config_option_t* options_arr);

#ifdef __cplusplus
}
#endif

#endif
