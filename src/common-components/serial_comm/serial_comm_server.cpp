#include "serial_comm_server.hpp"

#ifdef CONFIG_SERIAL_COMM_SERVER

#define RX_BUF_SIZE 1024

SerialCommSrv::SerialCommSrv(const uart_port_t port, int tx_io_num, int rx_io_num){
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

std::string SerialCommSrv::GetField(const std::string& field){
    auto iter = fields.find(field);
    if(iter == fields.end()){
        return "";
    }
    return iter->second;
}

std::string SerialCommSrv::SetField(const std::string& field, const std::string& value){
    fields[field] = value;
}

std::string SerialCommSrv::SetStatus(CommStatus status){
    _current_status = status;
}

ESP_FAIL_t SerialCommSrv::SendGetResponse(const std::string& field){
    auto iter = fields.find(field);
    if(iter == fields.end()){
        const std::string rsp = "FAIL";
        uart_write_bytes(_uart_port, rsp.c_str(), rsp.length() + 1);
        return ESP_FAIL;
    }
    uart_write_bytes(_uart_port, iter->second.c_str(), iter->second.length()+1);
    return ESP_OK;
}

ESP_FAIL_t SerialCommSrv::SendPutResponse(const std::string& field, const std::string& body){
    fields[field] = body;
    uart_write_bytes(_uart_port, body.c_str(), body.length()+1);
    return ESP_OK;
}

ESP_FAIL_t SerialCommSrv::SendStatusResponse(){
    const std::string status = GetStatusName(_current_status);
    uart_write_bytes(_uart_port, status.c_str(), status.length()+1);
    return ESP_OK;
}

ESP_FAIL_t SerialCommSrv::SendResponse(CmdType type, const std::string& field, const std::string& body){
    switch(type){
        case CmdType::GET:
            return SendGetResponse(field, body);
        case CmdType::PUT:
            return SendPutResponse(field, body);
        case CmdType::STATUS:
            return SendStatusResponse();
        default:
            return ESP_FAIL; // unknown CmdType
    }
}

void SerialCommSrv::Task(){
    uint8_t data_buffer = (uint8_t *) malloc(RX_BUF_SIZE+1)

    while(1){
        const int rxBytes = uart_read_bytes(uart_port, data_buffer, RX_BUF_SIZE, 1000 / portTICK_PERIOD_MS);
        if(rxBytes > 0){
            data_buffer[rxBytes] = 0;
            std::string cmd {data_buffer};
            ProcessCmd(cmd);
        }
    }

    free(data_buffer)
}

ESP_FAIL_t SerialCommSrv::ProcessCmd(std::string cmd){
    size_t n = cmd.find("\0");
	if (n != std::string::npos) {
		cmd = cmd.substr(0, n);
	}

    std::istringstream s{ std::move(cmd) };

    std::string command;
    s >> command;
    if(s.fail()){
        return ESP_FAIL;
    }

    for(size_t i = 0; i < command.length(); i++){
        command[i] = std::toupper(command[i]);
    }

    CmdType cmd_type = ParseCmdType(command);

    std::string field;
    s >> field;
    switch(cmd_type){
        case CmdType::STATUS:
            if(!s.fail()) return ESP_FAIL;
            return SendStatusResponse();
        case CmdType::GET:
        case CmdType::PUT:
        default:
            if(s.fail()) return ESP_FAIL;
    }

    std::string body;
    s >> body;

    switch(cmd_type){
        case CmdType::GET:
            if(!s.fail()) return ESP_FAIL;
            return SendGetResponse(field);
        case CmdType::STATUS:
            return ESP_FAIL; // cannot happen
        case CmdType::PUT:
        default:
            if(s.fail()) return ESP_FAIL;
    }

    std::string empty;
    s >> empty;

    switch(cmd_type){
        case CmdType::PUT:
            if(!s.fail()) return ESP_FAIL;
            return SendPutResponse(field, body);
        case CmdType::GET:
        case CmdType::STATUS:
        default:
            return ESP_FAIL; // cannot happen for GET and STATUS
    }

    return ESP_FAIL; // redundant
}

#endif // CONFIG_SERIAL_COMM_SERVER