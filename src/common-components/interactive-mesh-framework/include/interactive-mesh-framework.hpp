#ifndef INTERACTIVE_MESH_FRAMEWORK_H_
#define INTERACTIVE_MESH_FRAMEWORK_H_

#include "distance_meter.hpp"
#include "web_config.h"
#include "serial_comm_client.hpp"
#include "serial_comm_server.hpp"
#include "color/color.h"
#include "board.h"
#include "logger.h"
#include "location_defs.h"
#include "nvs_flash.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include <vector>
#include <map>
#include <memory>

// need forward declaration because of circular dependency
namespace imf{
    class IMF;
    class Device;
}

#include "location_topology.hpp"

namespace imf{
    typedef void (*update_function_t)(TickType_t diff_ms);

    enum class DeviceType {
        Mobile,
        Station
    };
    class Device{
        public:
            Device(uint32_t _id, DeviceType _type, std::string _wifi_mac_str, uint8_t _wifi_channel, uint16_t _ble_mesh_addr);
            #if CONFIG_IMF_DEBUG_STATIC_DEVICES
            Device(uint32_t _id, DeviceType _type, std::string _wifi_mac_str, uint8_t _wifi_channel, uint16_t _ble_mesh_addr, 
                rgb_t rgb, location_local_t location, int16_t level, uint32_t distance_cm);
            #endif
            esp_err_t setRgb(rgb_t rgb);
            esp_err_t getRgb(rgb_t &rgb_out);
            esp_err_t setLocation(const location_local_t &location);
            esp_err_t getLocation(location_local_t &location_out);
            esp_err_t setLevel(int16_t level);
            esp_err_t getLevel(int16_t &level_out);
            esp_err_t measureDistance(uint32_t &distance_cm);
            esp_err_t lastDistance(uint32_t &distance_cm);
            const uint32_t id;
            const DeviceType type;
            const uint16_t ble_mesh_addr;
            #if CONFIG_IMF_DEBUG_STATIC_DEVICES
            uint32_t debug_distance_cm = 0;
            rgb_t debug_rgb = {0,0,0};
            location_local_t debug_location = {0,0,0,0,0};
            int16_t debug_level = 0;
            #endif
            static esp_err_t setRgbAll(rgb_t rgb);
            static esp_err_t setLevelAll(int16_t level);
            static void setDM(std::shared_ptr<DistanceMeter> dm) { _dm = dm; }
            static std::shared_ptr<SerialCommCli> getSerialCli() { return _serial; }
            // if the ble-mesh address is not saved. this will wait for initialization of ble-mesh device and fetch the address
            // this should happen only on first boot
            static esp_err_t initLocalDevice(IMF *imf);
            static std::shared_ptr<Device> this_device;
        private:
            Device(uint32_t _id, DeviceType _type, std::string _wifi_mac_str, uint8_t _wifi_channel, uint16_t _ble_mesh_addr, bool _local_commands);
            static std::string _getMAC();
            bool _local_commands;
            std::shared_ptr<DistancePoint> _point;
            static std::shared_ptr<SerialCommCli> _serial;
            static std::shared_ptr<DistanceMeter> _dm;
            static constexpr int _maxRetries = 15;
    };

    class IMF{
        public:
            IMF(const std::vector<button_gpio_config_t> &buttons);
            esp_err_t start();
            esp_err_t registerCallbacks(board_button_callback_t btn_cb, esp_event_handler_t event_handler, void *handler_args, update_function_t update_func);
            uint32_t addDevice(DeviceType type, std::string wifi_mac_str, uint8_t wifi_channel, uint16_t ble_mesh_addr);
            std::shared_ptr<Device> getDevice(uint32_t id);
            esp_err_t createAP(const std::string& ssid, const std::string& password, uint8_t channel);
            esp_err_t connectToAP(const std::string& ssid, const std::string& password);
            esp_err_t addOption(const config_option_t& option);
            nvs_handle_t getOptionsHandle() { return _options_handle; }
            void startWebConfig();
            void stopWebConfig();
            esp_err_t startLocationTopology();
            void stopLocationTopology();
        private:
            std::map<uint32_t, std::shared_ptr<Device>> _devices;
            esp_err_t startUpdateTask();
            void stopUpdateTask();
            void _update_task();
            void _update_timer_cb();
            static void _update_task_wrapper(void *param){
                static_cast<IMF *>(param)->_update_task();
            }
            static void _update_timer_cb_wrapper(void *param){
                static_cast<IMF *>(param)->_update_timer_cb();
            }
            esp_err_t _wait_for_ble_mesh(uint32_t max_tries);
            void _init_topology();
            std::shared_ptr<DistanceMeter> _dm;
            std::vector<config_option_t> _options;
            esp_event_loop_handle_t _event_loop_hdl;
            update_function_t _update_cb = nullptr;
            uint32_t _next_id = 1; // 0 is reserved for local device
            nvs_handle_t _options_handle;
            std::shared_ptr<LocationTopology> _topology;
            int16_t current_state = 0;
            SemaphoreHandle_t xSemaphoreUpdate = NULL;
            TaskHandle_t _xUpdateHandle;
    };
}


#endif