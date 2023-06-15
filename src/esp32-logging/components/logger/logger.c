#include "logger.h"
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <esp_log.h>
#include <stdbool.h>
#include <esp_littlefs.h>
#include <string.h>
#include <unistd.h>
#include <driver/uart.h>

static const char *TAG = "LOGGER";

static struct logger_conf conf;
static QueueHandle_t uart_queue;

void logger_init(esp_log_level_t level){
    conf.level = level;
    conf.to_default = true;
}

bool logger_init_storage(){
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
    conf.log_file = fopen(filename, "a+");
    if(conf.log_file == NULL){
        return false;
    }
    conf.to_file = true;
    // conf.log_file_name = filename;
    fprintf(conf.log_file, "\n####[START OF LOG]####\n\n");
    return true;
}
bool logger_output_to_uart(const uart_port_t port, int tx_io_num, int rx_io_num, int rts_io_num, int cts_io_num){
    conf.uart_num = port;

    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_CTS_RTS,
        .rx_flow_ctrl_thresh = 122,
    };
    
    if(uart_param_config(uart_num, &uart_config) != ESP_OK){
        return false;
    }

    if(uart_set_pin(port, tx_io_num, rx_io_num, rts_io_num, cts_io_num) != ESP_OK){
        return false;
    }

    const int uart_buffer_size = (1024 * 2);
    if(uart_driver_install(port, uart_buffer_size, uart_buffer_size, 10, &uart_queue, 0) != ESP_OK){
        return false;
    }
    
    conf.to_uart = true;

    return true;
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

    if(conf.to_default){
        esp_log_writev(level, tag, format, args);
    }

    if(conf.to_file){
        vfprintf(conf.log_file, format, args);
    }

    if(conf.to_uart){
        char out[128];
        int ret = vsnprintf(out, sizeof(out), format, args);
        if (ret >= 0){
            uart_write_bytes(conf.uart_num, out, strlen(out));
        }
    }

    va_end(args);
}

bool logger_dump_log_file(){
    if(conf.log_file == NULL || !conf.to_file){
        return false;
    }
    fpos_t orig_pos;
    if(fgetpos(conf.log_file, &orig_pos) != 0){
        ESP_LOGE(TAG, "cannot get position of log file");
        return false;
    }
    rewind(conf.log_file);

    char line[128];
    char *pos;
    ESP_LOGI(TAG, "#########################\nSTART OF DUMP\n");
    while(fgets(line, sizeof(line), conf.log_file) != NULL){
        pos = strchr(line, '\n');
        if (pos) {
                *pos = '\0';
        }
        ESP_LOGI(TAG, "File read: %s", line);
    }
    if(fsetpos(conf.log_file, &orig_pos) != 0){
        ESP_LOGE(TAG, "cannot set position of log file");
        return false;
    }
    return true;
}

bool logger_delete_log(const char *filename){
    if(conf.to_file){
        ESP_LOGE(TAG, "Cannot remove opened file");
        return false;
    }
    int res = remove(filename);
    ESP_LOGI(TAG, "remove status %d", res);
    return res != 0;
}

void logger_close(){
    if(conf.log_file != NULL){
        fclose(conf.log_file);
    }
    conf.to_file = false;
}