#include "serial_comm_server.hpp"
#include "serial_comm_common.hpp"
#include "esp_log.h"
#include <sstream>
#include <string>

static const char *TAG = "SerialSrv";

// #ifdef CONFIG_SERIAL_COMM_SERVER

#define RX_BUF_SIZE 1024

SerialCommSrv::SerialCommSrv(const uart_port_t port, int tx_io_num, int rx_io_num, uint16_t default_addr){
    // TODO: handle errors
    _uart_port = port;
    _default_addr = default_addr;

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
        // return false;
    }

    if(uart_set_pin(port, tx_io_num, rx_io_num, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE) != ESP_OK){
        ESP_LOGE(TAG, "cannot set UART pins");
        // return false;
    }

    const int uart_buffer_size = (1024 * 2);
    if(uart_driver_install(port, uart_buffer_size, uart_buffer_size, 10, &_uart_queue, 0) != ESP_OK){
        ESP_LOGE(TAG, "cannot install UART driver");
        // return false;
    }
    
    // return true;
}

esp_err_t SerialCommSrv::GetField(uint16_t addr, const std::string& field, std::string& out){
    auto iter = fields.find(addr);
    if(iter == fields.end()){
        return ESP_FAIL;
    }
    auto iter2 = iter->second.find(field);
    if(iter2 == iter->second.end()){
        return ESP_FAIL;
    }
    out = iter2->second;
    return ESP_OK;
}

esp_err_t SerialCommSrv::SetField(uint16_t addr, const std::string& field, const std::string& value){
    auto iter = fields.find(addr);
    if(iter == fields.end()){
        fields[addr] = {};
    }
    fields[addr][field] = value;

    if(_change_callback != nullptr){
        _change_callback(addr, field, value);
    }

    return ESP_OK;
}

esp_err_t SerialCommSrv::SetStatus(CommStatus status){
    _current_status = status;
    return ESP_OK;
}

esp_err_t SerialCommSrv::SendGetResponse(uint16_t addr, const std::string& field){
    std::string value;
    esp_err_t err = GetField(addr, field, value);
    // if no value is in cache, try to fetch it
    if(err != ESP_OK && _get_callback != nullptr){
        _get_callback(addr, field);
        err = GetField(addr, field, value);
    }
    
    if(err != ESP_OK){
        const std::string rsp = "FAIL\n";
        ESP_LOGI(TAG, "Sending GET response: %s", rsp.c_str());
        uart_write_bytes(_uart_port, rsp.c_str(), rsp.length());
        
        return ESP_FAIL;
    }
    value += "\n";
    uart_write_bytes(_uart_port, value.c_str(), value.length());
    ESP_LOGI(TAG, "Sending GET response: %s", value.c_str());
    return ESP_OK;
}

esp_err_t SerialCommSrv::SendPutResponse(uint16_t addr, const std::string& field, const std::string& body){
    SetField(addr, field, body);
    std::string response = body + "\n";
    uart_write_bytes(_uart_port, response.c_str(), response.length());
    ESP_LOGI(TAG, "Sending PUT response: %s", response.c_str());
    return ESP_OK;
}

esp_err_t SerialCommSrv::SendStatusResponse(){
    const std::string status = GetStatusName(_current_status) + "\n";
    uart_write_bytes(_uart_port, status.c_str(), status.length());
    ESP_LOGI(TAG, "Sending STATUS response: %s", status.c_str());
    return ESP_OK;
}

esp_err_t SerialCommSrv::SendResponse(CmdType type, uint16_t addr, const std::string& field, const std::string& body){
    switch(type){
        case CmdType::GET:
            return SendGetResponse(addr, field);
        case CmdType::PUT:
            return SendPutResponse(addr, field, body);
        case CmdType::STATUS:
            return SendStatusResponse();
        default:
            return ESP_FAIL; // unknown CmdType
    }
}

void SerialCommSrv::Task(){
    uint8_t* data_buffer = (uint8_t *) malloc(RX_BUF_SIZE+1);

    while(1){
        const int rxBytes = uart_read_bytes(_uart_port, data_buffer, RX_BUF_SIZE, 500 / portTICK_PERIOD_MS);
        if(rxBytes > 0){
            data_buffer[rxBytes] = 0;
            ESP_LOGI(TAG, "Received: %s", data_buffer);
            std::string input {(char*) data_buffer};
            ProcessInput(input);
        }
    }

    free(data_buffer);
}

// Split input on '\n' char and pass it to ProcessCmd (without the '\n')
esp_err_t SerialCommSrv::ProcessInput(const std::string& input){
    std::string::size_type start_pos = 0;
    std::string::size_type end_pos = input.find_first_of('\n');
    esp_err_t ret = ESP_OK;
    while(true){
        std::string::size_type count = end_pos - start_pos;
        if(end_pos == input.npos){
            count = input.size() - start_pos;
        }
        std::string cmd = input.substr(start_pos, count);
        esp_err_t err = ProcessCmd(cmd);
        if(err != ESP_OK){
            ret = err;
        }

        if(end_pos == input.npos || end_pos == input.size()-1){
            break;
        }

        start_pos = end_pos + 1; // skip '\n' char
        end_pos = input.find_first_of('\n');
    }
    return ret;
}

esp_err_t SerialCommSrv::ProcessField(std::string& field, uint16_t& addr){
    auto pos = field.find_first_of(':');
    if(pos != std::string::npos){
        if(pos != 4){
            ESP_LOGE(TAG, "While processing CMD, address is malformed");
            return ESP_FAIL;
        }
        esp_err_t err = StrToAddr(field.substr(0, pos).c_str(), &addr);
        if(err != ESP_OK){
            ESP_LOGE(TAG, "While processing CMD, could not parse the address");
            return err;
        }
        field = field.substr(pos+1);
    } else{
        addr = _default_addr;
    }
    return ESP_OK;
}

// TODO: rewrite as separate functions for each command type

esp_err_t SerialCommSrv::ProcessCmd(std::string& cmd){
    ESP_LOGI(TAG, "Processing cmd: %s", cmd.c_str());

    // size_t n = cmd.find("\0");
	// if (n != std::string::npos) {
    //     ESP_LOGI(TAG, "Processing cmd: Found null byte, trimming string to %d", n);
	// 	cmd = cmd.substr(0, n);
	// }

    ESP_LOGI(TAG, "Processing cmd: %s", cmd.c_str());

    std::istringstream s {cmd};

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

    uint16_t addr;

    std::string field;
    s >> field;
    switch(cmd_type){
        case CmdType::STATUS:
            ESP_LOGE(TAG, "While processing CMD, STATUS command too many arguments");
            return ESP_FAIL;
        case CmdType::GET:
        case CmdType::PUT:
            if(s.fail()){
                ESP_LOGE(TAG, "While processing CMD, not enough arguments");
                return ESP_FAIL;
            }
            if(ProcessField(field, addr) != ESP_OK){
                ESP_LOGE(TAG, "While processing CMD, could not parse field");
                return ESP_FAIL;
            }
            break;
        default:
            ESP_LOGE(TAG, "While processing CMD, unknown command");
            return ESP_FAIL;
    }

    std::string body;
    s >> body;

    switch(cmd_type){
        case CmdType::GET:
            if(!s.fail())
            {
                ESP_LOGE(TAG, "While processing CMD, GET cmd has more arguments, expected 2");
                return ESP_FAIL;
            }
            return SendGetResponse(addr, field);
        case CmdType::STATUS:
            ESP_LOGE(TAG, "While processing CMD, STATUS cmd has more arguments, expected 0");
            return ESP_FAIL; // cannot happen
        case CmdType::PUT:
            if(s.fail())
            {
                ESP_LOGE(TAG, "While processing CMD, PUT cmd - not enough arguments");
                return ESP_FAIL;
            }
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
            return SendPutResponse(addr, field, body);
        case CmdType::GET:
        case CmdType::STATUS:
        default:
            ESP_LOGE(TAG, "While processing CMD, unknown command or failed to parse");
            return ESP_FAIL; // cannot happen for GET and STATUS
    }

    return ESP_FAIL; // redundant
}

// #endif // CONFIG_SERIAL_COMM_SERVER