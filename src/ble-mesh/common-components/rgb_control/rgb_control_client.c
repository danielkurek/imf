#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "rgb_control_client.h"
#include "color/hsl.h"


#define RGB_GET_BIT BIT0
#define RGB_FAIL_BIT BIT1

static const char *TAG = "RGBCLI";

static ble_mesh_rgb_client_get_cb rgb_get_cb = NULL;

void ble_mesh_rgb_client_register_get_cb(ble_mesh_rgb_client_get_cb get_cb){
    rgb_get_cb = get_cb;
}

static void ble_mesh_light_client_cb(esp_ble_mesh_light_client_cb_event_t event,
                                     esp_ble_mesh_light_client_cb_param_t *param){
    ESP_LOGI(TAG, "event 0x%02x, opcode 0x%04" PRIx32 ", src 0x%04x, dst 0x%04x",
        event, param->params->ctx.recv_op, param->params->ctx.addr, param->params->ctx.recv_dst);
    switch(event){
        case ESP_BLE_MESH_LIGHT_CLIENT_PUBLISH_EVT:
            // this event can happen when GET command is sent to group address
            ESP_LOGI(TAG, "ESP_BLE_MESH_LIGHT_CLIENT_PUBLISH_EVT");
        case ESP_BLE_MESH_LIGHT_CLIENT_GET_STATE_EVT:
            ESP_LOGI(TAG, "ESP_BLE_MESH_LIGHT_CLIENT_GET_STATE_EVT");
            esp_ble_mesh_light_hsl_status_cb_t hsl_status = param->status_cb.hsl_status;
            if(rgb_get_cb){
                hsl_t hsl = {
                    .lightness=hsl_status.hsl_lightness, 
                    .hue=hsl_status.hsl_hue, 
                    .saturation=hsl_status.hsl_saturation};
                rgb_t rgb = hsl_to_rgb(hsl);
                rgb_get_cb(param->params->ctx.addr, rgb);
            }
            return;
        case ESP_BLE_MESH_LIGHT_CLIENT_SET_STATE_EVT:
            ESP_LOGI(TAG, "ESP_BLE_MESH_LIGHT_CLIENT_SET_STATE_EVT");
            break;
        case ESP_BLE_MESH_LIGHT_CLIENT_TIMEOUT_EVT:
            ESP_LOGI(TAG, "ESP_BLE_MESH_LIGHT_CLIENT_TIMEOUT_EVT");
            return;
        default:
            ESP_LOGI(TAG, "Unknown esp_ble_mesh_light_client_cb_event_t event");
    }
}

esp_err_t ble_mesh_rgb_client_set_state(esp_ble_mesh_client_common_param_t *common, esp_ble_mesh_rgb_set_t *set_state){
    esp_ble_mesh_light_client_set_state_t set = {0};
    
    hsl_t hsl = rgb_to_hsl(set_state->rgb);

    set.hsl_set.op_en = set_state->op_en;
    set.hsl_set.hsl_lightness = hsl.lightness;
    set.hsl_set.hsl_hue = hsl.hue;
    set.hsl_set.hsl_saturation = hsl.saturation;
    set.hsl_set.tid = set_state->tid;
    set.hsl_set.trans_time = set_state->trans_time;
    set.hsl_set.delay = set_state->delay;

    return esp_ble_mesh_light_client_set_state(common, &set);
}

esp_err_t ble_mesh_rgb_client_get_state(esp_ble_mesh_client_common_param_t *common){
    esp_ble_mesh_light_client_get_state_t get = {0};

    return esp_ble_mesh_light_client_get_state(common, &get);
}

esp_err_t ble_mesh_rgb_client_init(){
    return esp_ble_mesh_register_light_client_callback(ble_mesh_light_client_cb);
}