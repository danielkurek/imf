#ifndef LOGGER_H_
#define LOGGER_H_

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <esp_log.h>
#include <stdbool.h>

// Log storage config
#define LOGGER_STORAGE_MOUNT "/logs"
#define LOGGER_STORAGE_LABEL "logs"
#define LOGGER_FILE(filename) LOGGER_STORAGE_MOUNT "/" filename
struct logger_conf{
  esp_log_level_t level;
  FILE *log_file;
  int uart_pin;
  bool to_file;
  bool to_uart;
  bool to_default;
  const char* log_file_name;
};

void logger_init(esp_log_level_t level);
bool logger_init_storage();
bool logger_output_to_file(const char* filename);
bool logger_output_to_uart(int pin);
void logger_set_log_level(esp_log_level_t level);

void logger_write(esp_log_level_t level, const char * tag, const char * format, ...);

#define LOGGER_FORMAT(letter, format) #letter " (%d) %s: " format "\n"
#define LOGGER_V( tag, format, ...) logger_write(ESP_LOG_VERBOSE, tag, LOGGER_FORMAT(V, format), esp_log_timestamp(), tag, ##__VA_ARGS__);
#define LOGGER_D( tag, format, ...) logger_write(ESP_LOG_DEBUG, tag, LOGGER_FORMAT(D, format), esp_log_timestamp(), tag, ##__VA_ARGS__);
#define LOGGER_I( tag, format, ...) logger_write(ESP_LOG_INFO, tag, LOGGER_FORMAT(I, format), esp_log_timestamp(), tag, ##__VA_ARGS__);
#define LOGGER_W( tag, format, ...) logger_write(ESP_LOG_WARN, tag, LOGGER_FORMAT(W, format), esp_log_timestamp(), tag, ##__VA_ARGS__);
#define LOGGER_E( tag, format, ...) logger_write(ESP_LOG_ERROR, tag, LOGGER_FORMAT(E, format), esp_log_timestamp(), tag, ##__VA_ARGS__);

bool logger_dump_log_file();
bool logger_delete_log(const char *filename);

void logger_close();

#endif