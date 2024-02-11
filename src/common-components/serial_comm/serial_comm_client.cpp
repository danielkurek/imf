// #ifdef CONFIG_SERIAL_COMM_CLIENT

#include "serial_comm_client.hpp"
#include "esp_log.h"
#include <string>

static const char *TAG = "SerialCli";

SerialCommCli::SerialCommCli(const uart_port_t port, int tx_io_num, int rx_io_num){
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
    
    // return true;
}

std::string SerialCommCli::SendCmd(CmdType type, const std::string& field, const std::string& body){
    std::string cmdString = GetCmdName(type);

    // maybe sanitize the parameters similar to CSV sanitization
    if(field.length() > 0){
        cmdString += " " + field;
    } else if(body.length() > 0){
        return "FAIL";
    }
    if(body.length() > 0){
        cmdString += " " + body;
    }
    cmdString += "\n";
    ESP_LOGI(TAG, "Sending cmd: %s", cmdString.c_str());
    uart_write_bytes(_uart_port, cmdString.c_str(), cmdString.length());
    ESP_LOGI(TAG, "Waiting for TX done...");
    uart_wait_tx_done(_uart_port, 1000 / portTICK_PERIOD_MS);
    ESP_LOGI(TAG, "Awaiting response...");
    return GetResponse();
}

std::string SerialCommCli::GetResponse(){
    uint8_t buf[rx_buffer_len];
    int len = uart_read_bytes(_uart_port, buf, rx_buffer_len, 800 / portTICK_PERIOD_MS);
    if(len > 0){
        buf[len] = '\0';

        // remove trailing new line
        if(len-1 > 0 && buf[len-1] == '\n'){
            buf[len-1] = '\0';
        }

        // skip previous timeouted responses
        size_t start = 0;
        for(size_t i = 0; i < len-1; i++){
            if(buf[i] == '\n'){
                start = i+1;
            }
        }

        ESP_LOGI(TAG, "Response (len=%d, start=%d): %s", len, start, buf+start);
        std::string response{(char*)buf+start};
        return response;
    }
    ESP_LOGW(TAG, "Could not get a response...");
    return "";
}

std::string SerialCommCli::GetField(const std::string& field){
    return SendCmd(CmdType::GET, field, "");
}

std::string SerialCommCli::GetField(uint16_t addr, const std::string& field){
    std::string addr_str;
    esp_err_t err = AddrToStr(addr, addr_str);
    if(err != ESP_OK){
        return "";
    }
    return SendCmd(CmdType::GET, addr_str + ":" + field, "");
}

std::string SerialCommCli::PutField(const std::string& field, const std::string& value){
    return SendCmd(CmdType::PUT, field, value);
}

std::string SerialCommCli::PutField(uint16_t addr, const std::string& field, const std::string& value){
    std::string addr_str;
    esp_err_t err = AddrToStr(addr, addr_str);
    if(err != ESP_OK){
        return "";
    }
    return SendCmd(CmdType::PUT, addr_str + ":" + field, value);
}

CommStatus SerialCommCli::GetStatus(){
    std::string result = SendCmd(CmdType::STATUS, "", "");
    return ParseStatus(result);
}

std::string SerialCommCli::SendStatus(CommStatus status){
    return SendCmd(CmdType::STATUS, GetStatusName(status), "");
}

// #endif //CONFIG_SERIAL_COMM_CLIENT