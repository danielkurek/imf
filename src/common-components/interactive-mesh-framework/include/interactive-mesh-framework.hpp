#ifndef INTERACTIVE_MESH_FRAMEWORK_H_
#define INTERACTIVE_MESH_FRAMEWORK_H_

#include "web_config.h"
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

#include "graph_localization.hpp"

namespace imf{
    typedef void (*update_function_t)(TickType_t diff_ms);

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
            auto devices_cbegin() { return _devices.cbegin(); }
            auto devices_cend() { return _devices.cend(); }
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
            std::shared_ptr<GraphLocalization> _topology;
            int16_t current_state = 0;
            SemaphoreHandle_t xSemaphoreUpdate = NULL;
            TaskHandle_t _xUpdateHandle;
    };
}


#endif