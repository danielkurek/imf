/**
 * @file imf-device.hpp
 * @author Daniel Kurek (daniel.kurek.dev@gmail.com)
 * @brief Definition of imf::Device class
 * @version 0.1
 * @date 2024-03-20
 * 
 * @copyright Copyright (c) 2024
 * 
 */
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
            /**
             * @brief Construct a new Device object
             * 
             * @param _id device id
             * @param _type device type
             * @param _wifi_mac_str WiFi MAC for distance measurement
             * @param _wifi_channel WiFi channel for distance measurement
             * @param _ble_mesh_addr Bluetooth mesh address
             */
            Device(uint32_t _id, DeviceType _type, std::string _wifi_mac_str, uint8_t _wifi_channel, uint16_t _ble_mesh_addr);
            #if CONFIG_IMF_DEBUG_STATIC_DEVICES
            Device(uint32_t _id, DeviceType _type, std::string _wifi_mac_str, uint8_t _wifi_channel, uint16_t _ble_mesh_addr, 
                rgb_t rgb, location_local_t location, int16_t level, uint32_t distance_cm, int8_t rssi);
            #endif

            /**
             * @brief Set LED Rgb
             * 
             * @param rgb 
             * @return esp_err_t ESP_OK if command is sent out successfully
             */
            esp_err_t setRgb(rgb_t rgb);

            /**
             * @brief Get LED Rgb
             * 
             * @param[out] rgb_out rgb value of the device, only valid if ESP_OK is returned
             * @return esp_err_t ESP_OK if rgb value is known and is valid
             */
            esp_err_t getRgb(rgb_t &rgb_out);

            /**
             * @brief Set device's Location
             * 
             * @param location new location
             * @return esp_err_t ESP_OK if command is sent out successfully
             */
            esp_err_t setLocation(const location_local_t &location);

            /**
             * @brief Get device's Location
             * 
             * @param[out] location_out location of the device, only valid if ESP_OK is returned
             * @return esp_err_t ESP_OK if location value is known and is valid
             */
            esp_err_t getLocation(location_local_t &location_out);

            /**
             * @brief Set device's Level
             * 
             * @param level new level
             * @return esp_err_t ESP_OK if command is sent out successfully
             */
            esp_err_t setLevel(int16_t level);

            /**
             * @brief Get device's Level
             * 
             * @param[out] level_out level of the device, only valid if ESP_OK is returned
             * @return esp_err_t ESP_OK if level value is known and is valid
             */
            esp_err_t getLevel(int16_t &level_out);

            /**
             * @brief Measure distance to this device
             * 
             * @param[out] measurement resulting measurement, only valid if ESP_OK is returned
             * @return esp_err_t ESP_OK if distance was measured successfully
             */
            esp_err_t measureDistance(distance_measurement_t &measurement);

            /**
             * @brief Get latest measured distance
             * 
             * @param distance_log latest distance measurement with timestamp
             * @return esp_err_t ESP_OK if distance is retrieved successfully
             */
            esp_err_t lastDistance(distance_log_t &distance_log);

            const uint32_t id; /**< device id */
            const DeviceType type; /**< device type */
            const uint16_t ble_mesh_addr; /**< Bluetooth mesh address */
            bool fixed_location; /**< perform localization on this device (used only for local device) */
            #if CONFIG_IMF_DEBUG_STATIC_DEVICES
            uint32_t debug_distance_cm = 0; /**< distance in debug mode */
            int8_t debug_rssi = INT8_MIN; /**< rssi in debug mode*/
            rgb_t debug_rgb = {0,0,0}; /**< default rgb in debug mode*/
            location_local_t debug_location = {0,0,0,0,0}; /**< default location in debug mode*/
            int16_t debug_level = 0; /**< default level in debug mode */
            #endif

            /**
             * @brief Set Rgb to all devices (only sends command)
             * 
             * @param rgb new rgb value
             * @return esp_err_t ESP_OK if command is sent out successfully
             */
            static esp_err_t setRgbAll(rgb_t rgb);

            /**
             * @brief Set Level to all devices (only sends command)
             * 
             * @param level new level value
             * @return esp_err_t ESP_OK if command is sent out successfully
             */
            static esp_err_t setLevelAll(int16_t level);

            /**
             * @brief Set DistanceMeter that will be used for distance measurement
             * **should be called before creating devices**
             * 
             * @param dm pointer to @ref DistanceMeter
             */
            static void setDM(std::shared_ptr<DistanceMeter> dm) { _dm = dm; }

            /**
             * @brief Get com::SerialCommCli used for communication with other devices
             * 
             * @return std::shared_ptr<com::SerialCommCli> 
             */
            static std::shared_ptr<com::SerialCommCli> getSerialCli() { return _serial; }

            /**
             * @brief Initialize local device (the device the this code runs on)
             * 
             * Can be blocking function. Address of local Bluetooth mesh device is retrieved by @ref com::SerialComm
             * Several retries are attempted before reverting to last known address of local device
             * 
             * @param options_handle opened NVS partition that stores settings from @ref web_config.h
             * @return esp_err_t ESP_OK if device is initialized
             */
            static esp_err_t initLocalDevice(nvs_handle_t options_handle);

            /**
             * @brief Pointer to instance of Local device (should be set by calling initLocalDevice())
             */
            static std::shared_ptr<Device> this_device;
        private:
            /**
             * @brief Special constructor for local device
             * 
             * @param _id device id
             * @param _type device type
             * @param _wifi_mac_str WiFi MAC for distance measurement
             * @param _wifi_channel WiFi channel for distance measurement
             * @param _ble_mesh_addr Bluetooth mesh address
             * @param _local_commands use commands without specifying address (if we do not know address of local Bluetooth mesh device)
             */
            Device(uint32_t _id, DeviceType _type, std::string _wifi_mac_str, uint8_t _wifi_channel, uint16_t _ble_mesh_addr, bool _local_commands);
            /**
             * @brief Convert byte format of MAC to string format ("xx:xx:xx:xx:xx:xx")
             * 
             * @return std::string MAC address in string format ("xx:xx:xx:xx:xx:xx") or empty string
             */
            static std::string _getMAC();
            bool _local_commands; /**< omit address when sending serial commands */
            std::shared_ptr<DistancePoint> _point; /**< DistancePoint instance of this device*/
            static std::shared_ptr<com::SerialCommCli> _serial; /**< @ref SerialComm used to communicate with Bluetooth mesh module*/
            static std::shared_ptr<DistanceMeter> _dm; /**< @ref DistanceMeter used for managing _point */
            static constexpr int _maxRetries = 15; /**< number of retries for fetching Bluetooth mesh address of local device */
    };
}

#endif // IMF_DEVICE_HPP_