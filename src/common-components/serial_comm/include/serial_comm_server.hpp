#ifndef SERIAL_COMM_SERVER_H_
#define SERIAL_COMM_SERVER_H_

#ifdef CONFIG_SERIAL_COMM_SERVER

#include <inttypes.h>
#include <driver/uart.h>
#include <iostream>
#include "serial_comm_common.hpp"
#include <unordered_map>

class SerialCommSrv {
    public:
        SerialCommSrv(const uart_port_t port, int tx_io_num, int rx_io_num);
        esp_err_t StartTask(){
            if(_xHandle != NULL){
                // delete and start the task again or do nothing
                vTaskDelete(_xHandle);
            }
            // STACK_SIZE=1024*2???
            xTaskCreate(TaskWrapper, "SerialCommSrv", STACK_SIZE, this, configMAX_PRIORITIES, &_xHandle);
        }
        std::string GetField(const std::string& field);
        std::string SetField(const std::string& field, const std::string& value);
        std::string SetStatus(CommStatus status);
    private:
        const std::string& SendResponse(CmdType type, const std::string& field, const std::string& body);
        void Task();
        static TaskWrapper(void* param){
            static_cast<SerialCommSrc *>(param)->Task();
        }
        uart_port_t _uart_port;
        QueueHandle_t _uart_queue;
        CommStatus _current_status;
        TaskHandle_t _xHandle = NULL;
        std::unordered_map<const std::string&, const std::string&> fields;
};

#endif // CONFIG_SERIAL_COMM_SERVER

#endif