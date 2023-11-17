#include "interactive-mesh-framework.hpp"
#include "serial_comm_client.hpp"
#include <string>
#include <cstdio>

using namespace imf;

Device::Device(DeviceType _type, uint8_t _wifi_mac[6], uint8_t _wifi_channel, uint16_t _ble_mesh_addr)
    : type(_type), ble_mesh_addr(_ble_mesh_addr), _point(_wifi_mac, _wifi_channel){
    
}

esp_err_t Device::setRgb(rgb_t rgb){
    char buf[6+1];
    snprintf(buf, 6+1, "%2hx%2hx%2hx", rgb.red, rgb.green, rgb.blue);
    std::string rgb_value {buf};
    _serial->PutField("rgb", rgb_value);
    return ESP_OK;
}

rgb_t Device::getRgb(){
    rgb_t rgb;
    _serial->GetField("rgb");
    const int ret = std::scanf("%2hx%2hx%2hx", &(rgb.red), &(rgb.green), &(rgb.blue));
    if(ret != 3){
        // error
    }

    return rgb;
}

uint32_t Device::measureDistance(){
    if(type == DeviceType::Mobile){
        return UINT32_MAX;
    }
    return _point.measureDistance();
}
IMF::IMF(){}
esp_err_t IMF::start() { return ESP_FAIL; }
esp_err_t IMF::registerEventCallback() { return ESP_FAIL; }
esp_err_t IMF::addDevice() { return ESP_FAIL; }