#ifndef SERIAL_COMM_H_
#define SERIAL_COMM_H_

#include <inttypes.h>
#include <driver/uart.h>
#include <iostream>

enum class CmdType{
    GET = 0,
    PUT,
    STATUS_GET,
};

enum class CommStatus{
    OK,
    FAIL,
    BOOTLOADER,
};

class SerialComm {
    public:
        SerialComm(const uart_port_t port, int tx_io_num, int rx_io_num);
        std::string SendCmd(CmdType type, std::string field, std::string body);
        std::string GetField(std::string field);
        std::string PutField(std::string field, std::string value);
        CommStatus  GetStatus();
        CommStatus  SendStatus(CommStatus status);
    private:
        uart_port_t uart_port;
        QueueHandle_t uart_queue;
};

#endif