#ifndef BLE_MESH_RGB_CONTROL_SERVER_H_
#define BLE_MESH_RGB_CONTROL_SERVER_H_

#include "esp_ble_mesh_defs.h"
#include "esp_ble_mesh_common_api.h"
#include "esp_ble_mesh_lighting_model_api.h"
#include "color.h"

#ifdef CONFIG_BLE_MESH_RGB_CONTROL_SERVER

static esp_ble_mesh_light_hsl_state_t hsl_state = {
    .lightness_default = UINT16_MAX/2,
    .hue_default = UINT16_MAX/2,
    .saturation_default = UINT16_MAX/2,
    .hue_range_min = 0,
    .hue_range_max = UINT16_MAX,
    .saturation_range_min = 0,
    .saturation_range_max = UINT16_MAX,
};

ESP_BLE_MESH_MODEL_PUB_DEFINE(light_hsl_pub, 2 + 30, ROLE_NODE);
static esp_ble_mesh_light_hsl_srv_t light_hsl_srv = {
    .rsp_ctrl.get_auto_rsp = ESP_BLE_MESH_SERVER_AUTO_RSP,
    .rsp_ctrl.set_auto_rsp = ESP_BLE_MESH_SERVER_AUTO_RSP,
    .rsp_ctrl.status_auto_rsp = ESP_BLE_MESH_SERVER_AUTO_RSP,
    .state = &hsl_state,
};

ESP_BLE_MESH_MODEL_PUB_DEFINE(light_hsl_setup_pub, 2 + 30, ROLE_NODE);
static esp_ble_mesh_light_hsl_setup_srv_t light_hsl_setup_srv = {
    .rsp_ctrl.get_auto_rsp = ESP_BLE_MESH_SERVER_AUTO_RSP,
    .rsp_ctrl.set_auto_rsp = ESP_BLE_MESH_SERVER_AUTO_RSP,
    .rsp_ctrl.status_auto_rsp = ESP_BLE_MESH_SERVER_AUTO_RSP,
    .state = &hsl_state,
};

ESP_BLE_MESH_MODEL_PUB_DEFINE(light_hsl_hue_pub, 2 + 30, ROLE_NODE);
static esp_ble_mesh_light_hsl_hue_srv_t light_hsl_hue_srv = {
    .rsp_ctrl.get_auto_rsp = ESP_BLE_MESH_SERVER_AUTO_RSP,
    .rsp_ctrl.set_auto_rsp = ESP_BLE_MESH_SERVER_AUTO_RSP,
    .rsp_ctrl.status_auto_rsp = ESP_BLE_MESH_SERVER_AUTO_RSP,
    .state = &hsl_state,
};

ESP_BLE_MESH_MODEL_PUB_DEFINE(light_hsl_sat_pub, 2 + 30, ROLE_NODE);
static esp_ble_mesh_light_hsl_sat_srv_t light_hsl_sat_srv = {
    .rsp_ctrl.get_auto_rsp = ESP_BLE_MESH_SERVER_AUTO_RSP,
    .rsp_ctrl.set_auto_rsp = ESP_BLE_MESH_SERVER_AUTO_RSP,
    .rsp_ctrl.status_auto_rsp = ESP_BLE_MESH_SERVER_AUTO_RSP,
    .state = &hsl_state,
};

#define BLE_MESH_MODEL_RGB_SRV ESP_BLE_MESH_MODEL_LIGHT_HSL_SRV(&light_hsl_pub, &light_hsl_srv)
#define BLE_MESH_MODEL_RGB_SETUP_SRV ESP_BLE_MESH_MODEL_LIGHT_HSL_SETUP_SRV(&light_hsl_setup_pub, &light_hsl_setup_srv)
#define BLE_MESH_MODEL_RGB_HUE_SRV ESP_BLE_MESH_MODEL_LIGHT_HSL_HUE_SRV(&light_hsl_hue_pub, &light_hsl_hue_srv)
#define BLE_MESH_MODEL_RGB_SAT_SRV ESP_BLE_MESH_MODEL_LIGHT_HSL_SAT_SRV(&light_hsl_sat_pub, &light_hsl_sat_srv)

/**
 * @brief   RGB control Server Model change callback function type
 * @param   event: Event type
 * @param   param: Pointer to callback parameter
 */
typedef void (* ble_mesh_rgb_control_server_change_cb_t)(rgb_t new_rgb);

esp_err_t ble_mesh_rgb_control_server_register_change_callback(ble_mesh_rgb_control_server_change_cb_t callback);

#endif // CONFIG_BLE_MESH_RGB_CONTROL_SERVER

#endif // BLE_MESH_RGB_CONTROL_SERVER_H_