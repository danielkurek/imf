/**
 * @file logger.h
 * @author Daniel Kurek (daniel.kurek.dev@gmail.com)
 * @brief 
 * @version 0.1
 * @date 2023-06-13
 * 
 * @copyright Copyright (c) 2023
 * 
 */
#ifndef LOGGER_H_
#define LOGGER_H_

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <esp_log.h>
#include <stdbool.h>
#include "driver/uart.h"

#ifdef __cplusplus
extern "C"{
#endif

// Log storage config
/**
 * @brief path to mounted storage
 */
#define LOGGER_STORAGE_MOUNT "/logs"
/**
 * @brief partition label of the storage
 */
#define LOGGER_STORAGE_LABEL "logs"
/**
 * @brief Macro for path creation given @p filename
 */
#define LOGGER_FILE(filename) LOGGER_STORAGE_MOUNT "/" filename
/**
 * @brief Default log file name that should be used
 */
#define LOGGER_DEFAULT_FILENAME "log.txt"

/**
 * @brief Storage for logger state and configuration
 */
typedef struct {
  esp_log_level_t level;
  FILE *log_file;
  uart_port_t uart_num;
  bool to_file;
  bool to_uart;
  bool to_default;
  char rnd_char;
  const char* log_file_name;
  size_t storage_size_total;
  size_t storage_size_used;
  size_t storage_size_threshold;
  long int storage_protect_start;
  long int storage_protect_end;
  uint32_t max_log_size;
  bool storage_protect_log_start;
  bool storage_overwriting;
} logger_conf_t;

/**
 * @brief Initialize logging library
 * 
 * @param level highest log level that will be written to outputs (everything below will also be written to outputs)
 * @return bool true if initialized correctly 
 */
bool logger_init(esp_log_level_t level);
/**
 * @brief Initialize storage for logs
 * 
 * @return bool true if initialization succeeds
 */
bool logger_init_storage();

/**
 * @brief Get current letter that differentiates logs from different runs
 * 
 * @return char random letter for current session
 */
char logger_get_current_rnd_letter();

/**
 * @brief Turn on/off output to default log channel from ESP-IDF
 * 
 * @param onoff 
 */
void logger_output_to_default(bool onoff);

/**
 * @brief Turn on log output to file
 * 
 * @param filename name of the log (full path)
 * @param protect_start_bytes Protect first X bytes from being overwritten when log file loops around
 * @return bool returns true if succeeds opening the file 
 */
bool logger_output_to_file(const char* filename, long int protect_start_bytes);

/**
 * @brief Output logs over UART port (can be used for external logger)
 * 
 * @param port UART port
 * @param tx_io_num GPIO of TX pin
 * @param rx_io_num GPIO of RX pin
 * @param rts_io_num GPIO of RTS pin
 * @param cts_io_num GPIO of CTS pin
 * @return bool return true if succeeds opening UART
 */
bool logger_output_to_uart(const uart_port_t port, int tx_io_num, int rx_io_num, int rts_io_num, int cts_io_num);

/**
 * @brief Syncs file to storage (should be called before long delay to prevent loosing logs in case of sudden power loss)
 */
void logger_sync_file();

/**
 * @brief Move to begging of log file and start overwriting logs
 */
void logger_set_file_overwrite();

/**
 * @brief Change highest log level that will be written to outputs
 * 
 * @param level new highest log level
 */
void logger_set_log_level(esp_log_level_t level);

/**
 * @brief Write log message to all configured outputs
 * 
 * @param level message log level
 * @param tag tag who generates the log message
 * @param format message format (same as printf)
 * @param ... parameters that will be substituted to format (same as printf)
 */
void logger_write(esp_log_level_t level, const char * tag, const char * format, ...);

/**
 * @brief default log message format
 * 
 * @param letter log level letter
 * @param format body of the log message
 */
#define LOGGER_FORMAT(letter, format) "%c|" #letter " (%d) %s: " format "\r\n"

/**
 * @brief Verbose log message with default format
 * 
 * @param tag log message tag (who generates the message)
 * @param format message format (same as printf)
 * @param ... parameters that will be substituted to format (same as printf)
 */
#define LOGGER_V( tag, format, ...) logger_write(ESP_LOG_VERBOSE, tag, LOGGER_FORMAT(V, format), logger_get_current_rnd_letter(), esp_log_timestamp(), tag, ##__VA_ARGS__);
/** 
 *  @brief Debug log message with default format
 *  @copydetails LOGGER_V 
 */
#define LOGGER_D( tag, format, ...) logger_write(ESP_LOG_DEBUG, tag, LOGGER_FORMAT(D, format), logger_get_current_rnd_letter(), esp_log_timestamp(), tag, ##__VA_ARGS__);
/** 
 *  @brief Info log message with default format
 *  @copydetails LOGGER_V 
 */
#define LOGGER_I( tag, format, ...) logger_write(ESP_LOG_INFO, tag, LOGGER_FORMAT(I, format), logger_get_current_rnd_letter(), esp_log_timestamp(), tag, ##__VA_ARGS__);
/** 
 *  @brief Warning log message with default format
 *  @copydetails LOGGER_V 
 */
#define LOGGER_W( tag, format, ...) logger_write(ESP_LOG_WARN, tag, LOGGER_FORMAT(W, format), logger_get_current_rnd_letter(), esp_log_timestamp(), tag, ##__VA_ARGS__);
/** 
 *  @brief Error log message with default format
 *  @copydetails LOGGER_V 
 */
#define LOGGER_E( tag, format, ...) logger_write(ESP_LOG_ERROR, tag, LOGGER_FORMAT(E, format), logger_get_current_rnd_letter(), esp_log_timestamp(), tag, ##__VA_ARGS__);

/**
 * @brief Read all log file and write it to default output
 * 
 * @return bool true on success
 */
bool logger_dump_log_file();

/**
 * @brief Deletes log file (file must be closed before calling this)
 * 
 * @param filename path to log file
 * @return bool returns true if file was deleted successfully
 */
bool logger_delete_log(const char *filename);

/**
 * @brief get current opened file (be careful not to close it and to return to same position in file)
 * 
 * @return FILE* pointer to opened file
 */
FILE * logger_get_file();

/**
 * @brief Close current log file
 * 
 * @return bool returns true on success 
 */
bool logger_file_close();

/**
 * @brief Close opened storage for log files
 */
void logger_storage_close();

/**
 * @brief Disables all log outputs and closes storage
 */
void logger_stop();

#ifdef __cplusplus
}
#endif

#endif // LOGGER_H_