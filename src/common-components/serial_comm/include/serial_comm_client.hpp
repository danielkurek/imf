#ifndef SERIAL_COMM_CLIENT_H_
#define SERIAL_COMM_CLIENT_H_

#include <inttypes.h>
#include <driver/uart.h>
#include <string>
#include "serial_comm_common.hpp"
#include "freertos/semphr.h"

class SerialCommCli {
    public:
        SerialCommCli(const uart_port_t port, int tx_io_num, int rx_io_num);
        std::string GetField(const std::string& field);
        std::string GetField(uint16_t addr, const std::string& field);
        std::string PutField(const std::string& field, const std::string& value);
        std::string PutField(uint16_t addr, const std::string& field, const std::string& value);
        CommStatus  GetStatus();
        std::string SendStatus(CommStatus status);
    private:
        constexpr static size_t rx_buffer_len = 128;
        uint8_t _buf[rx_buffer_len];
        
        std::string SendCmd(CmdType type, const std::string& field, const std::string& body);
        std::string GetResponse();
        SemaphoreHandle_t _semMutex;
        uart_port_t _uart_port;
        QueueHandle_t _uart_queue;
};

#endif