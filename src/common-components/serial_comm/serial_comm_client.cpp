// #ifdef CONFIG_SERIAL_COMM_CLIENT

#include "serial_comm_client.hpp"
#include "esp_log.h"
#include <string>

using namespace com;

static const char *TAG = "SerialCli";

SerialCommCli::SerialCommCli(const uart_port_t port, int tx_io_num, int rx_io_num, const TickType_t cache_threshold)
    : SerialComm(port, tx_io_num, rx_io_num), _cacheThreshold(cache_threshold){
    _semMutex = xSemaphoreCreateMutex();
}

std::string SerialCommCli::GetField(const std::string& field){
    TickType_t arrivalTime = 0;
    if(pdTRUE != xSemaphoreTake(_semMutex, 500 / portTICK_PERIOD_MS)){
        return "FAIL";
    }
    TickType_t now = xTaskGetTickCount();
    std::string resp = "FAIL";
    auto it = cache.find(field);
    if(it == cache.end()){
        ESP_LOGW(TAG, "Could not get field '%s'", field.c_str());
    } else{
        arrivalTime = it->second.arrivalTime;
        resp = it->second.value;
    }
    if((now - arrivalTime) >= _cacheThreshold){
        SerialRequest req{.type=CmdType::GET, .field=field, .value=""};
        writeRequest(req);
    }
    xSemaphoreGive(_semMutex);
    return resp;
}

std::string SerialCommCli::GetField(uint16_t addr, const std::string& field_name){
    std::string field;
    esp_err_t err = MakeField(addr, field_name, field);
    if(err != ESP_OK){
        return "";
    }
    return GetField(field);
}

esp_err_t SerialCommCli::PutField(const std::string& field, const std::string& value){
    SerialRequest req{.type=CmdType::PUT, .field=field, .value=value};
    return writeRequest(req);
}

esp_err_t SerialCommCli::PutField(uint16_t addr, const std::string& field_name, const std::string& value){
    std::string field;
    esp_err_t err = MakeField(addr, field_name, field);
    if(err != ESP_OK){
        return ESP_FAIL;
    }
    //TODO: either save to cache or wait for confirmation
    return PutField(field, value);
}

void SerialCommCli::processInput(const std::string& input){
    ESP_LOGI(TAG, "Processing input: %s", input.c_str());
    TickType_t now = xTaskGetTickCount();
    SerialResponse resp;
    esp_err_t err = SerialResponse::parse(input, resp);
    if(err != ESP_OK){
        ESP_LOGE(TAG, "Could not parse serial response '%s'", input.c_str());
        return;
    }
    ESP_LOGI(TAG, "Respose: field=%s value=%s", resp.field.c_str(), resp.value.c_str());
    // check field name
    std::string field;
    uint16_t addr;
    FieldParseErr f_err = ParseField(resp.field, field, addr);
    switch(f_err){
        case FieldParseErr::ok:
            // normalize addr formatting
            err = MakeField(addr, field, resp.field);
            if(err != ESP_OK){
                ESP_LOGE(TAG, "Could not make field string!");
                return;
            }
            [[fallthrough]];
        case FieldParseErr::no_addr:
            // field is correct
            if(pdTRUE != xSemaphoreTake(_semMutex, 1500 / portTICK_PERIOD_MS)){
                ESP_LOGW(TAG, "Could not take semaphore when processing input");
                return;
            }
            ESP_LOGI(TAG, "Setting cache[%s]=%s", resp.field.c_str(), resp.value.c_str());
            cache[resp.field] = (cache_value_t){now, resp.value};
            xSemaphoreGive(_semMutex);
            break;
        case FieldParseErr::malformed_addr:
            ESP_LOGE(TAG, "Error during parsing field '%s': malformed_addr", resp.field.c_str());
            break;
        case FieldParseErr::empty_field:
            ESP_LOGE(TAG, "Error during parsing field '%s': empty_field", resp.field.c_str());
            break;
        case FieldParseErr::empty_field_name:
            ESP_LOGE(TAG, "Error during parsing field '%s': empty_field_name", resp.field.c_str());
            break;
        default:
            ESP_LOGE(TAG, "Malformed field!");
            return;
    }
}

// #endif //CONFIG_SERIAL_COMM_CLIENT