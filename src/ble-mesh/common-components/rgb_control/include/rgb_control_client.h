#ifndef BLE_MESH_RGB_CONTROL_CLIENT_H_
#define BLE_MESH_RGB_CONTROL_CLIENT_H_

#include "esp_ble_mesh_defs.h"
#include "esp_ble_mesh_common_api.h"
#include "esp_ble_mesh_lighting_model_api.h"

#ifdef CONFIG_BLE_MESH_RGB_CONTROL_CLIENT
ESP_BLE_MESH_MODEL_PUB_DEFINE(light_hsl_cli_pub, 2 + 30, ROLE_NODE);
static esp_ble_mesh_client_t light_hsl_client;

#define BLE_MESH_MODEL_RGB_CLI ESP_BLE_MESH_MODEL_LIGHT_HSL_CLI(&light_hsl_cli_pub, &light_hsl_client)
#endif

#endif