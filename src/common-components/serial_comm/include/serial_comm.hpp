#ifndef SERIAL_COMM_H_
#define SERIAL_COMM_H_

#include <inttypes.h>
#include <driver/uart.h>
#include <iostream>
#include "serial_comm_common.hpp"

class SerialComm {
    public:
        SerialComm(const uart_port_t port, int tx_io_num, int rx_io_num);
        std::string GetField(std::string field);
        std::string PutField(std::string field, std::string value);
        CommStatus  GetStatus();
        std::string  SendStatus(CommStatus status);
    private:
        std::string SendCmd(CmdType type, std::string field, std::string body);
        uart_port_t uart_port;
        QueueHandle_t uart_queue;
};

#endif