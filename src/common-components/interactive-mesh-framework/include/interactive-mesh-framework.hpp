#ifndef INTERACTIVE_MESH_FRAMEWORK_H_
#define INTERACTIVE_MESH_FRAMEWORK_H_

#include "distance_meter.hpp"
#include "web_config.h"
#include "serial_comm_client.hpp"
#include "serial_comm_server.hpp"
#include "color.h"
#include "board.h"

#include <vector>

namespace imf{
    enum class DeviceType {
        Mobile,
        Station
    };

    class Device{
        public:
            Device(DeviceType _type, uint8_t _wifi_mac[6], uint8_t _wifi_channel, uint16_t _ble_mesh_addr);
            esp_err_t setRgb(rgb_t rgb);
            rgb_t getRgb();
            uint32_t measureDistance();
            const DeviceType type;
            const uint16_t ble_mesh_addr;
        private:
            DistancePoint _point;
            std::shared_ptr<SerialCommCli> _serial;
    };

    class IMF{
        public:
            IMF();
            esp_err_t start();
            esp_err_t registerCallbacks(board_button_callback_t btn_cb, esp_event_handler_t event_handler, void *handler_args);
            esp_err_t addDevice();
        private:
            std::vector<std::shared_ptr<Device>> _devices;
            DistanceMeter _dm;
            esp_event_loop_handle_t _event_loop_hdl;
    };
}


#endif