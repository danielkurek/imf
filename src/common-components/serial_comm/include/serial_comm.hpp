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

class SerialComm{
    public:
        SerialComm(const uart_port_t port, int tx_io_num, int rx_io_num);
        std::string SendCmd(CmdType type, std::string field, std::string body);
    private:
        uart_port_t uart_port;
        QueueHandle_t uart_queue;
};


// bool serial_init(const uart_port_t port, int tx_io_num, int rx_io_num);
// implement async using events
// when message is received dispatch an event to user so that they can read it
// create multiple events
//  - MSG_RECEIVE
//  - MSG_SENT
//  - MSG_STATUS

// returns msg id
// int comm_send_cmd(cmd_type_t type, const char[] field, char* output, const size_t output_len, const char[] body);

// int comm_get_field(const char[] field);

// int comm_put_field(const char[] field, const char[] value);

// int comm_get_status();

// int comm_send_status(int status);

// #ifdef __cplusplus
// }
// #endif

#endif