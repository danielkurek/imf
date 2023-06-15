#include "logger.h"
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <esp_log.h>
#include <stdbool.h>
#include <esp_littlefs.h>
#include <string.h>

static const char *TAG = "LOGGER";

static struct logger_conf conf;

void logger_init(esp_log_level_t level){
    conf.level = level;
    conf.to_uart = true;
}

bool logger_init_output_to_file(){
    esp_vfs_littlefs_conf_t conf = {
      .base_path = LOGGER_STORAGE_MOUNT,
      .partition_label = LOGGER_STORAGE_LABEL,
      .format_if_mount_failed = true
    };
    esp_err_t ret = esp_vfs_littlefs_register(&conf);
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
                ESP_LOGE(TAG, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
                ESP_LOGE(TAG, "Failed to find LittleFS partition");
        } else {
                ESP_LOGE(TAG, "Failed to initialize LittleFS (%s)", esp_err_to_name(ret));
        }
        return false;
    }

    size_t total = 0, used = 0;
    ret = esp_littlefs_info(conf.partition_label, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s). Formatting...", esp_err_to_name(ret));
        esp_littlefs_format(conf.partition_label);
        return false;
    } else {
        ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
    }

    return true;
}

bool logger_output_to_file(const char* filename){
    conf.log_file = fopen(filename, "a");
    if(conf.log_file == NULL){
        return false;
    }
    conf.to_file = true;
    fprintf(conf.log_file, "\n####[START OF LOG]####\n\n");
    return true;
}
bool logger_output_to_uart(int pin){
    return false;
}
void logger_set_log_level(esp_log_level_t level){
    conf.level = level;
}

void logger_write(esp_log_level_t level, const char * tag, const char * format, ...){
    if(level > conf.level){
        return;
    }
    va_list args;
    
    va_start(args, format);

    if(conf.to_uart){
        esp_log_writev(level, tag, format, args);
    }
    if(conf.to_file){
        vfprintf(conf.log_file, format, args);
    }
    va_end(args);
}

bool logger_dump_log_file(const char* filename){
    FILE *f = fopen(filename, "r");
    if(f == NULL){
        ESP_LOGE(TAG, "Failed to open file for reading");
        return false;
    }
    char line[128];
    char *pos;
    ESP_LOGI(TAG, "#########################\nSTART OF DUMP\n");
    while( fgets(line, sizeof(line), f) != NULL){
        pos = strchr(line, '\n');
        if (pos) {
                *pos = '\0';
        }
        ESP_LOGI(TAG, "File read: %s", line);
    }
    fclose(f);
    return true;
}

void logger_close(){
    if(conf.log_file != NULL){
        fclose(conf.log_file);
    }
    conf.to_file = false;
}