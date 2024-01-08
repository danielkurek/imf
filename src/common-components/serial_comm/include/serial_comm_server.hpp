#ifndef SERIAL_COMM_SERVER_H_
#define SERIAL_COMM_SERVER_H_

#include <inttypes.h>
#include <driver/uart.h>
#include <string>
#include "serial_comm_common.hpp"
#include <unordered_map>

typedef void (*serial_comm_change_cb)(uint16_t addr, const std::string& field, const std::string& value);
typedef void (*serial_comm_get_cb)(uint16_t addr, const std::string& field);

class SerialCommSrv {
    public:
        SerialCommSrv(const uart_port_t port, int tx_io_num, int rx_io_num, uint16_t default_addr);
        esp_err_t StartTask(){
            if(_xHandle != NULL){
                // delete and start the task again or do nothing
                vTaskDelete(_xHandle);
            }
            // STACK_SIZE=1024*2???
            xTaskCreate(TaskWrapper, "SerialCommSrv", 1024*8, this, configMAX_PRIORITIES, &_xHandle);
            return ESP_OK;
        }
        esp_err_t GetField(uint16_t addr, const std::string& field, std::string& out);
        esp_err_t SetField(uint16_t addr, const std::string& field, const std::string& value);
        esp_err_t SetStatus(CommStatus status);
        void RegisterChangeCallback(serial_comm_change_cb change_cb, serial_comm_get_cb get_cb){
            _change_callback = change_cb;
            _get_callback = get_cb;
        }
    private:
        esp_err_t SendGetResponse(uint16_t addr, const std::string& field);
        esp_err_t SendPutResponse(uint16_t addr, const std::string& field, const std::string& body);
        esp_err_t SendStatusResponse();
        esp_err_t SendResponse(CmdType type, uint16_t addr, const std::string& field, const std::string& body);
        esp_err_t ProcessInput(const std::string& input);
        esp_err_t ProcessCmd(std::string& cmd);
        esp_err_t ProcessField(std::string& field, uint16_t& addr);
        void Task();
        static void TaskWrapper(void* param){
            static_cast<SerialCommSrv *>(param)->Task();
        }
        uart_port_t _uart_port;
        uint16_t _default_addr;
        QueueHandle_t _uart_queue;
        CommStatus _current_status;
        TaskHandle_t _xHandle = NULL;
        std::unordered_map<uint16_t, std::unordered_map<std::string, std::string>> fields;
        serial_comm_change_cb _change_callback = nullptr;
        serial_comm_get_cb _get_callback = nullptr;
};

#endif