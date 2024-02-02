#include "interactive-mesh-framework.hpp"
#include "serial_comm_client.hpp"
#include <string>
#include <cstdio>
#include "board.h"
#include <memory>
#include "wifi_connect.h"
#include "esp_timer.h"
#include <vector>
#include "location_common.h"
#include "esp_check.h"

#define EVENT_LOOP_QUEUE_SIZE 16

#define DEFAULT_COLOR_OPT "color"
#define BLE_MESH_ADDR_OPT "ble_mesh/addr"

static const char* TAG = "IMF";

using namespace imf;

static esp_timer_handle_t update_timer;

std::shared_ptr<SerialCommCli> Device::_serial = std::make_shared<SerialCommCli>(UART_NUM_1, GPIO_NUM_14, GPIO_NUM_17);
std::shared_ptr<DistanceMeter> Device::_dm = nullptr;
std::shared_ptr<Device> Device::this_device = nullptr;

Device::Device(uint32_t _id, DeviceType _type, std::string _wifi_mac_str, uint8_t _wifi_channel, uint16_t _ble_mesh_addr, bool local_commands)
    : id(_id), type(_type), ble_mesh_addr(_ble_mesh_addr), _local_commands(local_commands){
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

Device::Device(uint32_t _id, DeviceType _type, std::string _wifi_mac_str, uint8_t _wifi_channel, uint16_t _ble_mesh_addr)
    : Device(_id, _type, _wifi_mac_str, _wifi_channel, _ble_mesh_addr, false) {
}

esp_err_t Device::initLocalDevice(IMF *imf){
    uint16_t ble_mesh_addr = 0x0000;
    bool valid_addr = false;
    uint16_t wifi_channel = 1;
    std::string addr;
    esp_err_t err;

    if(imf != nullptr){
        // save MAC and ble-mesh addr
        nvs_handle_t nvs_handle = imf->getOptionsHandle();
        char addr_str[ADDR_STR_LEN];
        size_t addr_str_len = sizeof(addr_str);
        err = nvs_get_str(nvs_handle, BLE_MESH_ADDR_OPT, addr_str, &addr_str_len);
        if(err == ESP_OK){
            LOGGER_I(TAG, "Using ble-mesh address stored in nvs for local device: %s", addr_str);
            err = StrToAddr(addr_str, &ble_mesh_addr);
            if(err == ESP_OK){
                valid_addr = true;
            } else{
                LOGGER_E(TAG, "Stored ble-mesh address is not valid!");
            }
        } else{
            LOGGER_E(TAG, "Could not get ble-mesh addr from NVS! Err: %d", err);
        }
        err = nvs_get_u16(nvs_handle, "softAP/channel", &wifi_channel);
        if(err != ESP_OK){
            LOGGER_E(TAG, "Could not get softAP/channel! Err: %d", err);
        }
    }
    if(!valid_addr){
        for(int i = 0; i < _maxRetries; i++){
            addr = _serial->GetField("addr");
            if(addr.length() > 0 && addr != "FAIL"){
                err = StrToAddr(addr.c_str(), &ble_mesh_addr);
                if(err == ESP_OK){
                    LOGGER_I(TAG, "Using ble-mesh address obtained from ble-mesh device: %s", addr.c_str());
                    LOGGER_I(TAG, "0x%04" PRIx16, ble_mesh_addr);
                    valid_addr = true;
                    break;
                }
            }
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }
    }
    if(!valid_addr){
        LOGGER_E(TAG, "Could not get valid ble-mesh address for local device");
        return ESP_FAIL;
    } 

    if(valid_addr && imf != nullptr){
        nvs_handle_t nvs_handle = imf->getOptionsHandle();
        err = AddrToStr(ble_mesh_addr, addr);
        if(err != ESP_OK){
            LOGGER_E(TAG, "Could not convert addr to str!");
        } else{
            err = nvs_set_str(nvs_handle, BLE_MESH_ADDR_OPT, addr.c_str());
            if(err != ESP_OK){
                LOGGER_E(TAG, "Could not save local ble_mesh/addr! Err: %d", err);
            }
        }
    }
    // TODO: get device type
    this_device = std::shared_ptr<Device>(new Device(0, DeviceType::Mobile, _getMAC(), 1, ble_mesh_addr, !valid_addr));\
    
    // set default color
    if(imf != nullptr){
        LOGGER_I(TAG, "Setting default color for this device");
        nvs_handle_t nvs_handle = imf->getOptionsHandle();
        char rgb_str[RGB_STR_LEN];
        size_t rgb_str_len = RGB_STR_LEN;
        err = nvs_get_str(nvs_handle, DEFAULT_COLOR_OPT, rgb_str, &rgb_str_len);
        if(err != ESP_OK){
            LOGGER_E(TAG, "Could not get default color! Err: %d", err);
        } else{
            rgb_t color;
            err = str_to_rgb(rgb_str, &color);
            if(err != ESP_OK){
                LOGGER_E(TAG, "Could not get convert default color! Color: %s", rgb_str);
            } else{
                this_device->setRgb(color);
            }
        }
    }
    return ESP_OK;
}

esp_err_t Device::setRgb(rgb_t rgb){
    size_t buf_len = 6+1;
    char buf[buf_len];
    esp_err_t err = rgb_to_str(rgb, buf_len, buf);
    if(err != ESP_OK){
        return err;
    }

    std::string rgb_value {buf};
    std::string response;
    if(_local_commands){
        response = _serial->PutField("rgb", rgb_value);
    } else{
        response = _serial->PutField(ble_mesh_addr, "rgb", rgb_value);
    }
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

esp_err_t Device::getRgb(rgb_t *rgb_out){
    std::string rgb_val;
    if(_local_commands){
        rgb_val = _serial->GetField("rgb");
    } else {
        rgb_val = _serial->GetField(ble_mesh_addr, "rgb");
    }
    esp_err_t err = str_to_rgb(rgb_val.c_str(), rgb_out);
    if(err != ESP_OK){
        ESP_LOGE(TAG, "Failed to convert RGB string to value: %s", rgb_val.c_str());
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t Device::setLocation(const location_local_t *location){
    char buf[SIMPLE_LOC_STR_LEN];
    esp_err_t err = simple_loc_to_str(location, SIMPLE_LOC_STR_LEN, buf);
    if(err != ESP_OK){
        return err;
    }

    std::string loc_value {buf};
    std::string response;
    if(_local_commands){
        response = _serial->PutField("loc", loc_value);
    } else{
        response = _serial->PutField(ble_mesh_addr, "loc", loc_value);
    }
    ESP_LOGI(TAG, "set loc response %s", response.c_str());
    return ESP_OK;
}

esp_err_t Device::getLocation(location_local_t *location_out){
    std::string loc_val;
    if(_local_commands){
        loc_val = _serial->GetField("loc");
    } else {
        loc_val = _serial->GetField(ble_mesh_addr, "loc");
    }
    esp_err_t err = simple_str_to_loc(loc_val.c_str(), location_out);
    if(err != ESP_OK){
        ESP_LOGE(TAG, "Failed to convert simple Location response to value: %s", loc_val.c_str());
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t Device::setLevel(int16_t level){
    char buf[5+1];
    int ret = snprintf(buf, 5+1, "%04" PRIx16, level);
    if(ret <= 0){
        return ESP_FAIL;
    }

    std::string level_value {buf};
    std::string response;
    if(_local_commands){
        response = _serial->PutField("level", level_value);
    } else{
        response = _serial->PutField(ble_mesh_addr, "level", level_value);
    }
    ESP_LOGI(TAG, "set level response %s", response.c_str());
    return ESP_OK;
}

esp_err_t Device::getLevel(int16_t *level_out){
    std::string level_val;
    if(_local_commands){
        level_val = _serial->GetField("level");
    } else {
        level_val = _serial->GetField(ble_mesh_addr, "level");
    }
    int ret = sscanf(level_val.c_str(), "%04" SCNx16, level_out);
    if(ret != 1){
        ESP_LOGE(TAG, "Failed to convert Level response to value: %s", level_val.c_str());
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t Device::measureDistance(uint32_t *distance_cm){
    if(type == DeviceType::Mobile || _point == nullptr){
        return UINT32_MAX;
    }
    return _point->measureDistance(distance_cm);
}

esp_err_t Device::distance(uint32_t *distance_cm){
    distance_measurement_t dm;
    esp_err_t err = _point->getDistanceFromLog(dm);
    if(err != ESP_OK) return err;
    (*distance_cm) = dm.distance_cm;
    return ESP_OK;
}

std::string Device::_getMAC(){
    uint8_t mac_addr[8]; // only 6 bytes will be used
    esp_read_mac(mac_addr, ESP_MAC_WIFI_SOFTAP);
    
    char macstr[2*6 + 5 + 1];
    int ret = snprintf(macstr, MAX_SSID_LEN, MACSTR, MAC2STR(mac_addr));
    if(ret > 0){
        return {macstr};
    }
    return "";
}

static esp_err_t color_validate(const char *value){
    rgb_t color;
    return str_to_rgb(value, &color);
}

static esp_err_t ble_mesh_addr_validate(const char *value){
    uint16_t addr;
    return StrToAddr(value, &addr);
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

    if (esp_event_loop_create_default() != ESP_OK){
        ESP_LOGE(TAG, "create default event loop failed");
        return;
    }

    _options = {};
    _options.emplace_back((config_option_t){
        .key = DEFAULT_COLOR_OPT,
        .type = NVS_TYPE_STR,
        .value_to_log = true,
        .validate_function = color_validate,
    });
    _options.emplace_back((config_option_t){
        .key = BLE_MESH_ADDR_OPT,
        .type = NVS_TYPE_STR,
        .value_to_log = true,
        .validate_function = ble_mesh_addr_validate,
    });

    ESP_LOGI(TAG, "NVS flash init...");
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    if(nvs_open("config", NVS_READWRITE, &_options_handle) != ESP_OK){
        ESP_LOGI(TAG, "Error opening NVS!");
        return;
    }

    // init wifi
    wifi_start();

    // DistanceMeter init
    _dm = std::make_shared<DistanceMeter>(false, _event_loop_hdl);
    Device::setDM(_dm);
}

esp_err_t IMF::_wait_for_ble_mesh(uint32_t max_tries){
    auto serial = Device::getSerialCli();
    uint16_t ble_mesh_addr;
    std::string addr;
    for(uint32_t i = 0; i < max_tries; i++){
        addr = serial->GetField("addr");
        LOGGER_I(TAG, "Wait4Mesh: Response for addr: %s", addr.c_str());
        if(addr.length() > 0 && addr != "FAIL"){
            esp_err_t err = StrToAddr(addr.c_str(), &ble_mesh_addr);
            if(err == ESP_OK){
                return ESP_OK;
            } else{
                LOGGER_E(TAG, "Cannot parse Addr while waiting for ble_mesh! AddrStr=%s", addr.c_str());
            }
        }
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    return ESP_FAIL;
}

esp_err_t IMF::start() { 
    Device::initLocalDevice(this);
    _devices.emplace(0, Device::this_device);
    _wait_for_ble_mesh(20);
    _dm->startTask();
#ifdef CONFIG_IMF_STATION_DEVICE 
    wifi_init_ap_default();
#endif
    return ESP_OK;
}

void IMF::_update_timer_cb(){
    static TickType_t _last_update = 0;
    TickType_t now = xTaskGetTickCount();
    if(_update_cb != nullptr){
        TickType_t diff = pdTICKS_TO_MS(now) - pdTICKS_TO_MS(_last_update);
        _update_cb(diff);
    }
    _last_update = now;
}

esp_err_t IMF::registerCallbacks(board_button_callback_t btn_cb, esp_event_handler_t event_handler, void *handler_args, update_function_t update_cb) 
{ 
    esp_err_t err = ESP_OK;
    if(btn_cb != NULL){
        err = board_buttons_release_register_callback(btn_cb);
        if(err != ESP_OK){
            ESP_LOGE(TAG, "Cannot register board button callback");
            return err;
        }
    }
    if(event_handler != NULL){
        err = _dm->registerEventHandle(event_handler, handler_args);
        if(err != ESP_OK){
            ESP_LOGE(TAG, "Cannot board register DM event handle");
            return err;
        }
    }
    if(update_cb != NULL){
        ESP_LOGE(TAG, "Here3");
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