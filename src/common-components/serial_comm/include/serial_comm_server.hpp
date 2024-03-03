#ifndef SERIAL_COMM_SERVER_H_
#define SERIAL_COMM_SERVER_H_

#include <inttypes.h>
#include <driver/uart.h>
#include <string>
#include "serial_comm_common.hpp"
#include <unordered_map>

typedef void (*serial_comm_change_cb)(uint16_t addr, const std::string& field, const std::string& value);
typedef void (*serial_comm_get_cb)(uint16_t addr, const std::string& field);

class SerialCommSrv : public SerialComm {
    public:
        SerialCommSrv(const uart_port_t port, int tx_io_num, int rx_io_num, uint16_t default_addr);
        esp_err_t GetField(uint16_t addr, const std::string& field, std::string& out);
        esp_err_t SetField(uint16_t addr, const std::string& field, const std::string& value, bool do_callback = true);
        void RegisterChangeCallback(serial_comm_change_cb change_cb, serial_comm_get_cb get_cb){
            _change_callback = change_cb;
            _get_callback = get_cb;
        }
        void processInput(const std::string& input) override;
    private:
        esp_err_t _sendField(uint16_t addr, const std::string& field_name);
        uint16_t _default_addr;
        std::unordered_map<uint16_t, std::unordered_map<std::string, std::string>> fields;
        serial_comm_change_cb _change_callback = nullptr;
        serial_comm_get_cb _get_callback = nullptr;
        SemaphoreHandle_t _semMutex;
};

#endif