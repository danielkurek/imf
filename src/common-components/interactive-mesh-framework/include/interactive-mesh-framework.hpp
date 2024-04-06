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
#include "esp_event.h"

#include <vector>
#include <map>
#include <unordered_map>
#include <memory>
#include <functional>

#include "imf-device.hpp"
#include "localization.hpp"

namespace imf{
    using tick_function_t = std::function<void(TickType_t)>;
    typedef void (*state_change_t)(uint32_t old_state, uint32_t new_state);

    typedef struct{
        tick_function_t tick_f;
        rgb_t color;
    } state_data_t;

    template <typename A>
    tick_function_t safeSharedTickCall(std::shared_ptr<A> obj){
        return [obj](TickType_t diff) {
            if(obj) obj->tick(diff);
        };
    }
    class IMF{
        public:
            IMF(const std::vector<button_gpio_config_t> &buttons, bool default_states = true);
            esp_err_t start();
            esp_err_t registerCallbacks(board_button_callback_t btn_cb, esp_event_handler_t event_handler, void *handler_args, tick_function_t update_func, state_change_t state_change_cb);
            uint32_t addDevice(DeviceType type, std::string wifi_mac_str, uint8_t wifi_channel, uint16_t ble_mesh_addr);
            std::shared_ptr<Device> getDevice(uint32_t id);
            esp_err_t createAP(const std::string& ssid, const std::string& password, uint8_t channel);
            esp_err_t connectToAP(const std::string& ssid, const std::string& password);
            esp_err_t addOption(const config_option_t& option);
            void setStateData(int16_t state_num, tick_function_t tick_fun, rgb_t color);
            void addDefaultStates();
            nvs_handle_t getOptionsHandle() { return _options_handle; }
            void startWebConfig();
            void stopWebConfig();
            esp_err_t startLocalization();
            void stopLocalization();
            auto devices_cbegin() { return _devices.cbegin(); }
            auto devices_cend() { return _devices.cend(); }
        private:
            std::map<uint32_t, std::shared_ptr<Device>> _devices;
            std::unordered_map<int16_t, state_data_t> _states;
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
            void _init_localization();
            std::shared_ptr<DistanceMeter> _dm;
            std::vector<config_option_t> _options;
            esp_event_loop_handle_t _event_loop_hdl;
            tick_function_t _update_cb = nullptr;
            state_change_t _state_change_cb = nullptr;
            uint32_t _next_id = 1; // 0 is reserved for local device
            nvs_handle_t _options_handle;
            std::shared_ptr<Localization> _localization;
            int16_t current_state = 0;
            SemaphoreHandle_t xSemaphoreUpdate = NULL;
            TaskHandle_t _xUpdateHandle;
    };
}


#endif