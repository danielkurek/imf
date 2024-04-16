#ifndef IMF_DEVICE_HPP_
#define IMF_DEVICE_HPP_

#include "esp_err.h"
#include <string>
#include "location_defs.h"
#include <cstdint>
#include "nvs_flash.h"

#include "color/color.h"
#include "distance_meter.hpp"
#include "serial_comm_client.hpp"

namespace imf{
    enum class DeviceType {
        Mobile,
        Station
    };

    class Device{
        public:
            Device(uint32_t _id, DeviceType _type, std::string _wifi_mac_str, uint8_t _wifi_channel, uint16_t _ble_mesh_addr);
            #if CONFIG_IMF_DEBUG_STATIC_DEVICES
            Device(uint32_t _id, DeviceType _type, std::string _wifi_mac_str, uint8_t _wifi_channel, uint16_t _ble_mesh_addr, 
                rgb_t rgb, location_local_t location, int16_t level, uint32_t distance_cm, int8_t rssi);
            #endif
            esp_err_t setRgb(rgb_t rgb);
            esp_err_t getRgb(rgb_t &rgb_out);
            esp_err_t setLocation(const location_local_t &location);
            esp_err_t getLocation(location_local_t &location_out);
            esp_err_t setLevel(int16_t level);
            esp_err_t getLevel(int16_t &level_out);
            esp_err_t measureDistance(distance_measurement_t &measurement);
            esp_err_t lastDistance(distance_measurement_t &measurement);
            const uint32_t id;
            const DeviceType type;
            const uint16_t ble_mesh_addr;
            bool fixed_location;
            #if CONFIG_IMF_DEBUG_STATIC_DEVICES
            uint32_t debug_distance_cm = 0;
            int8_t debug_rssi = INT8_MIN;
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
            static esp_err_t initLocalDevice(nvs_handle_t options_handle);
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
}

#endif // IMF_DEVICE_HPP_