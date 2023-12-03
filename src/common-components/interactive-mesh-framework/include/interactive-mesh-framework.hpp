#ifndef INTERACTIVE_MESH_FRAMEWORK_H_
#define INTERACTIVE_MESH_FRAMEWORK_H_

#include "distance_meter.hpp"
#include "web_config.h"
#include "serial_comm_client.hpp"
#include "serial_comm_server.hpp"
#include "color.h"
#include "board.h"
#include "logger.h"

#include <vector>
#include <map>
#include <memory>

namespace imf{
    typedef void (*update_function_t)(TickType_t diff_ms);

    enum class DeviceType {
        Mobile,
        Station
    };

    class Device{
        public:
            Device(uint32_t _id, DeviceType _type, std::string _wifi_mac_str, uint8_t _wifi_channel, uint16_t _ble_mesh_addr);
            esp_err_t setRgb(rgb_t rgb);
            rgb_t getRgb();
            esp_err_t measureDistance(uint32_t *distance_cm);
            const uint32_t id;
            const DeviceType type;
            const uint16_t ble_mesh_addr;
            static esp_err_t setRgbAll(rgb_t rgb);
            static void setDM(std::shared_ptr<DistanceMeter> dm) { _dm = dm; }
        private:
            std::shared_ptr<DistancePoint> _point;
            static std::shared_ptr<SerialCommCli> _serial;
            static std::shared_ptr<DistanceMeter> _dm;
    };

    class IMF{
        public:
            IMF();
            esp_err_t start();
            esp_err_t registerCallbacks(board_button_callback_t btn_cb, esp_event_handler_t event_handler, void *handler_args, update_function_t update_func);
            uint32_t addDevice(DeviceType type, std::string wifi_mac_str, uint8_t wifi_channel, uint16_t ble_mesh_addr);
            std::shared_ptr<Device> getDevice(uint32_t id);
            esp_err_t createAP(const std::string& ssid, const std::string& password, uint8_t channel);
            esp_err_t connectToAP(const std::string& ssid, const std::string& password);
        private:
            std::map<uint32_t, std::shared_ptr<Device>> _devices;
            static void _update_timer_cb_wrapper(void *param){
                static_cast<IMF *>(param)->_update_timer_cb();
            }
            void _update_timer_cb();
            std::shared_ptr<DistanceMeter> _dm;
            esp_event_loop_handle_t _event_loop_hdl;
            update_function_t _update_cb;
            uint32_t _next_id = 0;

    };
}


#endif