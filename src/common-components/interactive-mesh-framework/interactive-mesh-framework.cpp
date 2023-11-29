#include "interactive-mesh-framework.hpp"
#include "serial_comm_client.hpp"
#include <string>
#include <cstdio>
#include "board.h"
#include <memory>
#include "wifi_connect.h"

#define EVENT_LOOP_QUEUE_SIZE 16

static const char* TAG = "IMF";

using namespace imf;

std::shared_ptr<SerialCommCli> Device::_serial = std::make_shared<SerialCommCli>(UART_NUM_1, GPIO_NUM_14, GPIO_NUM_17);
std::shared_ptr<DistanceMeter> Device::_dm = nullptr;

Device::Device(DeviceType _type, uint8_t _wifi_mac[6], uint8_t _wifi_channel, uint16_t _ble_mesh_addr)
    : type(_type), ble_mesh_addr(_ble_mesh_addr){
    if(_type == DeviceType::Station){
        if(_dm != nullptr){
            uint32_t id = _dm->addPoint(_wifi_mac, _wifi_channel);
            if(id != UINT32_MAX){
                _point = _dm->getPoint(id);
            }
        } else{
            _point = std::make_shared<DistancePoint>(UINT32_MAX, _wifi_mac, _wifi_channel);
        }
    }
}

esp_err_t Device::setRgb(rgb_t rgb){
    size_t buf_len = 12+1;
    char buf[buf_len];
    esp_err_t err = rgb_to_str(rgb, buf_len, buf);
    if(err != ESP_OK){
        return err;
    }

    std::string rgb_value {buf};
    std::string response = _serial->PutField(ble_mesh_addr, "rgb", rgb_value);
    ESP_LOGI(TAG, "setRgb response %s", response.c_str());
    return ESP_OK;
}

esp_err_t Device::setRgbAll(rgb_t rgb){
    size_t buf_len = 12+1;
    char buf[buf_len];
    esp_err_t err = rgb_to_str(rgb, buf_len, buf);
    if(err != ESP_OK){
        return err;
    }

    std::string rgb_value {buf};
    _serial->PutField(0xffff, "rgb", rgb_value);
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
    if(type == DeviceType::Mobile || _point == nullptr){
        return UINT32_MAX;
    }
    return _point->measureDistance();
}

IMF::IMF(){
    // Init custom Logger
    logger_init(ESP_LOG_INFO);
    logger_output_to_default();
    logger_init_storage();

    logger_output_to_file("/logs/log.txt", 2000);

    // Init board helper functions
    board_init();

    // event loop init
    esp_event_loop_args_t loop_args{
        EVENT_LOOP_QUEUE_SIZE, // queue_size
        "IMF-loop",             // task_name
        tskIDLE_PRIORITY,      // task_priority
        1024*4,                // task_stack_size
        tskNO_AFFINITY         // task_core_id
    };
    
    if (esp_event_loop_create(&loop_args, &_event_loop_hdl) != ESP_OK) {
        ESP_LOGE(TAG, "create event loop failed");
        return;
    }

    // DistanceMeter init
    _dm = std::make_shared<DistanceMeter>(false, _event_loop_hdl);
    Device::setDM(_dm);

    // init wifi
    wifi_start();
}

esp_err_t IMF::start() { 
    _dm->startTask();
    return ESP_OK;
}
esp_err_t IMF::registerCallbacks(board_button_callback_t btn_cb, esp_event_handler_t event_handler, void *handler_args) 
{ 
    esp_err_t err;
    err = board_buttons_release_register_callback(btn_cb);
    if(err != ESP_OK){
        ESP_LOGE(TAG, "Cannot register board button callback");
        return err;
    }

    err = _dm->registerEventHandle(event_handler, handler_args);
    if(err != ESP_OK){
        ESP_LOGE(TAG, "Cannot board register DM event handle");
        return err;
    }

    return err;
}
esp_err_t IMF::addDevice(DeviceType _type, uint8_t _wifi_mac[6], uint8_t _wifi_channel, uint16_t _ble_mesh_addr) {
    devices.emplace_back(std::make_shared<Device>(_type, _wifi_mac, _wifi_channel, _ble_mesh_addr));
    return ESP_OK;
}

esp_err_t IMF::createAP(const std::string& ssid, const std::string& password, uint8_t channel){
    return wifi_init_ap_simple(ssid.c_str(), password.c_str(), channel);
}

esp_err_t IMF::connectToAP(const std::string& ssid, const std::string& password){
    return wifi_connect_simple(ssid.c_str(), password.c_str());
}

esp_err_t IMF::getMAC(std::string &){
    int64_t mac_addr = 0LL;
    esp_read_mac((uint8_t*) (&mac_addr), ESP_MAC_WIFI_SOFTAP);
    
    char ssid[MAX_SSID_LEN];
    snprintf(ssid, MAX_SSID_LEN, "NODE-%llX", mac_addr);
}