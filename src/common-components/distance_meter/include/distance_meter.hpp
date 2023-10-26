#ifndef FTM_INTERFACE_H_
#define FTM_INTERFACE_H_

#include <inttypes.h>
#include "esp_wifi_types.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_event.h"
#include <vector>
#include <memory>
#include <cstring>

typedef struct{
    uint8_t peer_mac[6];
    wifi_ftm_status_t status;
    uint32_t rtt_raw;
    uint32_t rtt_est;
    uint32_t dist_est;
    std::vector<wifi_ftm_report_entry_t> ftm_report_data;
} ftm_result_t;

typedef struct{
    uint32_t distance;
    TickType_t timestamp;
} distance_measurement_t;

class DistancePoint {
    public:
        DistancePoint(const uint8_t mac[6], uint8_t channel) 
            : _channel(channel), _macstr(MAC2STR(mac)) {
                memcpy(_mac, mac, 6);
            }
        static esp_err_t initDistanceMeasurement();
        uint32_t measureDistance();
        static ftm_result_t measureRawDistance(
                wifi_ftm_initiator_cfg_t* ftmi_conf);
        const uint8_t* getMac() { return _mac; }
        const std::string getMacStr() { return _macstr; }
        uint8_t getChannel() { return _channel; }
    private:
        static void event_handler(void* arg, esp_event_base_t event_base, 
            int32_t event_id, void* event_data);
        static const int FTM_REPORT_BIT = BIT0;
        static const int FTM_FAILURE_BIT = BIT1;
        static EventGroupHandle_t _s_ftm_event_group;
        static wifi_event_ftm_report_t _s_ftm_report;
        uint8_t _mac[6];
        std::string _macstr;
        uint8_t _channel;
};

class DistanceMeter{
    public:
        DistanceMeter(bool wifi_initialized = false);
        uint8_t addPoint(uint8_t mac[6], uint8_t channel);
        // esp_err_t removePoint(uint8_t mac[6]);
        void startTask();
        // nearest point in x amount of seconds
        std::shared_ptr<DistancePoint> nearestPoint();
    private:
        std::vector<std::shared_ptr<DistancePoint>> reachablePoints();
        static void taskWrapper(void* param){
            static_cast<DistanceMeter *>(param)->task();
        }
        void task();
        std::unordered_map<std::string, std::shared_ptr<DistancePoint>> _points;
        std::unordered_map<std::string, distance_measurement_t> _measurements;
        uint32_t time_threshold = 10_000 * (1000 / configTICK_RATE_HZ);
        TaskHandle_t _xHandle = NULL;

};


#endif