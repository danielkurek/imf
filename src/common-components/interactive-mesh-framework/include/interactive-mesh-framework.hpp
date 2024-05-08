#ifndef INTERACTIVE_MESH_FRAMEWORK_H_
#define INTERACTIVE_MESH_FRAMEWORK_H_

#include <inttypes.h>
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
    /**
     * @brief state tick function that is called periodically
     * Can also be a lambda function with same parameters.
     */
    using tick_function_t = std::function<void(TickType_t)>;
    /**
     * @brief callback function when state of the application changes
     */
    typedef void (*state_change_t)(uint32_t old_state, uint32_t new_state);

    typedef struct{
        tick_function_t tick_f; /**< state tick function that will be called periodically*/
        rgb_t color; /**< state color to display device status */
    } state_data_t;

    /**
     * @brief Helper function to create lambda tick function on pointer to object with tick function
     * 
     * @tparam A Object with tick_function_t with name tick(TickType_t)
     * @param obj pointer to object instance
     * @return tick_function_t returns lambda function that calls tick function on @p obj only if it is valid pointer
     */
    template <typename A>
    tick_function_t safeSharedTickCall(std::shared_ptr<A> obj){
        return [obj](TickType_t diff) {
            if(obj) obj->tick(diff);
        };
    }
    class IMF{
        public:
            /**
             * @brief Construct a new IMF object with buttons initialization
             * 
             * @param buttons buttons configurations to initialize
             * @param default_states create default states?
             */
            IMF(const std::vector<button_gpio_config_t> &buttons, bool default_states = true);

            /**
             * @brief Construct a new IMF object without any buttons
             * 
             * @param default_states create default states?
             */
            IMF(bool default_states = true) : IMF({}, default_states) {}

            /**
             * @brief Start main function of IMF
             * 
             * @return esp_err_t ESP_OK if start was successful
             */
            esp_err_t start();

            /**
             * @brief Register callbacks from IMF
             * 
             * @param btn_cb button callback function or @p nullptr
             * @param event_handler event handler function or @p nullptr
             * @param handler_args args that will be passed to event handle or @p nullptr
             * @param update_func application tick function or @p nullptr
             * @param state_change_cb callback when state changes or @p nullptr
             * @return esp_err_t ESP_OK if everything registered successfully
             */
            esp_err_t registerCallbacks(board_button_callback_t btn_cb, esp_event_handler_t event_handler, void *handler_args, tick_function_t update_func, state_change_t state_change_cb);

            /**
             * @brief Add device to IMF
             * 
             * @param type device type
             * @param wifi_mac_str WiFi MAC for distance measurement
             * @param wifi_channel WiFi channel for distance measurement
             * @param ble_mesh_addr Bluetooth mesh address
             * @return uint32_t resulting device id or UINT32_MAX on failure
             */
            uint32_t addDevice(DeviceType type, std::string wifi_mac_str, uint8_t wifi_channel, uint16_t ble_mesh_addr);

            /**
             * @brief Get pointer to Device with specified id
             * 
             * @param id device's id
             * @return std::shared_ptr<Device> pointer to device or @p nullptr
             */
            std::shared_ptr<Device> getDevice(uint32_t id);

            /**
             * @brief Create WiFi AP with given credentials
             * 
             * @param ssid WiFi SSID
             * @param password WPA2-PSK password
             * @param channel WiFi channel
             * @return esp_err_t ESP_OK if AP is created
             */
            esp_err_t createAP(const std::string& ssid, const std::string& password, uint8_t channel);

            /**
             * @brief Connect to WiFi AP
             * 
             * @param ssid SSID of the WiFi
             * @param password password of the WiFi, leave empty if WiFi has no password
             * @return esp_err_t ESP_OK if successfully connected
             */
            esp_err_t connectToAP(const std::string& ssid, const std::string& password);

            /**
             * @brief Add option to Web configuration
             * 
             * @param option configuration option
             */
            void addOption(const config_option_t& option);

            /**
             * @brief Add or change state
             * 
             * @param state_num state number (used for communication)
             * @param tick_fun  function that will be periodically be called when application is in this state
             * @param color state color which will be displayed on device status LED
             */
            void setStateData(int16_t state_num, tick_function_t tick_fun, rgb_t color);
            /**
             * @brief Populate states with default states
             * Station device: 
             * -> 0 = startup - do nothing
             * -> 1 = Distance measurement
             * -> 2 = Localization
             * -> 3 = app tick function
             * Mobile device: 
             * -> 0 = startup - do nothing
             * -> 1 = nothing
             * -> 2 = nothing
             * -> 3 = Distance measurement + localization + app tick function
             */
            void addDefaultStates();

            /**
             * @brief Get NVS handle for web configuration options
             * 
             * @return nvs_handle_t opened NVS partition with saved web configuration options
             */
            nvs_handle_t getOptionsHandle() { return _options_handle; }

            /**
             * @brief Start web configuration
             * Usually called at startup when certain button is held
             */
            void startWebConfig();

            /**
             * @brief Stop web configuration server
             * It is better to reboot device to return to normal app
             */
            void stopWebConfig();

            /**
             * @brief Start continuous localization (states with tick function are preferred)
             * 
             * @return esp_err_t ESP_OK if localization is started without error
             */
            esp_err_t startLocalization();

            /**
             * @brief Stop continuous localization 
             */
            void stopLocalization();
            
            /**
             * @brief Get constant iterator over added devices
             * @return auto std::map<uint32_t, std::shared_ptr<Device>> const iterator begin
             */
            auto devices_cbegin() { return _devices.cbegin(); }

            /**
             * @brief Get constant iterator over added devices
             * @return auto std::map<uint32_t, std::shared_ptr<Device>> end
             */
            auto devices_cend() { return _devices.cend(); }
        private:
            /** storage for added devices */
            std::map<uint32_t, std::shared_ptr<Device>> _devices;
            /** storage for application states */
            std::unordered_map<int16_t, state_data_t> _states;

            /**
             * @brief Start update task that performs state tick functions
             * 
             * @return esp_err_t ESP_OK if start is successful
             */
            esp_err_t startUpdateTask();

            /**
             * @brief Stop update task
             */
            void stopUpdateTask();

            /**
             * @brief Update task
             */
            void _update_task();
            
            /**
             * @brief Necessary wrapper for creating thread that performs object's method
             * 
             * @param param pointer to IMF instance (usually @p this )
             */
            static void _update_task_wrapper(void *param){
                static_cast<IMF *>(param)->_update_task();
            }
            
            /**
             * @brief Wait for Bluetooth mesh module to start responding to commands
             * 
             * @param max_tries Number of retries
             * @return esp_err_t ESP_OK if module is communicating
             */
            esp_err_t _wait_for_ble_mesh(uint32_t max_tries);

            /**
             * @brief Initialize localization (needs to called after all devices are added)
             */
            void _init_localization();
            std::shared_ptr<DistanceMeter> _dm; /**< DistanceMeter for measuring distances to devices */
            std::vector<config_option_t> _options; /**< added options to @ref web_config.h*/
            esp_event_loop_handle_t _event_loop_hdl; /**< separate event loop for DistanceMeter */
            tick_function_t _update_cb = nullptr; /**< update callback function */
            state_change_t _state_change_cb = nullptr; /**< state change callback function */
            /**
             * @brief id of the next added device (0 is reserved for local device)
             */
            uint32_t _next_id = 1; 
            nvs_handle_t _options_handle; /** NVS handle for web_config settings */
            std::shared_ptr<Localization> _localization; /** Localization module */
            int16_t current_state = 0; /** Current application state*/
            TaskHandle_t _xUpdateHandle; /** handle for UpdateTask thread*/
    };
}


#endif