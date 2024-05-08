#include "serial_comm_common.hpp"
#include <cinttypes>
#include <sstream>
#include <cstring>
#include "esp_log.h"

#define RX_BUF_SIZE 512

using namespace com;

static const char* TAG = "SerialComm";

std::string com::GetCmdName(CmdType type) {
    switch (type)
    {
    case CmdType::GET:
        return "GET";
        break;
    case CmdType::PUT:
        return "PUT";
        break;
    case CmdType::STATUS:
        return "STATUS";
        break;
    default:
        return "UNKWN";
        break;
    }
}

CmdType com::ParseCmdType(const std::string& cmdType) {
    if(cmdType == "GET") {
        return CmdType::GET;
    }
    if(cmdType == "PUT") {
        return CmdType::PUT;
    }
    if(cmdType == "STATUS") {
        return CmdType::STATUS;
    }
    return CmdType::None;
}

esp_err_t com::AddrToStr(uint16_t addr, std::string& out){
    char buf[addr_str_len];
    int ret = snprintf(buf, addr_str_len, "%04" PRIx16, addr);
    if(ret > 0){
        out = std::string(buf);
        return ESP_OK;
    }
    return ESP_FAIL;
}

esp_err_t com::StrToAddr(const char *addrStr, uint16_t *addrOut){
    errno = 0;
    char *end;
    unsigned long int result = strtoul(addrStr, &end, 16);
    if(addrStr == end) return ESP_FAIL;
    if(errno == ERANGE) return ESP_FAIL;
    if(result > UINT16_MAX) return ESP_FAIL;
    
    (*addrOut) = (uint16_t) result;
    return ESP_OK;
}

esp_err_t com::MakeField(uint16_t addr, const std::string& field, std::string& out){
    if(field.length() <= 0) return ESP_FAIL;

    std::string addr_str;
    esp_err_t err = AddrToStr(addr, addr_str);
    if(err != ESP_OK) return err;

    out = addr_str + ":" + field;
    return ESP_OK;
}

FieldParseErr com::ParseField(const std::string& input, std::string& field, uint16_t& addr){
    if(input.length() <= 0) return FieldParseErr::empty_field;
    auto pos = input.find_first_of(':');
    if(pos != std::string::npos){
        if(pos != 4) {
            ESP_LOGE(TAG, "':' is at pos %d (expected 4)", pos);
            return FieldParseErr::malformed_addr;
            }
        std::string addr_str = input.substr(0, pos);
        esp_err_t err = StrToAddr(addr_str.c_str(), &addr);
        if(err != ESP_OK) {
            ESP_LOGE(TAG, "Could not parse addr '%s'", addr_str.c_str());
            return FieldParseErr::malformed_addr;
        }
        
        if(pos == input.length()-1) return FieldParseErr::empty_field_name;
        field = input.substr(pos+1);
    } else{
        field = input;
        return FieldParseErr::no_addr;
    }
    return FieldParseErr::ok;
}

std::string SerialRequest::toString() const{
    return GetCmdName(type) + " " + field + " " + value;
}

esp_err_t SerialRequest::parse(const std::string& input, SerialRequest& out){
    std::istringstream s {input};

    std::string command;
    s >> command;
    if(s.fail()){
        ESP_LOGE(TAG, "While processing CMD, cannot get command type");
        return ESP_FAIL;
    }

    for(size_t i = 0; i < command.length(); i++){
        command[i] = std::toupper(command[i]);
    }
    ESP_LOGI(TAG, "CMD part: %s", command.c_str());
    CmdType cmd_type = ParseCmdType(command);
    out.type = cmd_type;

    std::string field;
    s >> field;
    switch(cmd_type){
        case CmdType::GET:
        case CmdType::PUT:
            if(s.fail()){
                ESP_LOGE(TAG, "While processing CMD, not enough arguments");
                return ESP_FAIL;
            }
            out.field = field;
            break;
        default:
            ESP_LOGE(TAG, "While processing CMD, unknown command");
            return ESP_FAIL;
    }

    std::string value;
    s >> value;

    switch(cmd_type){
        case CmdType::GET:
            if(!s.fail())
            {
                ESP_LOGE(TAG, "While processing CMD, GET cmd has more arguments, expected 2");
                return ESP_FAIL;
            }
            return ESP_OK;
        case CmdType::PUT:
            if(s.fail())
            {
                ESP_LOGE(TAG, "While processing CMD, PUT cmd - not enough arguments");
                return ESP_FAIL;
            }
            out.value = value;
            break;
        default:
            ESP_LOGE(TAG, "While processing CMD, unknown command");
            return ESP_FAIL;
    }

    std::string empty;
    s >> empty;

    switch(cmd_type){
        case CmdType::PUT:
            if(!s.fail())
            {
                ESP_LOGE(TAG, "%s", empty.c_str());
                ESP_LOGE(TAG, "While processing CMD, PUT cmd has more arguments, expected 3");
                return ESP_FAIL;
            }
            return ESP_OK;
        case CmdType::GET:
        default:
            ESP_LOGE(TAG, "While processing CMD, unknown command or failed to parse");
            return ESP_FAIL; // cannot happen for GET and STATUS
    }

    return ESP_FAIL; // redundant
}

std::string SerialResponse::toString() const{
    return field + "=" + value;
}

esp_err_t SerialResponse::parse(const std::string& input, SerialResponse& out){
    auto end_pos = input.find_first_of('=');
    if(end_pos == input.npos){
        return ESP_FAIL;
    }
    out.field = input.substr(0, end_pos);
    auto start_pos = end_pos + 1;
    if(start_pos < input.size()){
        out.value = input.substr(start_pos, input.npos);
    } else{
        out.value = "";
    }
    return ESP_OK;
}

SerialComm::SerialComm(const uart_port_t port, int tx_io_num, int rx_io_num, char sep){
    _uart_port = port;
    _sep = sep;

    uart_config_t uart_config = {};
    uart_config.baud_rate = 115200;
    uart_config.data_bits = UART_DATA_8_BITS;
    uart_config.parity = UART_PARITY_DISABLE;
    uart_config.stop_bits = UART_STOP_BITS_1;
    uart_config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    uart_config.rx_flow_ctrl_thresh = 122;
    uart_config.source_clk = UART_SCLK_DEFAULT;
    
    if(uart_param_config(port, &uart_config) != ESP_OK){
        ESP_LOGE(TAG, "cannot set UART params");
        abort();
    }

    if(uart_set_pin(port, tx_io_num, rx_io_num, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE) != ESP_OK){
        ESP_LOGE(TAG, "cannot set UART pins");
        abort();
    }

    const int uart_buffer_size = (1024 * 2);
    if(uart_driver_install(port, uart_buffer_size, 0, 10, &_uart_queue, 0) != ESP_OK){
        ESP_LOGE(TAG, "cannot install UART driver");
        abort();
    }
    _semMutex = xSemaphoreCreateMutex();
    // return true;
}

esp_err_t SerialComm::startReadTask(){
    if(_xHandle != NULL){
        // delete and start the task again or do nothing
        stopReadTask();
    }
    // STACK_SIZE=1024*2???
    auto ret = xTaskCreate(readTaskWrapper, "SerialRead", 1024*8, this, tskIDLE_PRIORITY+1, &_xHandle);
    if(ret != pdPASS){
        ESP_LOGE(TAG, "Could not create SerialRead task");
        return ESP_FAIL;
    }
    return ESP_OK;
}

void SerialComm::stopReadTask(){
    if(_xHandle != NULL){
        vTaskDelete(_xHandle);
        _xHandle = NULL;
    }
}
esp_err_t SerialComm::writeRequest(const SerialRequest& req){
    std::string cmdString = req.toString() + _sep;
    return write(cmdString);
}

esp_err_t SerialComm::writeResponse(const SerialResponse& res){
    std::string cmdString = res.toString() + _sep;
    return write(cmdString);
}

esp_err_t SerialComm::write(const std::string& data){
    if(pdTRUE != xSemaphoreTake(_semMutex, 10000 / portTICK_PERIOD_MS)){
        ESP_LOGE(TAG, "SendCmd could not get semaphore mutex! %s", data.c_str());
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Sending cmd: %s", data.c_str());
    uart_write_bytes(_uart_port, data.c_str(), data.length());

    xSemaphoreGive(_semMutex);
    return ESP_OK;
}

void SerialComm::readTask(){
    uint8_t* data_buffer = (uint8_t *) malloc(RX_BUF_SIZE+1);
    size_t start_pos = 0;

    while(1){
        assert(start_pos <= RX_BUF_SIZE);
        const int rxBytes = uart_read_bytes(_uart_port, data_buffer + start_pos, RX_BUF_SIZE - start_pos, 10 / portTICK_PERIOD_MS);
        if(rxBytes > 0){
            assert(start_pos + rxBytes <= RX_BUF_SIZE);
            data_buffer[start_pos + rxBytes] = 0;

            ESP_LOGI(TAG, "Received: %s", data_buffer);
            std::string input {(char*) data_buffer};
            size_t end_pos = processInputs(input);
            if(end_pos >= input.size()){
                start_pos = 0;
            } else{
                // end_pos < input.size()
                start_pos = end_pos + 1;
                const char* unread_data = input.c_str() + end_pos;
                // do not use strlen() + 1 because we omit null byte
                memcpy(data_buffer, unread_data, strlen(unread_data));
            }
        }
        // if buffer is almost full after processing it, discard it
        // this prevents long command from filling up the buffer
        if((RX_BUF_SIZE+1) - start_pos < 10){
            start_pos = 0;
        }
    }

    free(data_buffer);
    vTaskDelete(_xHandle);
}

int SerialComm::processInputs(const std::string& input){
    std::string::size_type start_pos = 0;
    std::string::size_type end_pos = input.find_first_of(_sep);
    while(end_pos == input.npos || end_pos < input.size()){
        std::string::size_type count = end_pos - start_pos;
        if(end_pos == input.npos){
            return start_pos;
            count = input.size() - start_pos;
        }
        std::string cmd = input.substr(start_pos, count);
        processInput(cmd);

        start_pos = end_pos + 1; // skip '\n' char
        if(start_pos >= input.size()){
            return input.size();
        }
        end_pos = input.find_first_of('\n', start_pos);
    }
    if(end_pos == input.npos){
        return start_pos;
    }
    return end_pos + 1;
}