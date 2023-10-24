#ifndef FTM_INTERFACE_H_
#define FTM_INTERFACE_H_

#include <inttypes.h>

typedef struct{
    const uint8_t peer_mac[6];
    wifi_ftm_status_t status;
    uint32_t rtt_raw;
    uint32_t rtt_est;
    uint32_t dist_est;
    std::vector<wifi_ftm_report_entry_t> ftm_report_data;
} ftm_result_t;

class DistancePoint {
    public:
        DistancePoint(uint8_t[6] mac, uint8_t channel) 
            : _mac(mac), _channel(channel) {}
        static esp_err_t initDistanceMeasurement();
        uint32_t measureDistance();
        static ftm_result_t measureRawDistance(
                wifi_ftm_initiator_cfg_t* ftmi_conf);
        const uint8_t[6] getMac() { return _mac; }
        const uint8_t getChannel() { return _channel; }
    private:
        static void event_handler(void* arg, esp_event_base_t event_base, 
            int32_t event_id, void* event_data);
        static const int FTM_REPORT_BIT = BIT0;
        static const int FTM_FAILURE_BIT = BIT1;
        static EventGroupHandle_t _s_ftm_event_group;
        static wifi_event_ftm_report_t _s_ftm_report;
        uint8_t[6] _mac;
        uint8_t _channel;
}

class DistanceMeter{
    public:
        DistanceMeter(bool wifi_initialized = false);
        esp_err_t wifiInit();
        uint8_t addPoint(uint8_t[6] mac);
        // esp_err_t removePoint(uint8_t [6] mac);
        void startTask();
        std::shared_ptr<DistancePoint> nearestDevice();
    private:
        std::vector<std::shared_ptr<DistancePoint>> reachablePoints();
        static void taskWrapper(void* param){
            static_cast<SerialCommSrc *>(param)->Task();
        }
        void task();
        std::vector<std::shared_ptr<DistancePoint>> _points;
        TaskHandle_t _xHandle = NULL;

};


#endif