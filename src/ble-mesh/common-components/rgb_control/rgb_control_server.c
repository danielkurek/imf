#include "rgb_control_server.h"
#include "hsl.h"

#ifdef CONFIG_BLE_MESH_RGB_CONTROL_SERVER

static const char* TAG = "RGB_server";

static ble_mesh_rgb_control_server_change_cb_t rgb_control_change_callback = NULL;

static void hsl_change(hsl_t new_hsl){
    rgb_t new_rgb = hsl_to_rgb(new_hsl);
    
    if(rgb_control_change_callback != NULL){
        rgb_control_change_callback(new_rgb);
    }
}

static void ble_mesh_lightning_server_cb(esp_ble_mesh_lighting_server_cb_event_t event,
                                                 esp_ble_mesh_lighting_server_cb_param_t *param)
{
    ESP_LOGI(TAG, "event 0x%02x, opcode 0x%04" PRIx32 ", src 0x%04x, dst 0x%04x",
        event, param->ctx.recv_op, param->ctx.addr, param->ctx.recv_dst);

    hsl_t hsl;

    switch (event) {
    case ESP_BLE_MESH_LIGHTING_SERVER_STATE_CHANGE_EVT:
        ESP_LOGI(TAG, "ESP_BLE_MESH_LIGHTING_SERVER_STATE_CHANGE_EVT");
        // update RGB value from model
        switch(param->ctx.recv_op){
            case ESP_BLE_MESH_MODEL_OP_LIGHT_HSL_SET:
            case ESP_BLE_MESH_MODEL_OP_LIGHT_HSL_SET_UNACK:
                hsl.hue = param->value.state_change.hsl_set.hue;
                hsl.saturation = param->value.state_change.hsl_set.saturation;
                hsl.lightness = param->value.state_change.hsl_set.lightness;
                hsl_change(hsl);
                break;
        }
        break;
    // should not happen since get_auto_rsp is set to ESP_BLE_MESH_SERVER_AUTO_RSP
    case ESP_BLE_MESH_LIGHTING_SERVER_RECV_GET_MSG_EVT:
        break;
    // should not happen since get_auto_rsp is set to ESP_BLE_MESH_SERVER_AUTO_RSP
    case ESP_BLE_MESH_LIGHTING_SERVER_RECV_SET_MSG_EVT:
        break;
    // should not happen since get_auto_rsp is set to ESP_BLE_MESH_SERVER_AUTO_RSP
    case ESP_BLE_MESH_LIGHTING_SERVER_RECV_STATUS_MSG_EVT:
        break;
    default:
        ESP_LOGE(TAG, "Unknown Generic Server event 0x%02x", event);
        break;
    }
}

esp_err_t ble_mesh_rgb_control_server_register_change_callback(ble_mesh_rgb_control_server_change_cb_t callback){
    esp_err_t err = esp_ble_mesh_register_lighting_server_callback(ble_mesh_lightning_server_cb);
    rgb_control_change_callback = callback;
    return err;
}


#endif // CONFIG_BLE_MESH_RGB_CONTROL_SERVER