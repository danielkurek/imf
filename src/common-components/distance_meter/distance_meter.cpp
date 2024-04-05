#include "distance_meter.hpp"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include <stdint.h>

#define EVENT_LOOP_QUEUE_SIZE 16

ESP_EVENT_DEFINE_BASE(DM_EVENT);

static const char *TAG = "DM";

EventGroupHandle_t DistancePoint::_s_ftm_event_group {};
wifi_event_ftm_report_t DistancePoint::_s_ftm_report{};

esp_err_t DistancePoint::initDistanceMeasurement(){
    _s_ftm_event_group = xEventGroupCreate();

    return esp_event_handler_instance_register(WIFI_EVENT,
                                               WIFI_EVENT_FTM_REPORT,
                                               &DistancePoint::event_handler,
                                               NULL,
                                               NULL);
}

void DistancePoint::event_handler(void* arg, esp_event_base_t event_base, 
            int32_t event_id, void* event_data){
    if (event_id == WIFI_EVENT_FTM_REPORT) {
        wifi_event_ftm_report_t *event = (wifi_event_ftm_report_t *) event_data;

        if (event->status == FTM_STATUS_SUCCESS) {
            if(_s_ftm_report.ftm_report_data){
                free(_s_ftm_report.ftm_report_data);
                _s_ftm_report.ftm_report_data = NULL;
                _s_ftm_report.ftm_report_num_entries = 0;
            }
            _s_ftm_report.rtt_raw = event->rtt_raw;
            _s_ftm_report.rtt_est = event->rtt_est;
            _s_ftm_report.dist_est = event->dist_est;
            _s_ftm_report.ftm_report_data = event->ftm_report_data;
            _s_ftm_report.ftm_report_num_entries = event->ftm_report_num_entries;
            xEventGroupSetBits(_s_ftm_event_group, FTM_REPORT_BIT);
            ESP_LOGI(TAG, "FTM measurement success");
        } else {
            ESP_LOGW(TAG, "FTM procedure with Peer(" MACSTR ") failed! (Status - %d)",
                     MAC2STR(event->peer_mac), event->status);
            xEventGroupSetBits(_s_ftm_event_group, FTM_FAILURE_BIT);
        }
    }
}

uint32_t DistancePoint::filterDistance(uint32_t new_measurement){
    while(_filter_data.size() >= _filter_max_size){
        _filter_sum -= _filter_data.front();
        _filter_data.pop_front();
    }
    _filter_data.push_back(new_measurement);
    _filter_sum += new_measurement;

    return _filter_sum / _filter_data.size();
}

esp_err_t DistancePoint::measureDistance(uint32_t &distance_cm){
    wifi_ftm_initiator_cfg_t ftmi_cfg {};
    memcpy(ftmi_cfg.resp_mac, _mac, 6);
    ftmi_cfg.channel = _channel;
    ftmi_cfg.frm_count = 16;
    ftmi_cfg.burst_period = 2;

    ftm_result_t ftm_report = measureRawDistance(&ftmi_cfg);

    if(ftm_report.status == FTM_STATUS_SUCCESS){
        distance_cm = filterDistance(ftm_report.dist_est);

        // allow _latest_log = -1 but not anything other
        if(_latest_log + 1 < 0) _latest_log = 0;
        size_t next_log = _latest_log + 1;
        if(next_log >= log_size){
            ESP_LOGI(TAG, "resetting log index %d+1 (%d) to 0 (log_size=%d)", _latest_log, next_log, log_size);
            next_log = 0;
        }
        assert(next_log < log_size);
        _distance_log[next_log].timestamp = xTaskGetTickCount();
        _distance_log[next_log].distance_cm = distance_cm;
        _latest_log = next_log;
        return ESP_OK;
    }
    return ESP_FAIL;
}

ftm_result_t DistancePoint::measureRawDistance(wifi_ftm_initiator_cfg_t* ftmi_cfg){
    EventBits_t bits;

    esp_err_t err = esp_wifi_ftm_initiate_session(ftmi_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start FTM session");
        return {};
    }
    // clear bits so that timeout of xEventGroupWaitBits does not have bits set from previous event
    xEventGroupClearBits(_s_ftm_event_group, FTM_REPORT_BIT | FTM_FAILURE_BIT);
    bits = xEventGroupWaitBits(_s_ftm_event_group, FTM_REPORT_BIT | FTM_FAILURE_BIT,
                                           pdTRUE, pdFALSE, portMAX_DELAY);
    if (bits & FTM_REPORT_BIT) {
        ftm_result_t ftm_result {};
        memcpy(ftm_result.peer_mac, _s_ftm_report.peer_mac, 6);
        ftm_result.status = _s_ftm_report.status;
        ftm_result.rtt_raw = _s_ftm_report.rtt_raw;
        ftm_result.rtt_est = _s_ftm_report.rtt_est;
        ftm_result.dist_est = _s_ftm_report.dist_est;
        for(uint8_t i = 0; i < _s_ftm_report.ftm_report_num_entries; i++){
            ftm_result.ftm_report_data.push_back(_s_ftm_report.ftm_report_data[i]);
        }
        free(_s_ftm_report.ftm_report_data);
        _s_ftm_report.ftm_report_data = NULL;
        _s_ftm_report.ftm_report_num_entries = 0;
        return ftm_result;
    } else{
        // FTM failed
        ftm_result_t ftm_result {};
        ftm_result.status = FTM_STATUS_FAIL;
        ftm_result.dist_est = UINT32_MAX;
        return ftm_result;
    }
    return {};
}

esp_err_t DistancePoint::getDistanceFromLog(distance_measurement_t &measurement, size_t offset){
    ESP_LOGI(TAG, "getDistanceFromLog offset=%d _latest_log=%d", offset, _latest_log);
    if(_latest_log < 0) return ESP_FAIL;
    if(offset >= log_size) return ESP_FAIL;
    size_t index = (_latest_log + offset) % log_size;
    assert(index < log_size);
    ESP_LOGI(TAG, "returning %d, distance %" PRIu32, index, _distance_log[index].distance_cm);
    measurement.timestamp = _distance_log[index].timestamp;
    measurement.distance_cm = _distance_log[index].distance_cm;
    return ESP_OK;
}

DistanceMeter::DistanceMeter(bool wifi_initialized) : _points() {
    esp_event_loop_args_t loop_args{
        EVENT_LOOP_QUEUE_SIZE, // queue_size
        "DM-loop",             // task_name
        tskIDLE_PRIORITY,      // task_priority
        1024*4,                // task_stack_size
        tskNO_AFFINITY         // task_core_id
    };

    if (esp_event_loop_create(&loop_args, &_event_loop_hdl) != ESP_OK) {
        ESP_LOGE(TAG, "create event loop failed");
    }

    esp_err_t err = DistancePoint::initDistanceMeasurement();
    if(err != ESP_OK){
        ESP_LOGE(TAG, "DistancePoint initDistanceMeasurement failed! %d", err);
    }
}

DistanceMeter::DistanceMeter(bool wifi_initialized, esp_event_loop_handle_t event_loop_handle) : _points() {
    _event_loop_hdl = event_loop_handle;
    esp_err_t err = DistancePoint::initDistanceMeasurement();
    if(err != ESP_OK){
        ESP_LOGE(TAG, "DistancePoint initDistanceMeasurement failed! %d", err);
    }
}

uint32_t DistanceMeter::addPoint(uint8_t mac[6], uint8_t channel){
    char buffer[17+1];
    sprintf(buffer, MACSTR, MAC2STR(mac));
    std::string macstr (buffer);

    return _addPoint(mac, macstr, channel);
}

uint32_t DistanceMeter::addPoint(std::string macstr, uint8_t channel){
    uint8_t mac[6];
    int ret = sscanf(macstr.c_str(), MACSTR_SCN, STR2MAC(mac));
    if(ret != 6){
        ESP_LOGE(TAG, "Error parsing MAC address, parsed %d, expected 6! \"%s\"", ret, macstr.c_str());
        return UINT32_MAX;
    }

    return _addPoint(mac, macstr, channel);
}

uint32_t DistanceMeter::_addPoint(const uint8_t mac[6], std::string macstr, uint8_t channel){
    if(_next_id == UINT32_MAX){
        return UINT32_MAX;
    }

    uint32_t id;
    auto search = _points_mac_id.find(macstr);
    if(search != _points_mac_id.end()){
        return search->second; // MAC is already added
    } else{
        id = _next_id;
        _next_id += 1;
        ESP_LOGI(TAG, "Added point: [%s] channel %d", macstr.c_str(), channel);
        _points.emplace(id, std::make_shared<DistancePoint>(id, mac, macstr, channel));
        _points_mac_id.emplace(macstr, id);
        return id;
    }
}

std::shared_ptr<DistancePoint> DistanceMeter::getPoint(uint32_t id){
    auto search = _points.find(id);
    if(search != _points.end()){
        return search->second;
    }
    return nullptr;
}

void DistanceMeter::startTask(){
    stopTask();
    // STACK_SIZE=1024*2???
    ESP_LOGI(TAG, "Starting DistanceMeter task");
    auto ret = xTaskCreatePinnedToCore(taskWrapper, "DistanceMeter", 1024*8, this, tskIDLE_PRIORITY+1, &_xHandle, 1);
    if(ret != pdPASS){
        ESP_LOGE(TAG, "Could not create DistanceMeter task");
    }
}

void DistanceMeter::stopTask(){
    if(_xHandle != NULL){
        vTaskDelete(_xHandle);
        _xHandle = NULL;
    }
}

static uint32_t nearestDeviceDistanceFunction(const distance_measurement_t& measurement, TickType_t now){
    TickType_t time_diff = (pdTICKS_TO_MS(now) - pdTICKS_TO_MS(measurement.timestamp));
    // in 10s travel by 3m = 300 cm in 10000 ms
    return measurement.distance_cm + (time_diff * (300 / 10000));
}

std::shared_ptr<DistancePoint> DistanceMeter::nearestPoint() {
    TickType_t now = xTaskGetTickCount();

    uint32_t best_id;
    // TickType_t best_time;
    uint32_t best_dist = UINT32_MAX;
    bool first = true;
    distance_measurement_t measurement;
    esp_err_t err;
    for(const auto& [id, point] : _points){
        err = point->getDistanceFromLog(measurement);
        if(err != ESP_OK) continue;

        uint32_t transformed_distance = nearestDeviceDistanceFunction(measurement, now);
        if(first || transformed_distance < best_dist){
            first = false;
            best_dist = transformed_distance;
            best_id = id;
        }
    }
    if(first){
        // no nearest point was found
        return nullptr;
    }
    return _points[best_id];
}
std::vector<std::shared_ptr<DistancePoint>> DistanceMeter::reachablePoints(){
    static uint16_t g_scan_ap_num;
    static wifi_ap_record_t *g_ap_list_buffer;

    std::vector<std::shared_ptr<DistancePoint>> result;

    wifi_scan_config_t scan_config {};
    esp_err_t err;
    uint8_t i;

    err = esp_wifi_scan_start(&scan_config, true);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "Failed to perform scan");
        return {};
    }

    esp_wifi_scan_get_ap_num(&g_scan_ap_num);
    if (g_scan_ap_num == 0) {
        ESP_LOGI(TAG, "No matching AP found");
        return {};
    }

    if (g_ap_list_buffer) {
        free(g_ap_list_buffer);
    }

    g_ap_list_buffer = (wifi_ap_record_t*) malloc(g_scan_ap_num * sizeof(wifi_ap_record_t));
    if (g_ap_list_buffer == NULL) {
        ESP_LOGE(TAG, "Failed to malloc buffer to print scan results");
        return {};
    }

    err = esp_wifi_scan_get_ap_records(&g_scan_ap_num, (wifi_ap_record_t *)g_ap_list_buffer);
    if (err == ESP_OK) {
        for (i = 0; i < g_scan_ap_num; i++) {
            char buffer[17+1];
            sprintf(buffer, MACSTR, MAC2STR(g_ap_list_buffer[i].bssid));
            std::string mac {buffer};
            if(_points_mac_id.contains(mac) && g_ap_list_buffer[i].ftm_responder){
                result.push_back(_points[_points_mac_id[mac]]);
            }
            ESP_LOGI(TAG, "[%s][%s][rssi=%d]""%s", mac.c_str(), g_ap_list_buffer[i].ssid, g_ap_list_buffer[i].rssi,
                        g_ap_list_buffer[i].ftm_responder ? "[FTM Responder]" : "");
        }
    }

    ESP_LOGI(TAG, "sta scan done");
    return result;
}

void DistanceMeter::tick(TickType_t diff){
    static std::shared_ptr<DistancePoint> s_nearest_point = nullptr;
    esp_err_t err;

    TickType_t now = xTaskGetTickCount();

    // auto points = reachablePoints();
    // ESP_LOGI(TAG, "%d reachable points", points.size());
    ESP_LOGI(TAG, "Measuring distance to %d points", _points.size());
    for(const auto& [key, point] : _points){
        uint32_t distance_cm = UINT32_MAX;
        err = point->measureDistance(distance_cm);
        bool valid = err == ESP_OK;
        uint32_t point_id = point->getID();
        dm_measurement_data_t event_data;
        event_data.point_id = point_id;
        event_data.distance_cm = distance_cm;
        event_data.valid = valid;
        if(valid)
            ESP_LOGI(TAG, "Distance to point %" PRIu32 " is %" PRIu32, point_id, distance_cm);
        err = esp_event_post_to(_event_loop_hdl, DM_EVENT, DM_MEASUREMENT_DONE, &event_data, sizeof(event_data), pdMS_TO_TICKS(10));
        if(err != ESP_OK){
            ESP_LOGE(TAG, "failed to post an event! %s", esp_err_to_name(err));
        }
    }

    now = xTaskGetTickCount();

    auto nearest_point = nearestPoint();
    
    if(nearest_point == nullptr){
        return;
    }

    // check if this device is in near proximity
    distance_measurement_t measurement;
    err = nearest_point->getDistanceFromLog(measurement);
    if(err == ESP_OK){
        auto nearest_distance = nearestDeviceDistanceFunction(measurement, now);
        if(nearest_distance >= _distance_threshold_cm){
            nearest_point = nullptr;
        }   
    } else{
        // fault happened -> reset nearest_point
        nearest_point = nullptr;
    }

    if(nearest_point != s_nearest_point){
        // when posting to event loop, the data are copied so we can use one instance for multiple events
        dm_nearest_device_change_t event_data;
        event_data.timestamp_ms = pdTICKS_TO_MS(xTaskGetTickCount());
        if(s_nearest_point){
            event_data.old_point_id = s_nearest_point->getID();;
        } else{
            event_data.old_point_id = UINT32_MAX;
        }
        
        if(nearest_point){
            event_data.new_point_id = nearest_point->getID();
        } else{
            event_data.new_point_id = UINT32_MAX;
        }

        s_nearest_point = nearest_point;
        err = esp_event_post_to(_event_loop_hdl, DM_EVENT, DM_NEAREST_DEVICE_CHANGE, &event_data, sizeof(event_data), pdMS_TO_TICKS(10));
        if(err != ESP_OK){
            ESP_LOGE(TAG, "failed to post an event! %s", esp_err_to_name(err));
        }
    }
}

void DistanceMeter::task(){
    while(true){
        vTaskDelay(500 / portTICK_PERIOD_MS); // allow other tasks to run
        
        tick(500 / portTICK_PERIOD_MS);
    }

}

esp_err_t DistanceMeter::registerEventHandle(esp_event_handler_t event_handler, void *handler_args){
    esp_err_t err = esp_event_handler_register_with(_event_loop_hdl, DM_EVENT, ESP_EVENT_ANY_ID, event_handler, handler_args);
    if(err != ESP_OK){
        ESP_LOGE(TAG, "Cannot register DM_EVENT handler");
        return err;
    }
    return err;
}