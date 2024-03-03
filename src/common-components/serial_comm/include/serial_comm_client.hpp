#ifndef SERIAL_COMM_CLIENT_H_
#define SERIAL_COMM_CLIENT_H_

#include <inttypes.h>
#include <driver/uart.h>
#include <string>
#include "serial_comm_common.hpp"
#include "freertos/semphr.h"
#include <unordered_map>

class SerialCommCli : public SerialComm {
    public:
        SerialCommCli(const uart_port_t port, int tx_io_num, int rx_io_num);
        std::string GetField(const std::string& field);
        std::string GetField(uint16_t addr, const std::string& field_name);
        esp_err_t PutField(const std::string& field, const std::string& value);
        esp_err_t PutField(uint16_t addr, const std::string& field_name, const std::string& value);
    private:
        void processInput(const std::string& input) override;
        std::unordered_map<std::string, std::string> cache{};
        SemaphoreHandle_t _semMutex;
};

#endif