#ifndef WEB_CONFIG_H_
#define WEB_CONFIG_H_

#include "esp_err.h"
#include "nvs_flash.h"
#include <stdbool.h>

typedef struct {
    char *key;
    nvs_type_t type;
    bool value_to_log;
} config_option_t;

void web_config_start();
void web_config_stop();

esp_err_t web_config_set_custom_options(size_t size, config_option_t options_arr[size]);

#endif
