#include "logger.h"
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <esp_log.h>
#include <stdbool.h>
#include "esp_littlefs.h"
#include <string.h>
#include <unistd.h>
#include <driver/uart.h>
#include <inttypes.h>
#include <sys/stat.h>
#include <errno.h>
#include "esp_random.h"

#define BLOCK_SIZE 4096
#define MIN_BLOCKS_FREE 10
#define MIN_FREE_SPACE MIN_BLOCKS_FREE * BLOCK_SIZE

static const char *TAG = "LOGGER";

static logger_conf_t conf;
static QueueHandle_t uart_queue;

void logger_init(esp_log_level_t level){
    static bool initialized = false;
    if(initialized) return;
    conf.level = level;
    uint8_t rnd = (uint8_t) esp_random();
    
    // A-Z (65-90)
    conf.rnd_char = 'A' + rnd % ('Z' - 'A');
    initialized = true;
}

char logger_get_current_rnd_letter(){
    return conf.rnd_char;
}

bool logger_init_storage(){
    static bool initialized = false;
    if(initialized) return true;

    esp_vfs_littlefs_conf_t storage_conf = {
      .base_path = "/logs",
      .partition_label = "logs",
      .format_if_mount_failed = true,
    };
    esp_err_t ret = esp_vfs_littlefs_register(&storage_conf);
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
    ret = esp_littlefs_info(storage_conf.partition_label, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get LittleFS partition information (%s). Formatting...", esp_err_to_name(ret));
        esp_littlefs_format(storage_conf.partition_label);
        return false;
    } else {
        ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
        if(total >= used){
            conf.storage_size_total = total;
            conf.storage_size_used = used;
            conf.storage_size_threshold = total - MIN_FREE_SPACE;
            ESP_LOGI(TAG, "Size threshold %d", conf.storage_size_threshold);
        }
    }

    initialized = true;

    return true;
}

void logger_output_to_default(){
    conf.to_default = true;
}

bool logger_output_to_file(const char* filename, long int protect_start_bytes){
    if(conf.log_file != NULL){
        logger_sync_file();
        fclose(conf.log_file);
        conf.log_file = NULL;
    }
    struct stat st;
    if (stat(filename, &st) == 0) {
        // File exists
        ESP_LOGI(TAG, "file already exists, opening in r+ mode");
        conf.log_file = fopen(filename, "r+");
    } else {
        conf.log_file = fopen(filename, "w+");
    }

    if(conf.log_file == NULL){
        ESP_LOGE(TAG, "cannot open file for logging");
        return false;
    }

    fseek(conf.log_file, 0, SEEK_END);

    conf.to_file = true;
    conf.log_file_name = filename;

    conf.storage_protect_log_start = protect_start_bytes != 0;
    conf.storage_overwriting = false;
    if(conf.storage_protect_log_start){
        conf.storage_protect_start = ftell(conf.log_file);
        if(conf.storage_protect_start < 0L){
            ESP_LOGE(TAG, "cannot get position of log file; %s", strerror(errno));
            return false;
        }
        conf.storage_protect_end = conf.storage_protect_start + protect_start_bytes;
    }

    LOGGER_E("", "\n####[START OF LOG]####\n\n");
    return true;
}

void logger_sync_file(){
    if(conf.log_file == NULL || conf.to_file == false){
        return;
    }
    fflush(conf.log_file);
    fsync(fileno(conf.log_file));
}

void logger_set_file_overwrite(){
    logger_sync_file();

    fseek(conf.log_file, 0, SEEK_END);
    long file_size = ftell(conf.log_file);
    fseek(conf.log_file, 0, SEEK_SET);

    ESP_LOGI(TAG, "file %s size %"PRId32", used %d", conf.log_file_name, file_size, conf.storage_size_used);
    conf.storage_size_used -= file_size;
    conf.storage_overwriting = true;
}

bool logger_output_to_uart(const uart_port_t port, int tx_io_num, int rx_io_num, int rts_io_num, int cts_io_num){
    conf.uart_num = port;

    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 122,
        .source_clk = UART_SCLK_DEFAULT,
    };
    
    if(uart_param_config(port, &uart_config) != ESP_OK){
        ESP_LOGE(TAG, "cannot set UART params");
        return false;
    }

    if(uart_set_pin(port, tx_io_num, rx_io_num, rts_io_num, cts_io_num) != ESP_OK){
        ESP_LOGE(TAG, "cannot set UART pins");
        return false;
    }

    const int uart_buffer_size = (1024 * 2);
    if(uart_driver_install(port, uart_buffer_size, uart_buffer_size, 10, &uart_queue, 0) != ESP_OK){
        ESP_LOGE(TAG, "cannot install UART driver");
        return false;
    }
    
    conf.to_uart = true;

    return true;
}
void logger_set_log_level(esp_log_level_t level){
    conf.level = level;
}

void logger_write(esp_log_level_t level, const char * tag, const char * format, ...){
    static int writes_to_file = 0;
    if(level > conf.level){
        return;
    }
    va_list args;
    
    va_start(args, format);

    if(conf.to_default){
        esp_log_writev(level, tag, format, args);
    }

    if(conf.to_file || conf.to_uart){
        char out[256];
        int ret = vsnprintf(out, sizeof(out), format, args);
        if (ret >= 0){
            if(conf.to_file){
                if(conf.storage_size_used >= conf.storage_size_threshold){
                    ESP_LOGI(TAG, "file estimation reached %d; starting overwrite", conf.storage_size_used);
                    logger_set_file_overwrite();
                }
                if(conf.storage_protect_log_start && conf.storage_overwriting){
                    if(ftell(conf.log_file) + ret >= conf.storage_protect_start - 1){
                        fseek(conf.log_file, conf.storage_protect_end, SEEK_SET);
                    }
                }
                int written = vfprintf(conf.log_file, format, args);
                conf.storage_size_used += written;
                writes_to_file += 1;
                if(writes_to_file > 10){
                    logger_sync_file();
                    writes_to_file = 0;
                }
            }
            if(conf.to_uart){
                uart_write_bytes(conf.uart_num, out, strlen(out));
            }
        } else{
            ESP_LOGE(TAG, "cannot print create msg");
        }
    }

    va_end(args);
}

bool logger_dump_log_file(){
    if(conf.log_file == NULL || !conf.to_file){
        ESP_LOGE(TAG, "log file is not opened");
        return false;
    }
    logger_sync_file();
    fpos_t orig_pos;
    ESP_LOGI(TAG, "getting position");
    if(fgetpos(conf.log_file, &orig_pos) != 0){
        ESP_LOGE(TAG, "cannot get position of log file; %s", strerror(errno));
        return false;
    }
    ESP_LOGI(TAG, "rewinding");
    fseek(conf.log_file, 0, SEEK_SET);

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
    ESP_LOGI(TAG, "setting position");
    if(fsetpos(conf.log_file, &orig_pos) != 0){
        ESP_LOGE(TAG, "cannot set position of log file; %s", strerror(errno));
        return false;
    }
    return true;
}

FILE * logger_get_file(){
    return conf.log_file;
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

void logger_storage_close(){
    if(conf.log_file != NULL){
        ESP_LOGI(TAG, "closing file");
        fclose(conf.log_file);
    }
    conf.to_file = false;
    ESP_LOGI(TAG, "unregistering spiffs");
    esp_vfs_littlefs_unregister(LOGGER_STORAGE_LABEL);
}

void logger_stop(){
    conf.to_default = false;
    conf.to_uart = false;
    logger_storage_close();
}