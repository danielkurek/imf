#ifndef BLE_MESH_RGB_CONTROL_CLIENT_H_
#define BLE_MESH_RGB_CONTROL_CLIENT_H_

#include "esp_ble_mesh_lighting_model_api.h"
#include "color/color.h"

#define BLE_MESH_RGB_CLI_PUB_DEFINE(_name, _role) \
    ESP_BLE_MESH_MODEL_PUB_DEFINE(_name, 2 + 30, _role);

// static esp_ble_mesh_client_t light_hsl_client;

/**
 *  cli_pub Pointer to the unique struct esp_ble_mesh_model_pub_t.
 *  cli_data Pointer to the unique struct esp_ble_mesh_client_t.
 */
#define BLE_MESH_MODEL_RGB_CLI(cli_pub, cli_data) \
    ESP_BLE_MESH_MODEL_LIGHT_HSL_CLI(cli_pub, cli_data)

#define BLE_MESH_MODEL_OP_RGB_GET ESP_BLE_MESH_MODEL_OP_LIGHT_HSL_GET
#define BLE_MESH_MODEL_OP_RGB_SET ESP_BLE_MESH_MODEL_OP_LIGHT_HSL_SET
#define BLE_MESH_MODEL_OP_RGB_SET_UNACK ESP_BLE_MESH_MODEL_OP_LIGHT_HSL_SET_UNACK
typedef struct {
    bool     op_en;       /*!< Indicate if optional parameters are included */
    rgb_t rgb;            /*!< Target value of light red state */
    uint8_t  tid;         /*!< Transaction ID */
    uint8_t  trans_time;  /*!< Time to complete state transition (optional) */
    uint8_t  delay;       /*!< Indicate message execution delay (C.1) */
} esp_ble_mesh_rgb_set_t;

typedef void(*ble_mesh_rgb_client_get_cb)(uint16_t addr, rgb_t rgb);

void ble_mesh_rgb_client_register_get_cb(ble_mesh_rgb_client_get_cb get_cb);
esp_err_t ble_mesh_rgb_client_set_state(esp_ble_mesh_client_common_param_t *common, esp_ble_mesh_rgb_set_t *set_state);
esp_err_t ble_mesh_rgb_client_get_state(esp_ble_mesh_client_common_param_t *common, rgb_t *color_out);
esp_err_t ble_mesh_rgb_client_init();

#endif