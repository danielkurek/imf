#ifndef WEB_CONFIG_H_
#define WEB_CONFIG_H_

#include "esp_err.h"
#include "nvs_flash.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C"{
#endif

typedef esp_err_t (*web_config_validate_function_t) (const char *received_value);

typedef struct {
    const char *key;
    nvs_type_t type;
    bool value_to_log;
    web_config_validate_function_t validate_function;
} config_option_t;

void web_config_start();
void web_config_stop();

// caller is responsible for ensuring that `options_arr`
// will be valid for the whole duration of web config 
esp_err_t web_config_set_custom_options(size_t size, config_option_t* options_arr);

#ifdef __cplusplus
}
#endif

#endif
