#ifndef BLE_MESH_H_
#define BLE_MESH_H_

#include "color.h"
#include <inttypes.h>
#include "board.h"
#include "rgb_control_client.h"
#include "location_defs.h"

#ifdef __cplusplus
extern "C"{
#endif


esp_err_t ble_mesh_init();

void ble_mesh_set_rgb_conf(rgb_conf_t *conf);

esp_err_t ble_mesh_set_rgb(uint16_t addr, rgb_t color, bool ack);

rgb_response_t ble_mesh_get_rgb(uint16_t addr);

esp_err_t ble_mesh_set_loc_local(uint16_t addr, const location_local_t *loc_local);

esp_err_t ble_mesh_get_loc_local(uint16_t addr, location_local_t *result);

#ifdef __cplusplus
}
#endif

#endif