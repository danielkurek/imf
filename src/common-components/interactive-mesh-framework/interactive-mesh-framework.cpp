#include "interactive-mesh-framework.hpp"
#include "imf-device.hpp"
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

#include "mlat_localization.hpp"

#define EVENT_LOOP_QUEUE_SIZE 16

#define DEFAULT_COLOR_OPT "color"
#define BLE_MESH_ADDR_OPT "ble_mesh/addr"
#define LOCATION_POS_OPT  "loc/pos"

// can be configured via menuconfig (idf.py menuconfig)
#define SERIAL_TX_GPIO CONFIG_IMF_SERIAL_TX_GPIO
#define SERIAL_RX_GPIO CONFIG_IMF_SERIAL_RX_GPIO

#define UPDATE_TIME_MS 500

static const char* TAG = "IMF";

using namespace imf;

static esp_timer_handle_t update_timer;

std::shared_ptr<SerialCommCli> Device::_serial = std::make_shared<SerialCommCli>(UART_NUM_1, SERIAL_TX_GPIO, SERIAL_RX_GPIO, 1000 / portTICK_PERIOD_MS);
std::shared_ptr<DistanceMeter> Device::_dm = nullptr;
std::shared_ptr<Device> Device::this_device = nullptr;

static esp_err_t loc_pos_opt_parse(const char *value, int16_t &north, int16_t &east){
    int ret = sscanf(value, "N%" SCNd16 "E%" SCNd16, &north, &east);
    if(ret == 2){
        return ESP_OK;
    }
    return ESP_FAIL;
}

Device::Device(uint32_t _id, DeviceType _type, std::string _wifi_mac_str, uint8_t _wifi_channel, uint16_t _ble_mesh_addr, bool local_commands)
    : id(_id), type(_type), ble_mesh_addr(_ble_mesh_addr), fixed_location(false), _local_commands(local_commands){
    // Only measure distances to stations
    if(_type == DeviceType::Station){
        if(_dm != nullptr){
            uint32_t id = _dm->addPoint(_wifi_mac_str, _wifi_channel, _id);
            if(id != UINT32_MAX){
                _point = _dm->getPoint(id);
            }
        } else{
            ESP_LOGW(TAG, "Adding device without DM");
            uint8_t wifi_mac[6];
            sscanf(_wifi_mac_str.c_str(), MACSTR_SCN, STR2MAC(wifi_mac));
            _point = std::make_shared<DistancePoint>(UINT32_MAX, wifi_mac, _wifi_mac_str, _wifi_channel);
        }
    }
    if(_point){
        _point->setFrameCount(8);
        _point->setBurstPeriod(0);
    }
}

Device::Device(uint32_t _id, DeviceType _type, std::string _wifi_mac_str, uint8_t _wifi_channel, uint16_t _ble_mesh_addr)
    : Device(_id, _type, _wifi_mac_str, _wifi_channel, _ble_mesh_addr, false) {
}

#if CONFIG_IMF_DEBUG_STATIC_DEVICES
Device::Device(uint32_t _id, DeviceType _type, std::string _wifi_mac_str, uint8_t _wifi_channel, uint16_t _ble_mesh_addr, 
    rgb_t rgb, location_local_t location, int16_t level, uint32_t distance_cm, int8_t rssi)
    : id(_id), type(_type), ble_mesh_addr(_ble_mesh_addr), _local_commands(false){
    setRgb(rgb);
    setLocation(location);
    setLevel(level);
    debug_distance_cm = distance_cm;
    debug_rssi = rssi;
}
#endif

esp_err_t Device::initLocalDevice(nvs_handle_t options_handle){
    if(Device::this_device){
        return ESP_FAIL;
    }
    uint16_t ble_mesh_addr = 0x0000;
    bool valid_addr = false;
    uint16_t wifi_channel = 1;
    std::string addr;
    esp_err_t err;

    // get AP channel
    err = nvs_get_u16(options_handle, "softAP/channel", &wifi_channel);
    if(err != ESP_OK){
        LOGGER_E(TAG, "Could not get softAP/channel! Err: %d", err);
    }

    // get BLE-Mesh addr
    for(int i = 0; i < _maxRetries; i++){
        addr = _serial->GetField("addr");
        if(addr.length() > 0 && addr != "FAIL"){
            err = StrToAddr(addr.c_str(), &ble_mesh_addr);
            if(err == ESP_OK){
                LOGGER_I(TAG, "Using ble-mesh address obtained from ble-mesh device: %s (0x%04" PRIx16 ")", addr.c_str(), ble_mesh_addr);
                valid_addr = true;
                break;
            }
        }
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    if(!valid_addr){
        // try to get it from NVS
        char addr_str[ADDR_STR_LEN];
        size_t addr_str_len = sizeof(addr_str);
        err = nvs_get_str(options_handle, BLE_MESH_ADDR_OPT, addr_str, &addr_str_len);
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
    }
    if(!valid_addr){
        LOGGER_E(TAG, "Could not get valid ble-mesh address for local device");
        return ESP_FAIL;
    } 

    // save BLE-Mesh addr
    if(valid_addr){
        err = AddrToStr(ble_mesh_addr, addr);
        if(err != ESP_OK){
            LOGGER_E(TAG, "Could not convert addr to str!");
        } else{
            err = nvs_set_str(options_handle, BLE_MESH_ADDR_OPT, addr.c_str());
            if(err != ESP_OK){
                LOGGER_E(TAG, "Could not save local ble_mesh/addr! Err: %d", err);
            }
        }
    }
#if CONFIG_IMF_MOBILE_DEVICE
    DeviceType type = DeviceType::Mobile;
#else
    DeviceType type = DeviceType::Station;
#endif

    this_device = std::shared_ptr<Device>(new Device(0, type, _getMAC(), 1, ble_mesh_addr, !valid_addr));
    
    // set default color && location
    LOGGER_I(TAG, "Setting default color for this device");
    char rgb_str[RGB_STR_LEN];
    size_t rgb_str_len = RGB_STR_LEN;
    err = nvs_get_str(options_handle, DEFAULT_COLOR_OPT, rgb_str, &rgb_str_len);
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
    location_local_t location {0,0,0,0,UINT16_MAX};
    size_t pos_len = ((6+1)*2)+1;
    char pos_str[pos_len];
    err = nvs_get_str(options_handle, LOCATION_POS_OPT, pos_str, &pos_len);
    if(err != ESP_OK){
        LOGGER_E(TAG, "Could not get location! Err: %d", err);
    } else{
        int16_t north,east;
        err = loc_pos_opt_parse(pos_str, north, east);
        if(err == ESP_OK){
            location.local_north = north;
            location.local_east = east;
            location.uncertainty = 0;
            this_device->fixed_location = true;
        }
    }
    this_device->setLocation(location);
    return ESP_OK;
}

#if CONFIG_IMF_DEBUG_STATIC_DEVICES
esp_err_t Device::setRgb(rgb_t rgb){
    char rgb_str[RGB_STR_LEN];
    if(ESP_OK == rgb_to_str(rgb, RGB_STR_LEN, rgb_str)){
        ESP_LOGI(TAG, "setRgb device=%" PRIu32 ": %s", id, rgb_str);
    } else {
        ESP_LOGI(TAG, "setRgb device=%" PRIu32 ": Could not convert", id);
    }
    debug_rgb = rgb;
    return ESP_OK;
}
#else
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
#endif

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

#if CONFIG_IMF_DEBUG_STATIC_DEVICES
esp_err_t Device::getRgb(rgb_t &rgb_out){
    rgb_out.red = debug_rgb.red;
    rgb_out.green = debug_rgb.green;
    rgb_out.blue = debug_rgb.blue;
    return ESP_OK;
}
#else
esp_err_t Device::getRgb(rgb_t &rgb_out){
    std::string rgb_val;
    if(_local_commands){
        rgb_val = _serial->GetField("rgb");
    } else {
        rgb_val = _serial->GetField(ble_mesh_addr, "rgb");
    }
    esp_err_t err = str_to_rgb(rgb_val.c_str(), &rgb_out);
    if(err != ESP_OK){
        ESP_LOGE(TAG, "Failed to convert RGB string to value: %s", rgb_val.c_str());
        return ESP_FAIL;
    }

    return ESP_OK;
}
#endif

#if CONFIG_IMF_DEBUG_STATIC_DEVICES
esp_err_t Device::setLocation(const location_local_t &location){
    char buf[SIMPLE_LOC_STR_LEN];
    if(ESP_OK == simple_loc_to_str(&location, SIMPLE_LOC_STR_LEN, buf)){
        ESP_LOGI(TAG, "setLocation device=%" PRIu32 ": %s", id, buf);
    } else{
        ESP_LOGI(TAG, "setLocation device=%" PRIu32 ": could not convert location to str", id);
    }
    
    debug_location = location;
    return ESP_OK;
}
#else
esp_err_t Device::setLocation(const location_local_t &location){
    char buf[SIMPLE_LOC_STR_LEN];
    esp_err_t err = simple_loc_to_str(&location, SIMPLE_LOC_STR_LEN, buf);
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
#endif

#if CONFIG_IMF_DEBUG_STATIC_DEVICES
esp_err_t Device::getLocation(location_local_t &location_out){
    location_out = debug_location;
    return ESP_OK;
}
#else
esp_err_t Device::getLocation(location_local_t &location_out){
    std::string loc_val;
    if(_local_commands){
        loc_val = _serial->GetField("loc");
    } else {
        loc_val = _serial->GetField(ble_mesh_addr, "loc");
    }
    esp_err_t err = simple_str_to_loc(loc_val.c_str(), &location_out);
    if(err != ESP_OK){
        ESP_LOGE(TAG, "Failed to convert simple Location response to value: %s", loc_val.c_str());
        return ESP_FAIL;
    }

    return ESP_OK;
}
#endif

#if CONFIG_IMF_DEBUG_STATIC_DEVICES
esp_err_t Device::setLevel(int16_t level){
    ESP_LOGI(TAG, "setLevel device=%" PRIu32": %" PRId16, id, level);
    debug_level = level;
    return ESP_OK;
}
#else
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
#endif

esp_err_t Device::setLevelAll(int16_t level){
    char buf[5+1];
    int ret = snprintf(buf, 5+1, "%04" PRIx16, level);
    if(ret <= 0){
        return ESP_FAIL;
    }

    std::string level_value {buf};
    std::string response;
    response = _serial->PutField(0xffff, "level", level_value);
    ESP_LOGI(TAG, "set level response %s", response.c_str());
    return ESP_OK;
}

#if CONFIG_IMF_DEBUG_STATIC_DEVICES
esp_err_t Device::getLevel(int16_t &level_out){
    level_out = debug_level;
    return ESP_OK;
}
#else
esp_err_t Device::getLevel(int16_t &level_out){
    std::string level_val;
    if(_local_commands){
        level_val = _serial->GetField("level");
    } else {
        level_val = _serial->GetField(ble_mesh_addr, "level");
    }
    if(level_val == "FAIL") return ESP_FAIL;
    
    int ret = sscanf(level_val.c_str(), "%04" SCNx16, &level_out);
    if(ret != 1){
        ESP_LOGE(TAG, "Failed to convert Level response to value: %s", level_val.c_str());
        return ESP_FAIL;
    }

    return ESP_OK;
}
#endif

#if CONFIG_IMF_DEBUG_STATIC_DEVICES
esp_err_t Device::measureDistance(distance_measurement_t &measurement){
    if(type == DeviceType::Mobile){
        return ESP_FAIL;
    }
    measurement.distance_cm = debug_distance_cm;
    measurement.rssi = debug_rssi;
    return ESP_OK;
}
#else
esp_err_t Device::measureDistance(distance_measurement_t &measurement){
    if(type == DeviceType::Mobile || _point == nullptr){
        return ESP_FAIL;
    }
    return _point->measureDistance(measurement);
}
#endif

#if CONFIG_IMF_DEBUG_STATIC_DEVICES
esp_err_t Device::lastDistance(distance_measurement_t &measurement){
    measurement.distance_cm = debug_distance_cm;
    measurement.rssi = debug_rssi;
    return ESP_OK;
}
#else
esp_err_t Device::lastDistance(distance_measurement_t &measurement){
    distance_log_t log_measurement;
    if(!_point) return ESP_FAIL;
    esp_err_t err = _point->getDistanceFromLog(log_measurement);
    if(err != ESP_OK) return err;
    measurement = log_measurement.measurement;
    return ESP_OK;
}
#endif

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

static esp_err_t loc_pos_validate(const char *value){
    // allow empty string
    if(strlen(value) == 0) return ESP_OK;
    int16_t north, east;
    return loc_pos_opt_parse(value, north, east);
}

IMF::IMF(const std::vector<button_gpio_config_t> &buttons, bool default_states){
    esp_err_t err;
    // Init custom Logger
    logger_init(ESP_LOG_INFO);
    logger_output_to_default(true);
    logger_init_storage();

    logger_output_to_file("/logs/log.txt", 2000);

    // Init board helper functions
    board_init(buttons.size(), &buttons[0]);
    board_set_rgb(&internal_rgb_conf, (rgb_t){0,0,0});
    board_set_ext_pwr(true);

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
    err = esp_event_loop_create_default();
    // ESP_ERR_INVALID_STATE is returned when default loop was already created
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE){
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
    _options.emplace_back((config_option_t){
        .key = LOCATION_POS_OPT,
        .type = NVS_TYPE_STR,
        .value_to_log = true,
        .validate_function = loc_pos_validate,
    });

    ESP_LOGI(TAG, "NVS flash init...");
    err = nvs_flash_init();
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

    xSemaphoreUpdate = xSemaphoreCreateBinary();
    auto serial = Device::getSerialCli();
    if(serial)
        serial->startReadTask();
    
    if(default_states){
        addDefaultStates();
    }
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

void IMF::_init_localization(){
    std::vector<std::shared_ptr<Device>> stations;
    for(auto it = _devices.begin(); it != _devices.end(); ++it){
        std::shared_ptr<Device> device = it->second;
        if(device && device->type == DeviceType::Station){
            stations.push_back(device);
        }
    }
    _localization = std::make_shared<MlatLocalization>(Device::this_device, stations);
}

esp_err_t IMF::start() { 
    _wait_for_ble_mesh(20);

    Device::initLocalDevice(getOptionsHandle());
    // _devices.emplace(0, Device::this_device);
    _init_localization();
    
    // _dm->startTask();
#ifdef CONFIG_IMF_STATION_DEVICE 
    wifi_init_ap_default();
#endif

    startUpdateTask();

    return ESP_OK;
}

esp_err_t IMF::startUpdateTask(){
    auto ret = xTaskCreatePinnedToCore(_update_task_wrapper, "UpdateTask", 1024*48, this, tskIDLE_PRIORITY, &_xUpdateHandle, 1);
    if(ret != pdPASS){
        ESP_LOGE(TAG, "Could not create UpdateTask");
        return ESP_FAIL;
    }
    return ESP_OK;
}
void IMF::stopUpdateTask(){
    if(_xUpdateHandle){
        vTaskDelete(_xUpdateHandle);
        _xUpdateHandle = NULL;
    }
}

void IMF::_update_task(){
    TickType_t _last_update = 0;
    tick_function_t tick = nullptr;
    auto it = _states.find(current_state);
    if(it == _states.end()){
        LOGGER_W(TAG, "Could not find tick function for given state %" PRId16, current_state);
        tick = nullptr;
        board_set_rgb(&internal_rgb_conf, (rgb_t){0,0,0});
    } else{
        tick = it->second.tick_f;
        board_set_rgb(&internal_rgb_conf, it->second.color);
    }
    while(true){
        ESP_LOGI(TAG, "StackHighWaterMark=%d, heapfree=%d, heapminfree=%d, heapmaxfree=%d", uxTaskGetStackHighWaterMark(NULL), heap_caps_get_free_size(MALLOC_CAP_8BIT), heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT), heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
        // ESP_LOGI(TAG, "Waiting for semaphore");
        // if(!xSemaphoreTake(xSemaphoreUpdate, 10000 / portTICK_PERIOD_MS)){
        //     ESP_LOGI(TAG, "Semaphore not given");
        //     continue;
        // }

        TickType_t now = xTaskGetTickCount();
        esp_err_t err;
        if(Device::this_device){
            int16_t state;
            err = Device::this_device->getLevel(state);
            if(err != ESP_OK){
                state = 0;
                LOGGER_E(TAG, "Could not get a state! Err=%d", err);
            }
            
            // do actions when state is switched
            if(current_state != state){
                ESP_LOGI(TAG, "State has changed");
                auto it = _states.find(state);
                if(it == _states.end()){
                    LOGGER_W(TAG, "Could not find tick function for given state %" PRId16, state);
                    tick = nullptr;
                    board_set_rgb(&internal_rgb_conf, (rgb_t){0,0,0});
                } else{
                    tick = it->second.tick_f;
                    board_set_rgb(&internal_rgb_conf, it->second.color);
                }
                if(_state_change_cb){
                    _state_change_cb(current_state, state);
                }
            }
            current_state = state;
            
            if(tick != nullptr){
                ESP_LOGI(TAG, "Performing tick state function");
                TickType_t diff = pdTICKS_TO_MS(now) - pdTICKS_TO_MS(_last_update);
                tick(diff);
                _last_update = now;
            }
        }
        logger_sync_file();
        vTaskDelay(UPDATE_TIME_MS / portTICK_PERIOD_MS);
    }
}

void IMF::_update_timer_cb(){
    ESP_LOGI(TAG, "Update timer tick");
    xSemaphoreGive(xSemaphoreUpdate);
}

esp_err_t IMF::registerCallbacks(board_button_callback_t btn_cb, esp_event_handler_t event_handler, void *handler_args, tick_function_t update_cb, state_change_t state_change_cb) 
{ 
    esp_err_t err = ESP_OK;
    if(btn_cb != nullptr){
        err = board_buttons_release_register_callback(btn_cb);
        if(err != ESP_OK){
            ESP_LOGE(TAG, "Cannot register board button callback");
            return err;
        }
    }
    if(event_handler != nullptr){
        err = _dm->registerEventHandle(event_handler, handler_args);
        if(err != ESP_OK){
            ESP_LOGE(TAG, "Cannot board register DM event handle");
            return err;
        }
    }
    _update_cb = update_cb;
    _state_change_cb = state_change_cb;
    // esp_timer_create_args_t args = {
    //     .callback = _update_timer_cb_wrapper,
    //     .arg = this,
    //     .dispatch_method = ESP_TIMER_TASK,
    //     .name = "UpdateTimer",
    //     .skip_unhandled_events = true
    // };
    
    // _update_timer_cb();
    // err = esp_timer_create(&args, &(update_timer));
    // if(err != ESP_OK){
    //     ESP_LOGE(TAG, "Failed initializing update timer! %s", esp_err_to_name(err));
    //     return err;
    // }
    
    // _update_cb = update_cb;
    // err = esp_timer_start_periodic(update_timer, UPDATE_TIME_MS * 1000);
    // if(err != ESP_OK){
    //     ESP_LOGE(TAG, "Failed to start update timer! %s", esp_err_to_name(err));
    //     return err;
    // }
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

void IMF::setStateData(int16_t state_num, tick_function_t tick_fun, rgb_t color){
    auto it = _states.find(state_num);
    if(it != _states.end()){
        _states[state_num].tick_f = tick_fun; 
        _states[state_num].color  = color;
    } else{
        _states[state_num] = (state_data_t){tick_fun, color};
    }
}

void IMF::addDefaultStates(){
    if(!Device::this_device){
        // try to init local device
        Device::initLocalDevice(getOptionsHandle());
        if(!Device::this_device){
            return;
        }
    }
    if(Device::this_device && Device::this_device->type == DeviceType::Station){
        auto dmTick = safeSharedTickCall<DistanceMeter>(_dm);
        auto topologyTick = [this](TickType_t diff) { 
            if(this->_localization && Device::this_device && Device::this_device->fixed_location == false) 
                this->_localization->tick(diff); 
        };
        auto updateTick = [this](TickType_t diff) { if(this->_update_cb) this->_update_cb(diff); };
        setStateData(0, nullptr,      (rgb_t){  0,  0,255});
        setStateData(1, dmTick,       (rgb_t){255,  0,  0}); // red
        setStateData(2, topologyTick, (rgb_t){255,  0,255}); // pink
        setStateData(3, updateTick,   (rgb_t){  0,255,  0}); // green
    } else{
        auto updateTick = [this](TickType_t diff) {
            if(this->_dm) _dm->tick(diff);
            if(this->_localization) _localization->tick(diff);
            if(this->_update_cb) this->_update_cb(diff);
        };
        setStateData(0, nullptr,    (rgb_t){  0,  0,255});
        setStateData(1, nullptr,    (rgb_t){255,  0,  0}); // red
        setStateData(2, nullptr,    (rgb_t){255,  0,255}); // pink
        setStateData(3, updateTick, (rgb_t){  0,255,  0}); // green
    }
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

esp_err_t IMF::startLocalization(){
    if(_localization){
        return _localization->start();
    }
    return ESP_FAIL;
}
void IMF::stopLocalization(){
    if(_localization){
        _localization->stop();
    }
}