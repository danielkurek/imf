/**
 * @file serial_comm_server.cpp
 * @author Daniel Kurek (daniel.kurek.dev@gmail.com)
 * @brief Implementation of @ref serial_comm_server.hpp
 * @version 0.1
 * @date 2023-10-22
 * 
 * @copyright Copyright (c) 2023
 * 
 */
#include "serial_comm_server.hpp"
#include "serial_comm_common.hpp"
#include "esp_log.h"
#include <sstream>
#include <string>

using namespace com;

static const char *TAG = "SerialSrv";

SerialCommSrv::SerialCommSrv(const uart_port_t port, int tx_io_num, int rx_io_num, uint16_t default_addr)
     : SerialComm(port, tx_io_num, rx_io_num), _default_addr(default_addr){
    _semMutex = xSemaphoreCreateMutex();
}

esp_err_t SerialCommSrv::GetField(uint16_t addr, const std::string& field, std::string& out){
    if(pdTRUE != xSemaphoreTake(_semMutex, 500 / portTICK_PERIOD_MS)){
        return ESP_FAIL;
    }
    esp_err_t err = ESP_OK;
    auto iter = fields.find(addr);
    if(iter != fields.end()){
        auto iter2 = iter->second.find(field);
        if(iter2 == iter->second.end()){
            err = ESP_FAIL;
        } else{
            out = iter2->second;
        }
    } else{
        err = ESP_FAIL;
    }
    xSemaphoreGive(_semMutex);
    return err;
}

esp_err_t SerialCommSrv::SetField(uint16_t addr, const std::string& field, const std::string& value, bool do_callback){
    if(pdTRUE != xSemaphoreTake(_semMutex, 500 / portTICK_PERIOD_MS)){
        return ESP_FAIL;
    }
    auto iter = fields.find(addr);
    if(iter == fields.end()){
        fields[addr] = {};
    }
    fields[addr][field] = value;

    xSemaphoreGive(_semMutex);

    if(do_callback && _change_callback != nullptr){
        _change_callback(addr, field, value);
    }

    _sendField(addr, field);

    return ESP_OK;
}

esp_err_t SerialCommSrv::_sendField(uint16_t addr, const std::string& field_name){
    std::string value;
    esp_err_t err = GetField(addr, field_name, value);
    if(err != ESP_OK){
        return err;
    }
    std::string field;
    err = MakeField(addr, field_name, field);
    if(err != ESP_OK){
        return err;
    }
    SerialResponse resp{.field=field, .value=value};
    return writeResponse(resp);
}

void SerialCommSrv::processInput(const std::string& input){
    SerialRequest req;
    esp_err_t err = SerialRequest::parse(input, req);
    if(err != ESP_OK){
        ESP_LOGE(TAG, "Could not parse cmd '%s'", input.c_str());
    }
    std::string field;
    uint16_t addr;
    FieldParseErr f_err = ParseField(req.field, field, addr);
    switch(f_err){
        case FieldParseErr::no_addr:
            if(field == "addr"){
                std::string addr_str;
                err = AddrToStr(_default_addr, addr_str);
                if(err == ESP_OK){
                    SerialResponse resp{.field=field, .value=addr_str};
                    writeResponse(resp);
                    return;
                }
            }
            addr = _default_addr;
            break;
        case FieldParseErr::ok:
            break;
        case FieldParseErr::malformed_addr:
            ESP_LOGE(TAG, "Error during parsing field '%s': malformed_addr", req.field.c_str());
            break;
        case FieldParseErr::empty_field:
            ESP_LOGE(TAG, "Error during parsing field '%s': empty_field", req.field.c_str());
            break;
        case FieldParseErr::empty_field_name:
            ESP_LOGE(TAG, "Error during parsing field '%s': empty_field_name", req.field.c_str());
            break;
        default:
            ESP_LOGE(TAG, "Could not parse Field '%s'!", req.field.c_str());
            return;
    }
    switch(req.type){
        case CmdType::GET:
            if(_get_callback){
                _get_callback(addr, field);
            } else{
                _sendField(addr, field);
            }
            break;
        case CmdType::PUT:
            if(_change_callback){
               _change_callback(addr, field, req.value);
            } else{
                SetField(addr, field, req.value);
            }
            break;
        default:
            ESP_LOGE(TAG, "process input unknown command type");
            return;
    }
}

// #endif // CONFIG_SERIAL_COMM_SERVER