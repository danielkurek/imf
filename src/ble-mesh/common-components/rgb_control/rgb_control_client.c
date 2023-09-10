#include "rgb_control_client.h"
#include "hsl.h"

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