#ifndef SERIAL_COMM_COMMON_H_
#define SERIAL_COMM_COMMON_H_

#include <string>
#include "esp_err.h"
#include <functional>
#include <driver/uart.h>
#include "freertos/semphr.h"
#include "freertos/FreeRTOS.h"

#define ADDR_STR_LEN (4+1)

enum class CmdType{
    None = 0,
    GET,
    PUT,
    STATUS,
};

enum class CommStatus{
    None = 0,
    OK,
    FAIL,
    BOOTLOADER,
};

enum class FieldParseErr{
    ok = 0,
    no_addr,
    malformed_addr,
    empty_field,
    empty_field_name,
};

std::string GetCmdName(CmdType type);

CmdType ParseCmdType(const std::string& cmdType);

std::string GetStatusName(CommStatus type);

CommStatus ParseStatus(const std::string& status);

esp_err_t AddrToStr(uint16_t addr, std::string& out);

esp_err_t StrToAddr(const char *addrStr, uint16_t *addrOut);

esp_err_t MakeField(uint16_t addr, const std::string& field, std::string& out);
FieldParseErr ParseField(const std::string& input, std::string& field, uint16_t& addr);

struct SerialRequest{
    CmdType type;
    std::string field;
    std::string value;
    std::string toString() const;
    static esp_err_t parse(const std::string& input, SerialRequest& out);
};
struct SerialResponse{
    std::string field;
    std::string value;
    std::string toString() const;
    static esp_err_t parse(const std::string& input, SerialResponse& out);
};

class SerialComm{
    public:
        SerialComm(const uart_port_t port, int tx_io_num, int rx_io_num, char sep='\n');
        esp_err_t startReadTask();
        void stopReadTask();
        esp_err_t writeRequest(const SerialRequest& req);
        esp_err_t writeResponse(const SerialResponse& res);
        virtual void processInput(const std::string&) = 0;
    private:
        esp_err_t write(const std::string& data);
        static void readTaskWrapper(void* param){
            static_cast<SerialComm *>(param)->readTask();
        }
        void readTask();
        int processInputs(const std::string& input);
        SemaphoreHandle_t _semMutex;
        TaskHandle_t _xHandle = NULL;
        uart_port_t _uart_port;
        QueueHandle_t _uart_queue;
        char _sep;
};

#endif