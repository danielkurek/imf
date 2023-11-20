#include "interactive-mesh-framework.hpp"
#include "serial_comm_client.hpp"
#include <string>
#include <cstdio>

using namespace imf;

Device::Device(DeviceType _type, uint8_t _wifi_mac[6], uint8_t _wifi_channel, uint16_t _ble_mesh_addr)
    : type(_type), ble_mesh_addr(_ble_mesh_addr), _point(_wifi_mac, _wifi_channel){
    
}

esp_err_t Device::setRgb(rgb_t rgb){
    size_t buf_len = 12+1;
    char buf[buf_len];
    esp_err_t err = rgb_to_str(rgb, buf_len, buf);
    if(err != ESP_OK){
        return err;
    }

    std::string rgb_value {buf};
    _serial->PutField(ble_mesh_addr, "rgb", rgb_value);
    return ESP_OK;
}

rgb_t Device::getRgb(){
    rgb_t rgb;
    std::string rgb_val = _serial->GetField(ble_mesh_addr, "rgb");
    esp_err_t err = str_to_rgb(rgb_val.c_str(), &rgb);
    if(err != ESP_OK){
        ESP_LOGE(TAG, "Failed to convert RGB string to value: %s", rgb_val.c_str());
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