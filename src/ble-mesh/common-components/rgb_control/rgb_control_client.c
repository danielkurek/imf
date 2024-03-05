#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "rgb_control_client.h"
#include "color/hsl.h"


#define RGB_GET_BIT BIT0
#define RGB_FAIL_BIT BIT1

static const char *TAG = "RGBCLI";

static EventGroupHandle_t s_rgb_event_group;

typedef struct{
    esp_ble_mesh_msg_ctx_t ctx;
    esp_ble_mesh_light_hsl_status_cb_t hsl_status;
} rgb_event_cb_param_t;

static rgb_event_cb_param_t rgb_event_cb_param;

static ble_mesh_rgb_client_get_cb rgb_get_cb = NULL;

void ble_mesh_rgb_client_register_get_cb(ble_mesh_rgb_client_get_cb get_cb){
    rgb_get_cb = get_cb;
}

static void ble_mesh_light_client_cb(esp_ble_mesh_light_client_cb_event_t event,
                                     esp_ble_mesh_light_client_cb_param_t *param){
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

            rgb_event_cb_param.ctx = param->params->ctx;
            rgb_event_cb_param.hsl_status = param->status_cb.hsl_status;
            xEventGroupSetBits(s_rgb_event_group, RGB_GET_BIT);
            return;
        case ESP_BLE_MESH_LIGHT_CLIENT_SET_STATE_EVT:
            ESP_LOGI(TAG, "ESP_BLE_MESH_LIGHT_CLIENT_SET_STATE_EVT");
            break;
        case ESP_BLE_MESH_LIGHT_CLIENT_TIMEOUT_EVT:
            ESP_LOGI(TAG, "ESP_BLE_MESH_LIGHT_CLIENT_TIMEOUT_EVT");
            rgb_event_cb_param.ctx = param->params->ctx;
            rgb_event_cb_param.hsl_status = param->status_cb.hsl_status;
            xEventGroupSetBits(s_rgb_event_group, RGB_FAIL_BIT);
            return;
        default:
            ESP_LOGI(TAG, "Unknown esp_ble_mesh_light_client_cb_event_t event");
    }
    ESP_LOGI(TAG, "Bits not set by event");
    xEventGroupSetBits(s_rgb_event_group, RGB_FAIL_BIT);
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

esp_err_t ble_mesh_rgb_client_get_state(esp_ble_mesh_client_common_param_t *common, rgb_t *color_out){
    EventBits_t bits;
    esp_err_t err;
    
    esp_ble_mesh_light_client_get_state_t get = {0};

    err = esp_ble_mesh_light_client_get_state(common, &get);
    if(err != ESP_OK){
        return err;
    }

    while(true){
        // clear bits so that timeout of xEventGroupWaitBits does not have bits set from previous event
        xEventGroupClearBits(s_rgb_event_group, RGB_GET_BIT | RGB_FAIL_BIT);
        bits = xEventGroupWaitBits(s_rgb_event_group, RGB_GET_BIT | RGB_FAIL_BIT,
                                        pdTRUE, pdFALSE, 500 / portTICK_PERIOD_MS);
        // check rgb_event_cb_param.ctx.addr or rgb_event_cb_param.ctx.recv_dst
        // compare it to common->ctx.addr
        ESP_LOGI(TAG, "rgb_event_cb_param.ctx.addr=0x%04" PRIx16 " | rgb_event_cb_param.ctx.recv_dst=0x%04" PRIx16 " | common->ctx.addr=0x%04" PRIx16, rgb_event_cb_param.ctx.addr, rgb_event_cb_param.ctx.recv_dst, common->ctx.addr);
        hsl_t hsl = {
            .hue = rgb_event_cb_param.hsl_status.hsl_hue,
            .saturation = rgb_event_cb_param.hsl_status.hsl_saturation,
            .lightness = rgb_event_cb_param.hsl_status.hsl_lightness,
        };
        
        (*color_out) = hsl_to_rgb(hsl);
        ESP_LOGI(TAG, "hsl.hue=%d, hsl.saturation=%d, hsl.lightness=%d, rgb.red=%d, rgb.green=%d, rgb.blue=%d", hsl.hue, hsl.saturation, hsl.lightness, color_out->red, color_out->green, color_out->blue);
        if(bits & RGB_GET_BIT){
            ESP_LOGI(TAG, "Success GET");
            return ESP_OK;
        } else {
            ESP_LOGI(TAG, "Failed GET");
            return ESP_FAIL;
        }
    }
}

esp_err_t ble_mesh_rgb_client_init(){
    s_rgb_event_group = xEventGroupCreate();

    return esp_ble_mesh_register_light_client_callback(ble_mesh_light_client_cb);
}