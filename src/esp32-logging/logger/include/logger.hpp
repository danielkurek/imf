#ifndef LOGGER_H_
#define LOGGER_H_

#include <stdarg.h>

namespace LOG {

enum LOG_LEVEL{
    DEBUG,
    INFO,
    WARNING,
    ERROR,
    CRITICAL
}; // or use esp-idf levels

struct log_conf{
  LOG_LEVEL level;
  FILE log_file;
};

void init();
void output_to_file(const char* filename);
void output_to_uart()
void set_log_level(LOG_LEVEL level);

void write(LOG_LEVEL level, const char * format, ...);

void debug(const char * format, ...);
void info(const char * format, ...);
void warning(const char * format, ...);
void error(const char * format, ...);
void critical(const char * format, ...);

}

#endif