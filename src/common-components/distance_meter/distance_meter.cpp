#include "distance_meter.hpp"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include <stdint.h>

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
            // ESP_LOGI(TAG, "FTM procedure with Peer(" MACSTR ") failed! (Status - %d)",
            //          MAC2STR(event->peer_mac), event->status);
            xEventGroupSetBits(_s_ftm_event_group, FTM_FAILURE_BIT);
        }
    }
}

uint32_t DistancePoint::measureDistance(){
    wifi_ftm_initiator_cfg_t ftmi_cfg {};
    memcpy(ftmi_cfg.resp_mac, _mac, 6);
    ftmi_cfg.channel = _channel;
    ftmi_cfg.frm_count = 16;
    ftmi_cfg.burst_period = 2;

    ftm_result_t ftm_report = measureRawDistance(&ftmi_cfg);
    // TODO: filter data

    if(ftm_report.status != FTM_STATUS_SUCCESS)
        return UINT32_MAX;
    return ftm_report.dist_est;
}

ftm_result_t DistancePoint::measureRawDistance(wifi_ftm_initiator_cfg_t* ftmi_cfg){
    EventBits_t bits;

    esp_err_t err = esp_wifi_ftm_initiate_session(ftmi_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start FTM session");
        return {};
    }

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

DistanceMeter::DistanceMeter(bool wifi_initialized) : _points() {
    
}

uint8_t DistanceMeter::addPoint(uint8_t mac[6], uint8_t channel){
    char buffer[17+1];
    sprintf(buffer, MACSTR, MAC2STR(mac));
    std::string macstr (buffer);
    if(!_points.contains(macstr)){
        ESP_LOGI(TAG, "Added point: [%s] channel %d", macstr.c_str(), channel);
        _points.emplace(macstr, std::make_shared<DistancePoint>(mac, macstr, channel));
    }

    ESP_LOGI(TAG, "Tracked points:");
    for(const auto& [key,point] : _points){
        ESP_LOGI(TAG, "[%s] channel=%d", key.c_str(), point->getChannel());
    }
    return 0;
}
// esp_err_t DistanceMeter::removePoint(uint8_t[6] mac){
//     for(auto it = _points.begin(); it != _points.end(); it++){
//         const uint8_t[6] other_mac = *it->getMac();
//         bool equal = true;
//         for(int i = 0; i < 6; i++){
//             if(mac[i] != other_mac[i]){
//                 equal = false;
//                 break;
//             }
//         }

//         if(equal){
//             _points.erase(it);
//             return ESP_OK;
//         }
//     }
//     return ESP_FAIL;
// }

void DistanceMeter::startTask(){
    if(_xHandle != NULL){
        // delete and start the task again or do nothing
        vTaskDelete(_xHandle);
    }
    // STACK_SIZE=1024*2???
    xTaskCreate(taskWrapper, "DistanceMeter", 1024*8, this, configMAX_PRIORITIES, &_xHandle);
}

static uint32_t nearestDeviceDistanceFunction(std::shared_ptr<distance_measurement_t> measurement, TickType_t now){
    TickType_t time_diff = (pdTICKS_TO_MS(now) - pdTICKS_TO_MS(measurement->timestamp));
    // in 10s travel by 3m = 300 cm in 10000 ms
    return measurement->distance_cm + (time_diff * (300 / 10000));
}

std::shared_ptr<DistancePoint> DistanceMeter::nearestPoint() {
    TickType_t now = xTaskGetTickCount();

    std::string best_mac;
    // TickType_t best_time;
    uint32_t best_dist = UINT32_MAX;
    bool first = true;
    for(const auto& [mac, measurement] : _measurements){
        uint32_t transformed_distance = nearestDeviceDistanceFunction(measurement, now);
        if(first || transformed_distance < best_dist){
            first = false;
            best_dist = transformed_distance;
            best_mac = mac;
        }
    }
    if(first){
        // no nearest point was found
        return nullptr;
    }
    return _points[best_mac];
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
            if(_points.contains(mac) && g_ap_list_buffer[i].ftm_responder){
                result.push_back(_points[mac]);
            }
            ESP_LOGI(TAG, "[%s][%s][rssi=%d]""%s", mac.c_str(), g_ap_list_buffer[i].ssid, g_ap_list_buffer[i].rssi,
                        g_ap_list_buffer[i].ftm_responder ? "[FTM Responder]" : "");
        }
    }

    ESP_LOGI(TAG, "sta scan done");
    return result;
}
void DistanceMeter::task(){
    static std::shared_ptr<DistancePoint> s_nearest_point = nullptr;
    while(true){
        TickType_t now = xTaskGetTickCount();

        // auto points = reachablePoints();
        // ESP_LOGI(TAG, "%d reachable points", points.size());
        for(const auto& [key, point] : _points){
            uint32_t distance_cm = point->measureDistance();
            const std::string point_mac = point->getMacStr();
            auto search = _measurements.find(point_mac);
            if(search != _measurements.end()){
                _measurements[point_mac]->distance_cm = distance_cm;
                _measurements[point_mac]->timestamp = now;
            } else{
                _measurements.emplace(point_mac, std::make_shared<distance_measurement_t>(distance_cm, now));
            }
            dm_measurement_data_t event_data;
            memcpy(event_data.peer_mac, point->getMac(), 6);
            event_data.distance_cm = distance_cm;
            esp_event_post(DM_EVENT, DM_MEASUREMENT_DONE, &event_data, sizeof(event_data), pdMS_TO_TICKS(10));

            if(distance_cm != UINT32_MAX)
                ESP_LOGI(TAG, "Distance for point %s is %" PRIu32, point->getMacStr().c_str(), distance_cm);
        }

        now = xTaskGetTickCount();

        auto nearest_point = nearestPoint();
        
        // check if this device is in near proximity
        auto nearest_distance = nearestDeviceDistanceFunction(_measurements[nearest_point->getMacStr()], now);
        if(nearest_distance >= _distance_threshold_cm){
            nearest_point = nullptr;
        }

        if(nearest_point != s_nearest_point){
            std::string mac;
            if(s_nearest_point){
                mac = s_nearest_point->getMacStr();
                esp_event_post(DM_EVENT, DM_NEAREST_DEVICE_LEAVE, mac.c_str(), mac.size()+1, pdMS_TO_TICKS(10));
            } else{
                esp_event_post(DM_EVENT, DM_NEAREST_DEVICE_LEAVE, NULL, 0, pdMS_TO_TICKS(10));
            }
            
            s_nearest_point = nearest_point;

            if(nearest_point){
                mac = nearest_point->getMacStr();
                esp_event_post(DM_EVENT, DM_NEAREST_DEVICE_ENTER, mac.c_str(), mac.size()+1, pdMS_TO_TICKS(10));
            } else{
                esp_event_post(DM_EVENT, DM_NEAREST_DEVICE_ENTER, NULL, 0, pdMS_TO_TICKS(10));
            }
        }

        vTaskDelay(500 / portTICK_PERIOD_MS); // allow other tasks to run
    }

}