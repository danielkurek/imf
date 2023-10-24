#include "distance_meter.hpp"

static esp_err_t DistancePoint::initDistanceMeasurement(){
    s_ftm_event_group = xEventGroupCreate();

    return esp_event_handler_instance_register(WIFI_EVENT,
                                               WIFI_EVENT_FTM_REPORT,
                                               &DistancePoint::event_handler,
                                               NULL,
                                               NULL);
}

static void DistancePoint::event_handler(void* arg, esp_event_base_t event_base, 
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
        } else {
            ESP_LOGI(TAG_STA, "FTM procedure with Peer("MACSTR") failed! (Status - %d)",
                     MAC2STR(event->peer_mac), event->status);
            xEventGroupSetBits(_s_ftm_event_group, FTM_FAILURE_BIT);
        }
    }
}

uint32_t DistancePoint::measureDistance(){
    wifi_ftm_initiator_cfg_t ftmi_cfg {0};
    ftmi_cfg.resp_mac = _mac;
    ftmi_cfg.channel = channel;
    ftmi_cfg.frm_count = 16;
    ftmi_cfg.burst_period = 1;

    ftm_result_t ftm_report = measureRawDistance(&ftmi_cfg);
    // TODO: filter data

    return ftm_report.dist_est;
}

static ftm_result_t DistancePoint::measureRawDistance(wifi_ftm_initiator_cfg_t* ftmi_cfg){
    EventBits_t bits;

    esp_err_t err = esp_wifi_ftm_initiate_session(&ftmi_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start FTM session");
        return -1;
    }

    bits = xEventGroupWaitBits(_s_ftm_event_group, FTM_REPORT_BIT | FTM_FAILURE_BIT,
                                           pdTRUE, pdFALSE, portMAX_DELAY);
    if (bits & FTM_REPORT_BIT) {
        ftm_result_t ftm_result {};
        ftm_result.peer_mac = _s_ftm_report.peer_mac;
        ftm_result.status = _s_ftm_report.status;
        ftm_result.rtt_raw = _s_ftm_report.rtt_raw;
        ftm_result.rtt_est = _s_ftm_report.rtt_est;
        ftm_result.dist_est = _s_ftm_report.dist_est;
        //TODO: verify that this moving does not leak data
        for(uint8_t i = 0; i < _s_ftm_report.ftm_report_num_entries; i++){
            ftm_result.ftm_report_data.push_back(std::move(_s_ftm_report.ftm_report_data[i]));
        }
        _s_ftm_report.ftm_report_data = NULL;
        _s_ftm_report.ftm_report_num_entries = 0;
        return ftm_report;
    } else{
        // FTM failed
    }
}

DistanceMeter::DistanceMeter(bool wifi_initialized) : _points() {
    if(!wifi_initialized)
        wifiInit();
}
uint8_t DistanceMeter::addPoint(uint8_t[6] mac, uint8_t channel){
    _points.emplace_back(std::make_shared<DistancePoint>(mac, channel));
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
    xTaskCreate(TaskWrapper, "DistanceMeter", STACK_SIZE, this, configMAX_PRIORITIES, &_xHandle);
}
std::shared_ptr<DistancePoint> DistanceMeter::nearestDevice();
std::vector<std::shared_ptr<DistancePoint>> DistanceMeter::reachablePoints();
void DistanceMeter::task();