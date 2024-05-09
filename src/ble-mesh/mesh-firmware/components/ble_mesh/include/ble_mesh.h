/**
 * @file ble_mesh.h
 * @author Daniel Kurek (daniel.kurek.dev@gmail.com)
 * @brief Definition of interface with Bluetooth mesh
 * @version 0.1
 * @date 2023-06-26
 * 
 * @copyright Copyright (c) 2023
 * 
 */
#ifndef BLE_MESH_H_
#define BLE_MESH_H_

#include "color/color.h"
#include <inttypes.h>
#include "board.h"
#include "rgb_control_client.h"
#include "location_defs.h"

#ifdef __cplusplus
extern "C"{
#endif

/**
 * @brief Initialize Bluetooth mesh
 * 
 * @return esp_err_t ESP_OK if succeeds
 */
esp_err_t ble_mesh_init();

/**
 * @brief Pass configuration of RGB LED which should be controlled by RGB model
 * 
 * @param conf RGB LED configuration
 */
void ble_mesh_set_rgb_conf(rgb_conf_t *conf);

/**
 * @brief Set RGB state of a device
 * 
 * @param addr device address
 * @param color new color
 * @param ack should be sent reliably?
 * @return esp_err_t ESP_OK if message is sent
 */
esp_err_t ble_mesh_set_rgb(uint16_t addr, rgb_t color, bool ack);
/**
 * @brief send GET command for RGB state of a device
 * 
 * @param addr device address
 * @return esp_err_t ESP_OK if message is sent
 */
esp_err_t ble_mesh_get_rgb(uint16_t addr);

/**
 * @brief SET location station of a device
 * 
 * @param addr device address
 * @param loc_local new location
 * @return esp_err_t ESP_OK if message is sent
 */
esp_err_t ble_mesh_set_loc_local(uint16_t addr, const location_local_t *loc_local);
/**
 * @brief send GET command for location state of a device
 * 
 * @param addr device address
 * @return esp_err_t ESP_OK if message is sent
 */
esp_err_t ble_mesh_get_loc_local(uint16_t addr);

/**
 * @brief SET OnOff state of a device
 * 
 * @param addr device address
 * @param onoff onoff state
 * @return esp_err_t ESP_OK if message is sent
 */
esp_err_t ble_mesh_set_onoff(uint16_t addr, bool onoff);
/**
 * @brief send GET cmd for OnOff state of a device
 * 
 * @param addr device address
 * @return esp_err_t ESP_OK if message is sent
 */
esp_err_t ble_mesh_get_onoff(uint16_t addr);

/**
 * @brief SET level state of a device
 * 
 * @param addr device address
 * @param level new level
 * @return esp_err_t 
 */
esp_err_t ble_mesh_set_level(uint16_t addr, int16_t level);
/**
 * @brief send GET cmd for Level state of a device
 * 
 * @param addr device address
 * @return esp_err_t ESP_OK if message is sent
 */
esp_err_t ble_mesh_get_level(uint16_t addr);

/**
 * @brief Get addresses of this node
 * 
 * @param[out] primary_addr primary adress
 * @param[out] addresses number of addresses
 * @return esp_err_t ESP_OK if address is assigned
 */
esp_err_t ble_mesh_get_addresses(uint16_t *primary_addr, uint8_t *addresses);

typedef enum{
    LOC_LOCAL_CHANGE = 0,
    RGB_CHANGE,
    ONOFF_CHANGE,
    LEVEL_CHANGE
} ble_mesh_value_change_type_t;

typedef struct{
    ble_mesh_value_change_type_t type;
    uint16_t addr;
    union{
        location_local_t loc_local;
        rgb_t rgb;
        bool onoff;
        int16_t level;
    };
} ble_mesh_value_change_data_t;

typedef void (* ble_mesh_value_change_cb)(ble_mesh_value_change_data_t event_data);

/**
 * @brief Register callback for change of a value
 * 
 * @param value_change_cb callback function
 */
void ble_mesh_register_cb(ble_mesh_value_change_cb value_change_cb);

#ifdef __cplusplus
}
#endif

#endif