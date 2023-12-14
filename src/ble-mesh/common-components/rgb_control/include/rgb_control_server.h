#ifndef BLE_MESH_RGB_CONTROL_SERVER_H_
#define BLE_MESH_RGB_CONTROL_SERVER_H_

#include "esp_ble_mesh_defs.h"
#include "esp_ble_mesh_common_api.h"
#include "esp_ble_mesh_lighting_model_api.h"
#include "color/color.h"

#define BLE_MESH_MODEL_RGB_STATE_DEFINE(_name) \
    static esp_ble_mesh_light_hsl_state_t _name = { \
        .lightness_default = UINT16_MAX/2, \
        .hue_default = UINT16_MAX/2, \
        .saturation_default = UINT16_MAX/2, \
        .hue_range_min = 0, \
        .hue_range_max = UINT16_MAX, \
        .saturation_range_min = 0, \
        .saturation_range_max = UINT16_MAX, \
    }

#define BLE_MESH_MODEL_RGB_SRV_PUB_DEFINE(_name, _role) \
    ESP_BLE_MESH_MODEL_PUB_DEFINE(_name, 2 + 30, _role);

#define BLE_MESH_MODEL_RGB_SRV_DEFINE(_name, _state) \
    static esp_ble_mesh_light_hsl_srv_t _name = { \
        .rsp_ctrl.get_auto_rsp = ESP_BLE_MESH_SERVER_AUTO_RSP, \
        .rsp_ctrl.set_auto_rsp = ESP_BLE_MESH_SERVER_AUTO_RSP, \
        .rsp_ctrl.status_auto_rsp = ESP_BLE_MESH_SERVER_AUTO_RSP, \
        .state = _state, \
    }


// ESP_BLE_MESH_MODEL_PUB_DEFINE(light_hsl_setup_pub, 2 + 30, ROLE_NODE);
#define BLE_MESH_MODEL_RGB_SETUP_SRV_DEFINE(_name, _state) \
    static esp_ble_mesh_light_hsl_setup_srv_t _name = { \
        .rsp_ctrl.get_auto_rsp = ESP_BLE_MESH_SERVER_AUTO_RSP, \
        .rsp_ctrl.set_auto_rsp = ESP_BLE_MESH_SERVER_AUTO_RSP, \
        .rsp_ctrl.status_auto_rsp = ESP_BLE_MESH_SERVER_AUTO_RSP, \
        .state = _state, \
    }

// ESP_BLE_MESH_MODEL_PUB_DEFINE(light_hsl_hue_pub, 2 + 30, ROLE_NODE);
#define BLE_MESH_MODEL_RGB_SRV_ELM1_DEFINE(_name, _state) \
    static esp_ble_mesh_light_hsl_hue_srv_t _name = { \
        .rsp_ctrl.get_auto_rsp = ESP_BLE_MESH_SERVER_AUTO_RSP, \
        .rsp_ctrl.set_auto_rsp = ESP_BLE_MESH_SERVER_AUTO_RSP, \
        .rsp_ctrl.status_auto_rsp = ESP_BLE_MESH_SERVER_AUTO_RSP, \
        .state = _state, \
    }

// ESP_BLE_MESH_MODEL_PUB_DEFINE(light_hsl_sat_pub, 2 + 30, ROLE_NODE);
#define BLE_MESH_MODEL_RGB_SRV_ELM2_DEFINE(_name, _state) \
    static esp_ble_mesh_light_hsl_sat_srv_t _name = { \
        .rsp_ctrl.get_auto_rsp = ESP_BLE_MESH_SERVER_AUTO_RSP, \
        .rsp_ctrl.set_auto_rsp = ESP_BLE_MESH_SERVER_AUTO_RSP, \
        .rsp_ctrl.status_auto_rsp = ESP_BLE_MESH_SERVER_AUTO_RSP, \
        .state = _state, \
    }

/**
 *  srv_pub  Pointer to the unique struct esp_ble_mesh_model_pub_t.
 *  srv_data Pointer to the unique struct esp_ble_mesh_light_hsl_srv_t defined by BLE_MESH_MODEL_RGB_SRV_DEFINE.
 */
#define BLE_MESH_MODEL_RGB_SRV(srv_pub, srv_data) \
    ESP_BLE_MESH_MODEL_LIGHT_HSL_SRV(srv_pub, srv_data)
/**
 *  srv_pub  Pointer to the unique struct esp_ble_mesh_model_pub_t.
 *  srv_data Pointer to the unique struct esp_ble_mesh_light_hsl_setup_srv_t defined by BLE_MESH_MODEL_RGB_SETUP_SRV_DEFINE.
 */
#define BLE_MESH_MODEL_RGB_SETUP_SRV(srv_pub, srv_data) \
    ESP_BLE_MESH_MODEL_LIGHT_HSL_SETUP_SRV(srv_pub, srv_data)
/**
 *  srv_pub  Pointer to the unique struct esp_ble_mesh_model_pub_t.
 *  srv_data Pointer to the unique struct esp_ble_mesh_light_hsl_hue_srv_t defined by BLE_MESH_MODEL_RGB_SRV_ELM1_DEFINE.
 */
#define BLE_MESH_MODEL_RGB_ELM1_SRV(srv_pub, srv_data) \
    ESP_BLE_MESH_MODEL_LIGHT_HSL_HUE_SRV(srv_pub, srv_data)
/**
 *  srv_pub  Pointer to the unique struct esp_ble_mesh_model_pub_t.
 *  srv_data Pointer to the unique struct esp_ble_mesh_light_hsl_sat_srv_t defined by BLE_MESH_MODEL_RGB_SRV_ELM2_DEFINE.
 */
#define BLE_MESH_MODEL_RGB_ELM2_SRV(srv_pub, srv_data) \
    ESP_BLE_MESH_MODEL_LIGHT_HSL_SAT_SRV(srv_pub, srv_data)

/**
 * @brief   RGB control Server Model change callback function type
 * @param   event: Event type
 * @param   param: Pointer to callback parameter
 */
typedef void (* ble_mesh_rgb_control_server_change_cb_t)(rgb_t new_rgb);

esp_err_t ble_mesh_rgb_control_server_register_change_callback(ble_mesh_rgb_control_server_change_cb_t callback);

#endif // BLE_MESH_RGB_CONTROL_SERVER_H_