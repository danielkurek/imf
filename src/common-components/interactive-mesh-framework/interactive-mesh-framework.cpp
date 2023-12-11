#include "interactive-mesh-framework.hpp"
#include "serial_comm_client.hpp"
#include <string>
#include <cstdio>
#include "board.h"
#include <memory>
#include "wifi_connect.h"
#include "esp_timer.h"
#include <vector>

#define EVENT_LOOP_QUEUE_SIZE 16

static const char* TAG = "IMF";

using namespace imf;

static esp_timer_handle_t update_timer;

std::shared_ptr<SerialCommCli> Device::_serial = std::make_shared<SerialCommCli>(UART_NUM_1, GPIO_NUM_14, GPIO_NUM_17);
std::shared_ptr<DistanceMeter> Device::_dm = nullptr;

Device::Device(uint32_t _id, DeviceType _type, std::string _wifi_mac_str, uint8_t _wifi_channel, uint16_t _ble_mesh_addr)
    : id(_id), type(_type), ble_mesh_addr(_ble_mesh_addr){
    if(_type == DeviceType::Station){
        if(_dm != nullptr){
            uint32_t id = _dm->addPoint(_wifi_mac_str, _wifi_channel);
            if(id != UINT32_MAX){
                _point = _dm->getPoint(id);
            }
        } else{
            uint8_t wifi_mac[6];
            sscanf(_wifi_mac_str.c_str(), MACSTR_SCN, STR2MAC(wifi_mac));
            _point = std::make_shared<DistancePoint>(UINT32_MAX, wifi_mac, _wifi_mac_str, _wifi_channel);
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

esp_err_t Device::measureDistance(uint32_t *distance_cm){
    if(type == DeviceType::Mobile || _point == nullptr){
        return UINT32_MAX;
    }
    return _point->measureDistance(distance_cm);
}

static esp_err_t color_validate(const char* value){
    rgb_t color;
    return str_to_rgb(value, &color);
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

    _options = {};
    _options.emplace_back((config_option_t){
        .key = "color",
        .type = NVS_TYPE_STR,
        .value_to_log = true,
        .validate_function = color_validate,
    });

    if(nvs_open("config", NVS_READWRITE, &_options_handle) != ESP_OK){
        ESP_LOGI(TAG, "Error opening NVS!");
        return;
    }

    // init wifi
    wifi_start();
}

esp_err_t IMF::start() { 
    _dm->startTask();
    return ESP_OK;
}

void IMF::_update_timer_cb(){
    static TickType_t _last_update = 0;
    TickType_t now = xTaskGetTickCount();
    TickType_t diff = pdTICKS_TO_MS(now) - pdTICKS_TO_MS(_last_update);
    if(_update_cb != NULL){
        _update_cb(diff);
    }
    _last_update = now;
}

esp_err_t IMF::registerCallbacks(board_button_callback_t btn_cb, esp_event_handler_t event_handler, void *handler_args, update_function_t update_cb) 
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
    if(update_cb != NULL){
        esp_timer_create_args_t args = {
            .callback = _update_timer_cb_wrapper,
            .arg = this,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "UpdateTimer",
            .skip_unhandled_events = true
        };
        
        _update_timer_cb();

        err = esp_timer_create(&args, &(update_timer));
        if(err != ESP_OK){
            ESP_LOGE(TAG, "Failed initializing update timer! %s", esp_err_to_name(err));
            return err;
        }
        
        _update_cb = update_cb;

        err = esp_timer_start_periodic(update_timer, 200 * 1000); // every 200 ms
        if(err != ESP_OK){
            ESP_LOGE(TAG, "Failed to start update timer! %s", esp_err_to_name(err));
            return err;
        }
    }
    return err;
}
uint32_t IMF::addDevice(DeviceType type, std::string _wifi_mac_str, uint8_t wifi_channel, uint16_t ble_mesh_addr) {
    if(_next_id == UINT32_MAX){
        return UINT32_MAX;
    }
    uint32_t id = _next_id;
    _devices.emplace(id, std::make_shared<Device>(id, type, _wifi_mac_str, wifi_channel, ble_mesh_addr));
    _next_id += 1;
    return id;
}

std::shared_ptr<Device> IMF::getDevice(uint32_t id){
    auto search = _devices.find(id);
    if(search != _devices.end()){
        return search->second;
    }

    return nullptr;
}

esp_err_t IMF::createAP(const std::string& ssid, const std::string& password, uint8_t channel){
    return wifi_init_ap_simple(ssid.c_str(), password.c_str(), channel);
}

esp_err_t IMF::connectToAP(const std::string& ssid, const std::string& password){
    return wifi_connect_simple(ssid.c_str(), password.c_str());
}

esp_err_t IMF::addOption(const config_option_t& option){
    _options.push_back(option);
    return ESP_OK;
}

void IMF::startWebConfig(){
    esp_err_t err = web_config_set_custom_options(_options.size(), _options.data());
    if(err != ESP_OK){
        LOGGER_E(TAG, "Could not set custom web config options! Err: %d", err);
        return;
    }
    web_config_start();
}

void IMF::stopWebConfig(){
    web_config_stop();
}