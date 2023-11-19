// #ifdef CONFIG_SERIAL_COMM_CLIENT

#include "serial_comm_client.hpp"
#include "esp_log.h"
#include <string>

static const char *TAG = "SerialCli";

SerialCommCli::SerialCommCli(const uart_port_t port, int tx_io_num, int rx_io_num){
    // TODO: handle errors
    _uart_port = port;

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

std::string SerialCommCli::SendCmd(CmdType type, uint16_t addr, std::string field, std::string body){
    std::string cmdString = GetCmdName(type);

    if(type != CmdType::STATUS){
        cmdString += " " + AddrToStr(addr);
    }
    // maybe sanitize the parameters similar to CSV sanitization
    if(field.length() > 0){
        cmdString += " " + field;
    } else if(body.length() > 0){
        // error: field is not specified
    }
    if(body.length() > 0){
        cmdString += " " + body;
    }

    // length() + 1 because of \0 at the end
    ESP_LOGI(TAG, "Sending cmd");
    uart_write_bytes(_uart_port, cmdString.c_str(), cmdString.length()+1);
    ESP_LOGI(TAG, "Awaiting response...");
    return GetResponse();
}

std::string SerialCommCli::GetResponse(){
    uint8_t buf[rx_buffer_len];
    int len = uart_read_bytes(_uart_port, buf, rx_buffer_len, 500 / portTICK_PERIOD_MS);
    if(len > 0){
        buf[len] = '\0';
        std::string response{(char*)buf};
        return response;
    }
    return "";
}

std::string SerialCommCli::GetField(uint16_t addr, std::string field){
    return SendCmd(CmdType::GET, addr, field, "");
}

std::string SerialCommCli::PutField(uint16_t addr, std::string field, std::string value){
    return SendCmd(CmdType::PUT, addr, field, value);
}

CommStatus SerialCommCli::GetStatus(){
    std::string result = SendCmd(CmdType::STATUS, 0, "", "");
    return ParseStatus(result);
}

std::string SerialCommCli::SendStatus(CommStatus status){
    return SendCmd(CmdType::STATUS, 0, GetStatusName(status), "");
}

// #endif //CONFIG_SERIAL_COMM_CLIENT