/**
 * @file distance_meter.hpp
 * @author Daniel Kurek (daniel.kurek.dev@gmail.com)
 * @brief Measures distances using WiFi FTM
 * @version 0.1
 * @date 2023-10-24
 * 
 * @copyright Copyright (c) 2023
 * 
 */

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

// definitions of MAC string for scanf
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

/**
 * @brief Represents a point to which we can measure distance
 * 
 */
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
        
        /**
         * @brief Initialize WiFi FTM distance measurement
         * 
         * default event loop needs to be created before calling this function
         * 
         * @return esp_err_t returns ESP_OK if succeeds
         */
        static esp_err_t initDistanceMeasurement();

        /**
         * @brief Start distance measurement and wait for result
         * 
         * applies distance correction and filtering
         * 
         * @param measurement resulting measurement
         * @return esp_err_t returns ESP_OK if succeeds 
         */
        esp_err_t measureDistance(distance_measurement_t &measurement);

        /**
         * @brief Raw distance measurement using WiFi FTM
         * 
         * @param ftmi_conf WiFi FTM configuration
         * @return ftm_result_t raw measurement results
         */
        static ftm_result_t measureRawDistance(wifi_ftm_initiator_cfg_t* ftmi_conf);

        const uint8_t* getMac() { return _mac; }
        const std::string getMacStr() { return _macstr; }
        uint8_t getChannel() { return _channel; }
        uint32_t getID() { return _id; }

        /**
         * @brief Get Distance measurement From Log (history of last \ref log_size measurements)
         * 
         * @param measurement output
         * @param offset from 0=latest to \ref log_size = oldest
         * @return esp_err_t 
         */
        esp_err_t getDistanceFromLog(distance_log_t &measurement, size_t offset = 0);
        /**
         * @brief Set the Frame Count of WiFi FTM measurements
         * 
         * @param frm_count number of frames in one FTM measurement (allowed values 0=no preference/16/24/32/64)
         * @return esp_err_t returns ESP_OK if value is valid and set
         */
        esp_err_t setFrameCount(uint8_t frm_count);
        /**
         * @brief Set the Burst Period of WiFi FTM measurements
         * 
         * @param burst_period delay between bursts of FTM frames in 100ms (allowed values 0=no preference/2-255)
         * @return esp_err_t returns ESP_OK if value is valid and set
         */
        esp_err_t setBurstPeriod(uint16_t burst_period);

        /**
         * @brief number of measurements kept in log (history)
         */
        static constexpr size_t log_size = 5;
    private:
        /**
         * @brief Handles events for WiFi FTM results
         */
        static void event_handler(void* arg, esp_event_base_t event_base, 
            int32_t event_id, void* event_data);
        
        /**
         * @brief bit signaling FTM success (results are passed through @ref _s_ftm_report)
         * used in waiting for FTM result - xEventGroupWaitBits()
         */
        static const int FTM_REPORT_BIT = BIT0;
        /**
         * @brief bit signaling FTM failure - no results are passed
         * used in waiting for FTM result - xEventGroupWaitBits()
         */
        static const int FTM_FAILURE_BIT = BIT1;
        /**
         * @brief event group used for waiting for FTM result (used in xEventGroupWaitBits())
         */
        static EventGroupHandle_t _s_ftm_event_group;
        /**
         * @brief static variable to pass result from event_handler() to individual instances
         */
        static wifi_event_ftm_report_t _s_ftm_report;

        /**
         * @brief Apply correction to measured distance
         * 
         * @param distance_cm raw distance measured (in cm)
         * @return uint32_t corrected distance (in cm)
         */
        uint32_t distanceCorrection(uint32_t distance_cm);

        /**
         * @brief Filter distance measurements (can be after correction)
         * 
         * @param new_measurement newly measured distance
         * @return distance_measurement_t filtered distance
         */
        distance_measurement_t filterDistance(const distance_measurement_t &new_measurement);
        uint32_t _id;
        uint8_t _mac[6];
        std::string _macstr;
        uint8_t _channel;
        /**
         * @brief frame count of FTM measurement (allowed values 0=no preference/16/24/32/64)
         */
        uint8_t _frm_count = 16;
        /**
         * @brief delay between bursts of FTM frames in 100ms (allowed values 0=no preference/2-255)
         */
        uint16_t _burst_period = 0;
        size_t _filter_max_size;
        std::deque<distance_measurement_t> _filter_data;
        /**
         * @brief rolling sum of all distances in _filter_data
         */
        uint64_t _filter_distance_sum = 0;
        /**
         * @brief rolling sum of all RSSIs in _filter_data
         */
        int64_t _filter_rssi_sum = 0;
        int _latest_log = -1;
        distance_log_t _distance_log[log_size];
};

/**
 * @brief Manages measuring distances to instances of DistancePoint
 * 
 */
class DistanceMeter{
    public:
        DistanceMeter(bool wifi_initialized, bool only_reachable = false);
        DistanceMeter(bool wifi_initialized, esp_event_loop_handle_t event_loop_handle, bool only_reachable = false);

        /**
         * @brief Create DistancePoint that will be managed by DistanceMeter
         * 
         * @param mac WiFi MAC address of the point
         * @param channel WiFi channel of the point
         * @param id requested id of the created point (UINT32_MAX=no preference -> `largest_id`+1 will be assigned)
         * @return uint32_t resulting id of created point or id of already created point with same mac, 
         *                  UINT32_MAX if id is used by other point or an error occurred
         */
        uint32_t addPoint(uint8_t mac[6], uint8_t channel, uint32_t id = UINT32_MAX);
        /**
         * @brief Create DistancePoint that will be managed by DistanceMeter
         * 
         * @param macstr WiFi MAC address of the point in string form ("xx:xx:xx:xx:xx:xx")
         * @param channel WiFi channel of the point
         * @param id requested id of the created point (UINT32_MAX=no preference -> `largest_id`+1 will be assigned)
         * @return uint32_t resulting id of created point or id of already created point with same mac, 
         *                  UINT32_MAX if id is used by other point or an error occurred
         */
        uint32_t addPoint(std::string macstr, uint8_t channel, uint32_t id = UINT32_MAX);
        /**
         * @brief Get Distance Point
         * 
         * @param id id of the Distance point
         * @return std::shared_ptr<DistancePoint> return shared_ptr of the point or nullptr if point is not found
         */
        std::shared_ptr<DistancePoint> getPoint(uint32_t id);

        /**
         * @brief Function that should be called periodically, measures distances and generates events
         * 
         * @param diff Time difference since last run
         */
        void tick(TickType_t diff);

        /**
         * @brief Create thread that periodically calls tick()
         */
        void startTask();

        /**
         * @brief Delete thread
         */
        void stopTask();
        
        /**
         * @brief Nearest point in last x seconds (updated in tick() function)
         * 
         * @return std::shared_ptr<DistancePoint> 
         */
        std::shared_ptr<DistancePoint> nearestPoint();

        /**
         * @brief Helper function to register event handler for events by this object
         * 
         * @param event_handler event handler function
         * @param handler_args args that will be passed to every event handler function
         * @return esp_err_t returns ESP_OK if succeeds
         */
        esp_err_t registerEventHandle(esp_event_handler_t event_handler, void *handler_args);
    private:

        /**
         * @brief Create point (called by addPoint())
         * 
         * @param mac WiFi MAC address in binary form
         * @param macstr WiFi MAC address in string form ("xx:xx:xx:xx:xx:xx")
         * @param channel WiFi channel
         * @param id requested id of the created point (UINT32_MAX=no preference -> `largest_id`+1 will be assigned)
         * @return uint32_t resulting id of created point or id of already created point with same mac, 
         *                  UINT32_MAX if id is used by other point or an error occurred
         */
        uint32_t _addPoint(const uint8_t mac[6], std::string macstr, uint8_t channel, uint32_t id);

        /**
         * @brief Helper function that measures distance to given @p point , produces events
         * 
         * @param point point to which the distance will be measured
         * @return esp_err_t returns ESP_OK if the measurement is valid and event was posted
         */
        esp_err_t measureDistance(std::shared_ptr<DistancePoint> point);

        /**
         * @brief Discover reachable points by performing WiFi Scan
         * 
         * @return std::vector<std::shared_ptr<DistancePoint>> reachable points that were found
         */
        std::vector<std::shared_ptr<DistancePoint>> reachablePoints();

        /**
         * @brief Necessary wrapper for creating thread that performs object's method
         * @param param pointer to DistanceMeter (usually @p this )
         */
        static void taskWrapper(void* param){
            static_cast<DistanceMeter *>(param)->task();
        }
        /**
         * @brief Task that is performed by created thread in startTask()
         */
        void task();

        bool _only_reachable;
        /**
         * @brief container for storing managed points
         */
        std::unordered_map<uint32_t, std::shared_ptr<DistancePoint>> _points;
        /**
         * @brief map for searching point id by WiFi MAC string
         */
        std::unordered_map<std::string, uint32_t> _points_mac_id;
        esp_event_loop_handle_t _event_loop_hdl;
        /**
         * @brief Time threshold for determining closest point
         */
        const uint32_t time_threshold = 10000 * (1000 / configTICK_RATE_HZ);
        /**
         * @brief Distance threshold for determining closest point
         */
        const uint32_t _distance_threshold_cm = 10 * 100;
        TaskHandle_t _xHandle = NULL;
        uint32_t _next_id = 0;
};


#endif