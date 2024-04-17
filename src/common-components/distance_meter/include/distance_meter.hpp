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
#include "esp_mac.h"

#include <set>
#include <unordered_map>
#include <deque>

#define MACSTR_SCN "%02" SCNx8 ":%02" SCNx8 ":%02" SCNx8 ":%02" SCNx8 ":%02" SCNx8 ":%02" SCNx8
#define STR2MAC(a) &(a)[0], &(a)[1], &(a)[2], &(a)[3], &(a)[4], &(a)[5]

ESP_EVENT_DECLARE_BASE(DM_EVENT);
typedef enum {
    DM_MEASUREMENT_DONE,
    DM_NEAREST_DEVICE_CHANGE,
} dm_event_t;

typedef struct {
    uint32_t old_point_id;
    uint32_t new_point_id;
    TickType_t timestamp_ms;
} dm_nearest_device_change_t;

typedef struct{
    uint8_t peer_mac[6];
    wifi_ftm_status_t status;
    uint32_t rtt_raw;
    uint32_t rtt_est;
    uint32_t dist_est;
    std::vector<wifi_ftm_report_entry_t> ftm_report_data;
} ftm_result_t;

typedef struct{
    uint32_t distance_cm;
    int8_t rssi;
} distance_measurement_t;

typedef struct{
    distance_measurement_t measurement;
    TickType_t timestamp;
} distance_log_t;

typedef struct{
    uint32_t point_id;
    distance_measurement_t measurement;
    bool valid;
} dm_measurement_data_t;

class DistancePoint {
    public:
        DistancePoint(uint32_t id, const uint8_t mac[6], std::string macstr, uint8_t channel, size_t filter_max_size)
            : _id(id), _macstr(macstr), _channel(channel), _filter_max_size(filter_max_size){
            memcpy(_mac, mac, 6);
        }
        DistancePoint(uint32_t id, const uint8_t mac[6], std::string macstr, uint8_t channel) 
            : DistancePoint(id, mac, macstr, channel, CONFIG_DISTANCE_FILTER_MAX_SIZE_DEFAULT){
                
            }
        DistancePoint(uint32_t id, const uint8_t mac[6], uint8_t channel) 
            : _id(id), _channel(channel), _filter_max_size(CONFIG_DISTANCE_FILTER_MAX_SIZE_DEFAULT){
                memcpy(_mac, mac, 6);
                char buffer[17+1];
                sprintf(buffer, MACSTR, MAC2STR(_mac));
                _macstr = std::string(buffer);
            }
        
        // default event loop needs to be created before calling this function
        static esp_err_t initDistanceMeasurement();
        esp_err_t measureDistance(distance_measurement_t &measurement);
        static ftm_result_t measureRawDistance(
                wifi_ftm_initiator_cfg_t* ftmi_conf);
        const uint8_t* getMac() { return _mac; }
        const std::string getMacStr() { return _macstr; }
        uint8_t getChannel() { return _channel; }
        uint32_t getID() { return _id; }
        esp_err_t getDistanceFromLog(distance_log_t &measurement, size_t offset = 0);
        esp_err_t setFrameCount(uint8_t frm_count);
        esp_err_t setBurstPeriod(uint16_t burst_period);
        static constexpr size_t log_size = 5;
    private:
        static void event_handler(void* arg, esp_event_base_t event_base, 
            int32_t event_id, void* event_data);
        static const int FTM_REPORT_BIT = BIT0;
        static const int FTM_FAILURE_BIT = BIT1;
        static EventGroupHandle_t _s_ftm_event_group;
        static wifi_event_ftm_report_t _s_ftm_report;
        uint32_t distanceCorrection(uint32_t distance_cm);
        distance_measurement_t filterDistance(const distance_measurement_t &new_measurement);
        uint32_t _id;
        uint8_t _mac[6];
        std::string _macstr;
        uint8_t _channel;
        uint8_t _frm_count = 16;
        uint16_t _burst_period = 0;
        size_t _filter_max_size;
        std::deque<distance_measurement_t> _filter_data;
        uint64_t _filter_distance_sum = 0;
        int64_t _filter_rssi_sum = 0;
        int _latest_log = -1;
        distance_log_t _distance_log[log_size];
};

class DistanceMeter{
    public:
        DistanceMeter(bool wifi_initialized, bool only_reachable = false);
        DistanceMeter(bool wifi_initialized, esp_event_loop_handle_t event_loop_handle, bool only_reachable = false);

        // user specified ID can be passed as parameter, if it is UINT32_MAX, ID will be assigned as the next largest ID added to this point
        // return   ID of added Point (if point already is added, ID of the existing point is returned)
        //          UINT32_MAX means an error occurred (ran out of IDs or something else)
        uint32_t addPoint(uint8_t mac[6], uint8_t channel, uint32_t id = UINT32_MAX);
        uint32_t addPoint(std::string macstr, uint8_t channel, uint32_t id = UINT32_MAX);
        std::shared_ptr<DistancePoint> getPoint(uint32_t id);
        void tick(TickType_t diff);
        void startTask();
        void stopTask();
        // nearest point in last x amount of seconds
        std::shared_ptr<DistancePoint> nearestPoint();
        esp_err_t registerEventHandle(esp_event_handler_t event_handler, void *handler_args);
    private:
        uint32_t _addPoint(const uint8_t mac[6], std::string macstr, uint8_t channel, uint32_t id);
        esp_err_t measureDistance(std::shared_ptr<DistancePoint> point);
        std::vector<std::shared_ptr<DistancePoint>> reachablePoints();
        static void taskWrapper(void* param){
            static_cast<DistanceMeter *>(param)->task();
        }
        void task();
        bool _only_reachable;
        std::unordered_map<uint32_t, std::shared_ptr<DistancePoint>> _points;
        std::unordered_map<std::string, uint32_t> _points_mac_id;
        esp_event_loop_handle_t _event_loop_hdl;
        const uint32_t time_threshold = 10000 * (1000 / configTICK_RATE_HZ);
        const uint32_t _distance_threshold_cm = 10 * 100;
        TaskHandle_t _xHandle = NULL;
        uint32_t _next_id = 0;
};


#endif