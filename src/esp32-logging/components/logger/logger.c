#include "logger.h"
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <esp_log.h>

static struct logger_conf conf;

void logger_init(esp_log_level_t level){
    conf.level = level;
    conf.to_uart = 1;
}
void logger_output_to_file(const char* filename){

}
void logger_output_to_uart(int pin){

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

void logger_close(){
    if(conf.log_file != NULL){
        fclose(conf.log_file);
    }
}