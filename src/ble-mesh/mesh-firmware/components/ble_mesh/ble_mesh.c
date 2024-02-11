/* main.c - Application main entry point */

/*
 * SPDX-FileCopyrightText: 2017 Intel Corporation
 * SPDX-FileContributor: 2018-2021 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

// Library Header

#include "ble_mesh.h"

// General imports

#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include "esp_log.h"
#include "nvs_flash.h"

// BLE-mesh includes

#include "esp_ble_mesh_defs.h"
#include "esp_ble_mesh_common_api.h"
#include "esp_ble_mesh_networking_api.h"
#include "esp_ble_mesh_provisioning_api.h"
#include "esp_ble_mesh_config_model_api.h"
#include "esp_ble_mesh_generic_model_api.h"
#include "esp_ble_mesh_health_model_api.h"
#include "esp_ble_mesh_lighting_model_api.h"
#include "esp_ble_mesh_local_data_operation_api.h"

// Additional functions for this project

#include "board.h"
#include "ble_mesh_example_init.h"
#include "ble_mesh_example_nvs.h"

#include "rgb_control_server.h"
#include "rgb_control_client.h"

#include "driver/gpio.h"

#include "color/color.h"
#include "color/hsl.h"

#include "logger.h"

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#define LOC_LOCAL_SIZE 9

#define EVENT_GET_SUCCESS_BIT BIT0
#define EVENT_GET_FAIL_BIT BIT1

// tag for logging
static const char* TAG = "BLE-MESH";

static rgb_conf_t *rgb_conf;

static EventGroupHandle_t s_location_event_group;
static struct{
    location_local_t location;
    uint16_t addr;   
} s_location_event_data;

static EventGroupHandle_t s_onoff_event_group;
static struct{
    bool onoff;
    uint16_t addr;   
} s_onoff_event_data;

static EventGroupHandle_t s_level_event_group;
static struct{
    int16_t level;
    uint16_t addr;   
} s_level_event_data;

// company ID who implemented BLE-mesh
// needs to be member of Bluetooth group
// we can use Espressif ID
#define CID_ESP 0x02E5

// NVS handle for reading the web_config options
static nvs_handle_t config_nvs;

// uuid of the device (that runs this code)
// later is initialized by `ble_mesh_get_dev_uuid`
static uint8_t dev_uuid[16] = { 0xdd, 0xdd };

// struct that is stored in NVS
// holds the current information about BLE-mesh
// that is restored when device is rebooted
// net_idx, app_idx and tid are required
// for proper functioning of the network
static struct example_info_store {
    uint16_t net_idx;   /* NetKey Index */
    uint16_t app_idx;   /* AppKey Index */
    uint8_t  tid;       /* Message TID */
} __attribute__((packed)) store = {
    .net_idx = ESP_BLE_MESH_KEY_UNUSED,
    .app_idx = ESP_BLE_MESH_KEY_UNUSED,
    .tid = 0x0,
};

// variables for working with NVS
static nvs_handle_t NVS_HANDLE; // stores opened stream to NVS
static const char * NVS_KEY = "mesh-device"; // key that will store the struct state

/*
 * Definitions of models
 */

// configuration of BLE-mesh
static esp_ble_mesh_cfg_srv_t config_server = {
    .relay = ESP_BLE_MESH_RELAY_ENABLED,
    .beacon = ESP_BLE_MESH_BEACON_ENABLED,
#if defined(CONFIG_BLE_MESH_FRIEND)
    .friend_state = ESP_BLE_MESH_FRIEND_ENABLED,
#else
    .friend_state = ESP_BLE_MESH_FRIEND_NOT_SUPPORTED,
#endif
#if defined(CONFIG_BLE_MESH_GATT_PROXY_SERVER)
    .gatt_proxy = ESP_BLE_MESH_GATT_PROXY_ENABLED,
#else
    .gatt_proxy = ESP_BLE_MESH_GATT_PROXY_NOT_SUPPORTED,
#endif
    .default_ttl = 7,
    /* 3 transmissions with 20ms interval */
    .net_transmit = ESP_BLE_MESH_TRANSMIT(2, 20),
    .relay_retransmit = ESP_BLE_MESH_TRANSMIT(2, 20),
};

// definition of OnOff model
ESP_BLE_MESH_MODEL_PUB_DEFINE(onoff_pub_0, 2 + 3, ROLE_NODE);
static esp_ble_mesh_gen_onoff_srv_t onoff_server_0 = {
    .rsp_ctrl.get_auto_rsp = ESP_BLE_MESH_SERVER_AUTO_RSP, // automatic response
    .rsp_ctrl.set_auto_rsp = ESP_BLE_MESH_SERVER_AUTO_RSP, // automatic response
};
// automatic response has the key advantage that we do not have to code the tedious part
// of responding to commands, we only need to wait for an event that notifies us that
// state of server has changed



// OnOff client
static esp_ble_mesh_client_t onoff_client;
ESP_BLE_MESH_MODEL_PUB_DEFINE(onoff_cli_pub, 2 + 1, ROLE_NODE);

// definitions of Level model
ESP_BLE_MESH_MODEL_PUB_DEFINE(level_srv_pub, 2 + 4, ROLE_NODE);
esp_ble_mesh_gen_level_srv_t level_server = {
    .rsp_ctrl.get_auto_rsp = ESP_BLE_MESH_SERVER_AUTO_RSP, // automatic response
    .rsp_ctrl.set_auto_rsp = ESP_BLE_MESH_SERVER_AUTO_RSP, // automatic response
};

static esp_ble_mesh_client_t level_client;
ESP_BLE_MESH_MODEL_PUB_DEFINE(level_cli_pub, 2 + 4, ROLE_NODE);

// definition of Health model
// it is used for device identification during provisioning
// (led starts blinking when we click `Identify`)
uint8_t test_ids[] = {0x00};

/** ESP BLE Mesh Health Server Model Context */
ESP_BLE_MESH_MODEL_PUB_DEFINE(health_pub, 2 + 11, ROLE_NODE);
static esp_ble_mesh_health_srv_t health_server = {
    .health_test.id_count = 1,
    .health_test.test_ids = test_ids,
};

// RGB server definitions
BLE_MESH_MODEL_RGB_STATE_DEFINE(rgb_state);
BLE_MESH_MODEL_RGB_SRV_PUB_DEFINE(rgb_srv_pub, ROLE_NODE);
BLE_MESH_MODEL_RGB_SRV_DEFINE(rgb_srv, &rgb_state);
BLE_MESH_MODEL_RGB_SRV_PUB_DEFINE(rgb_setup_pub, ROLE_NODE);
BLE_MESH_MODEL_RGB_SETUP_SRV_DEFINE(rgb_setup_srv, &rgb_state);
BLE_MESH_MODEL_RGB_SRV_PUB_DEFINE(rgb_elm1_pub, ROLE_NODE);
BLE_MESH_MODEL_RGB_SRV_ELM1_DEFINE(rgb_srv_elm1, &rgb_state);
BLE_MESH_MODEL_RGB_SRV_PUB_DEFINE(rgb_elm2_pub, ROLE_NODE);
BLE_MESH_MODEL_RGB_SRV_ELM2_DEFINE(rgb_srv_elm2, &rgb_state);

// RGB client definitions
static esp_ble_mesh_client_t rgb_client;
BLE_MESH_RGB_CLI_PUB_DEFINE(rgb_cli_pub, ROLE_NODE);

// Location server definition
static esp_ble_mesh_gen_location_state_t location_state = {};

ESP_BLE_MESH_MODEL_PUB_DEFINE(location_srv_pub, 2 + LOC_LOCAL_SIZE, ROLE_NODE);
static esp_ble_mesh_gen_location_srv_t location_server = {
    .rsp_ctrl.get_auto_rsp = ESP_BLE_MESH_SERVER_AUTO_RSP,
    .rsp_ctrl.set_auto_rsp = ESP_BLE_MESH_SERVER_AUTO_RSP,
    .state = &location_state
};

ESP_BLE_MESH_MODEL_PUB_DEFINE(location_setup_pub, 2 + LOC_LOCAL_SIZE, ROLE_NODE);
static esp_ble_mesh_gen_location_setup_srv_t location_setup_server = {
    .rsp_ctrl.get_auto_rsp = ESP_BLE_MESH_SERVER_AUTO_RSP,
    .rsp_ctrl.set_auto_rsp = ESP_BLE_MESH_SERVER_AUTO_RSP,
    .state = &location_state
};

// Location client definition
ESP_BLE_MESH_MODEL_PUB_DEFINE(location_cli_pub, 2 + LOC_LOCAL_SIZE, ROLE_NODE);
static esp_ble_mesh_client_t location_client;

/*
 * Definitions of BLE-mesh Elements
 */

// definition of the main element
// it is the main one because it contains config server
static esp_ble_mesh_model_t root_models[] = {
    ESP_BLE_MESH_MODEL_CFG_SRV(&config_server),
    ESP_BLE_MESH_MODEL_GEN_ONOFF_SRV(&onoff_pub_0, &onoff_server_0),
    ESP_BLE_MESH_MODEL_GEN_ONOFF_CLI(&onoff_cli_pub, &onoff_client),
    ESP_BLE_MESH_MODEL_HEALTH_SRV(&health_server, &health_pub),
    BLE_MESH_MODEL_RGB_SRV(&rgb_srv_pub, &rgb_srv),
    BLE_MESH_MODEL_RGB_SETUP_SRV(&rgb_setup_pub, &rgb_setup_srv),
    BLE_MESH_MODEL_RGB_CLI(&rgb_cli_pub, &rgb_client),
    ESP_BLE_MESH_MODEL_GEN_LOCATION_SRV(&location_srv_pub, &location_server),
    ESP_BLE_MESH_MODEL_GEN_LOCATION_SETUP_SRV(&location_setup_pub, &location_setup_server),
    ESP_BLE_MESH_MODEL_GEN_LOCATION_CLI(&location_cli_pub, &location_client),
    ESP_BLE_MESH_MODEL_GEN_LEVEL_SRV(&level_srv_pub, &level_server),
    ESP_BLE_MESH_MODEL_GEN_LEVEL_CLI(&level_cli_pub, &level_client),
};

// additional elements that are needed for RGB server
static esp_ble_mesh_model_t extend_model_0[] = {
    BLE_MESH_MODEL_RGB_ELM1_SRV(&rgb_elm1_pub, &rgb_srv_elm1),
};
static esp_ble_mesh_model_t extend_model_1[] = {
    BLE_MESH_MODEL_RGB_ELM2_SRV(&rgb_elm2_pub, &rgb_srv_elm2),
};

// array of elements used in this node
static esp_ble_mesh_elem_t elements[] = {
    ESP_BLE_MESH_ELEMENT(0, root_models, ESP_BLE_MESH_MODEL_NONE),
    ESP_BLE_MESH_ELEMENT(0, extend_model_0, ESP_BLE_MESH_MODEL_NONE),
    ESP_BLE_MESH_ELEMENT(0, extend_model_1, ESP_BLE_MESH_MODEL_NONE),
};

// defines the composition of the node
// which includes which elements are used
// and IDs of the product and manufacturer
static esp_ble_mesh_comp_t composition = {
    .cid = CID_ESP,
    .elements = elements,
    .element_count = ARRAY_SIZE(elements),
};

/* Disable OOB security for SILabs Android app */
static esp_ble_mesh_prov_t provision = {
    .uuid = dev_uuid,
#if 0
    .output_size = 4,
    .output_actions = ESP_BLE_MESH_DISPLAY_NUMBER,
    .input_actions = ESP_BLE_MESH_PUSH,
    .input_size = 4,
#else
    .output_size = 0,
    .output_actions = 0,
#endif
};

// helper function for storing information struct
// we need to be careful when call it because
// if we store invalid data, the device will
// need to be provisioned again
static void mesh_example_info_store(void)
{
    ble_mesh_nvs_store(NVS_HANDLE, NVS_KEY, &store, sizeof(store));
}

// helper function for restoring information struct
// needs to be restored at the right place
static void mesh_example_info_restore(void)
{
    esp_err_t err = ESP_OK;
    bool exist = false;

    err = ble_mesh_nvs_restore(NVS_HANDLE, NVS_KEY, &store, sizeof(store), &exist);
    if (err != ESP_OK) {
        return;
    }

    if (exist) {
        LOGGER_I(TAG, "Restore, net_idx 0x%04x, app_idx 0x%04x, onoff %u, tid 0x%02x",
            store.net_idx, store.app_idx, store.tid);
    }
}

// callback for completion of provisioning of this device
static void prov_complete(uint16_t net_idx, uint16_t addr, uint8_t flags, uint32_t iv_index)
{
    LOGGER_I(TAG, "net_idx: 0x%04x, addr: 0x%04x", net_idx, addr);
    LOGGER_I(TAG, "flags: 0x%02x, iv_index: 0x%08" PRIx32, flags, iv_index);
    board_set_rgb(&internal_rgb_conf, GREEN);
    store.net_idx = net_idx;
    /* mesh_example_info_store() shall not be invoked here, because if the device
     * is restarted and goes into a provisioned state, then the following events
     * will come:
     * 1st: ESP_BLE_MESH_NODE_PROV_COMPLETE_EVT
     * 2nd: ESP_BLE_MESH_PROV_REGISTER_COMP_EVT
     * So the store.net_idx will be updated here, and if we store the mesh example
     * info here, the wrong app_idx (initialized with 0xFFFF) will be stored in nvs
     * just before restoring it.
     */
}

static void change_local_loc_state(esp_ble_mesh_state_change_gen_loc_local_set_t loc_local){

}

// Callback for Generic server
// this is needed for OnOff Server
// because it does not have its own callback
static void example_ble_mesh_generic_server_cb(esp_ble_mesh_generic_server_cb_event_t event,
                                               esp_ble_mesh_generic_server_cb_param_t *param)
{
    esp_ble_mesh_gen_onoff_srv_t *srv;
    LOGGER_I(TAG, "event 0x%02x, opcode 0x%04" PRIx32 ", src 0x%04x, dst 0x%04x",
        event, param->ctx.recv_op, param->ctx.addr, param->ctx.recv_dst);

    switch (event) {
    case ESP_BLE_MESH_GENERIC_SERVER_STATE_CHANGE_EVT:
        // Triggered by SET operation, only if SET AUTO_RSP is set
        LOGGER_I(TAG, "ESP_BLE_MESH_GENERIC_SERVER_STATE_CHANGE_EVT");
        if (param->ctx.recv_op == ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_SET ||
            param->ctx.recv_op == ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_SET_UNACK) {
            LOGGER_I(TAG, "onoff 0x%02x", param->value.state_change.onoff_set.onoff);
            board_set_onoff(&internal_rgb_conf, param->value.state_change.onoff_set.onoff);
        }
        if (param->ctx.recv_op == ESP_BLE_MESH_MODEL_OP_GEN_LOC_LOCAL_SET ||
            param->ctx.recv_op == ESP_BLE_MESH_MODEL_OP_GEN_LOC_LOCAL_SET_UNACK) {
            esp_ble_mesh_state_change_gen_loc_local_set_t loc_change = param->value.state_change.loc_local_set;
            LOGGER_I(TAG, "local location: N=%" PRId16 " E=%" PRId16" Alt=%" PRId16 " Floor=%" PRIu8 " Uncertainty=%" PRIu16, 
                                        loc_change.north, 
                                        loc_change.east,
                                        loc_change.altitude,
                                        loc_change.floor_number,
                                        loc_change.uncertainty);
            change_local_loc_state(loc_change);
        }
        break;
    case ESP_BLE_MESH_GENERIC_SERVER_RECV_GET_MSG_EVT:
        // Triggered by GET operation, only if GET RSP_BY_APP is set
        // we should send back a response message
        LOGGER_I(TAG, "ESP_BLE_MESH_GENERIC_SERVER_RECV_GET_MSG_EVT");
        break;
    case ESP_BLE_MESH_GENERIC_SERVER_RECV_SET_MSG_EVT:
        // Triggered by GET operation, only if SET RSP_BY_APP is set
        // we should send back a response message
        LOGGER_I(TAG, "ESP_BLE_MESH_GENERIC_SERVER_RECV_SET_MSG_EVT");
        break;
    default:
        LOGGER_E(TAG, "Unknown Generic Server event 0x%02x", event);
        break;
    }
}

void generic_client_cb(esp_ble_mesh_generic_client_cb_event_t event,
                       esp_ble_mesh_generic_client_cb_param_t *param){
    esp_ble_mesh_gen_onoff_srv_t *srv;
    LOGGER_I(TAG, "event 0x%02x, opcode 0x%04" PRIx32 ", src 0x%04x, dst 0x%04x",
        event, param->params->ctx.recv_op, param->params->ctx.addr, param->params->ctx.recv_dst);

    switch (event) {
    case ESP_BLE_MESH_GENERIC_CLIENT_GET_STATE_EVT:
        LOGGER_I(TAG, "ESP_BLE_MESH_GENERIC_CLIENT_GET_STATE_EVT");
        if(param->params->opcode == ESP_BLE_MESH_MODEL_OP_GEN_LOC_LOCAL_GET){
            s_location_event_data.location.local_north = param->status_cb.location_local_status.local_north;
            s_location_event_data.location.local_east = param->status_cb.location_local_status.local_east;
            s_location_event_data.location.local_altitude = param->status_cb.location_local_status.local_altitude;
            s_location_event_data.location.floor_number = param->status_cb.location_local_status.floor_number;
            s_location_event_data.location.uncertainty = param->status_cb.location_local_status.uncertainty;
            
            // TODO: test if this is the right address
            s_location_event_data.addr = param->params->ctx.addr;
            xEventGroupSetBits(s_location_event_group, EVENT_GET_SUCCESS_BIT);
        }
        if(param->params->opcode == ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_GET){
            s_onoff_event_data.onoff = param->status_cb.location_local_status.local_north;
            
            // TODO: test if this is the right address
            s_onoff_event_data.addr = param->params->ctx.addr;
            xEventGroupSetBits(s_onoff_event_group, EVENT_GET_SUCCESS_BIT);
        }
        if(param->params->opcode == ESP_BLE_MESH_MODEL_OP_GEN_LEVEL_GET){
            s_level_event_data.level = param->status_cb.level_status.present_level;
            
            // TODO: test if this is the right address
            s_onoff_event_data.addr = param->params->ctx.addr;
            xEventGroupSetBits(s_level_event_group, EVENT_GET_SUCCESS_BIT);
        }
        break;
    case ESP_BLE_MESH_GENERIC_CLIENT_SET_STATE_EVT:
        // Triggered by receiving acknowledgment packet after sending SET message through GENERIC_CLIENT
        LOGGER_I(TAG, "ESP_BLE_MESH_GENERIC_CLIENT_SET_STATE_EVT");
        break;
    case ESP_BLE_MESH_GENERIC_CLIENT_PUBLISH_EVT:
        // Triggered by receiving publishing message
        LOGGER_I(TAG, "ESP_BLE_MESH_GENERIC_CLIENT_PUBLISH_EVT");
        break;
    case ESP_BLE_MESH_GENERIC_CLIENT_TIMEOUT_EVT:
        // Timeout reached when sending a message
        LOGGER_I(TAG, "ESP_BLE_MESH_GENERIC_CLIENT_TIMEOUT_EVT");
        if(param->params->opcode == ESP_BLE_MESH_MODEL_OP_GEN_LOC_LOCAL_GET){
            xEventGroupSetBits(s_location_event_group, EVENT_GET_FAIL_BIT);
        }
        if(param->params->opcode == ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_GET){
            xEventGroupSetBits(s_onoff_event_group, EVENT_GET_FAIL_BIT);
        }
        break;
    default:
        LOGGER_E(TAG, "Unknown Generic Server event 0x%02x", event);
        break;
    }
}

// provisioning callback
// this is the right place where to restore the BLE-mesh information
// this is also the place where we know when the provisioning is completed
static void example_ble_mesh_provisioning_cb(esp_ble_mesh_prov_cb_event_t event,
                                             esp_ble_mesh_prov_cb_param_t *param)
{
    switch (event) {
    case ESP_BLE_MESH_PROV_REGISTER_COMP_EVT:
        LOGGER_I(TAG, "ESP_BLE_MESH_PROV_REGISTER_COMP_EVT, err_code %d", param->prov_register_comp.err_code);
        mesh_example_info_restore(); /* Restore proper mesh example info */
        board_set_rgb(&internal_rgb_conf, GREEN);
        break;
    case ESP_BLE_MESH_NODE_PROV_ENABLE_COMP_EVT:
        LOGGER_I(TAG, "ESP_BLE_MESH_NODE_PROV_ENABLE_COMP_EVT, err_code %d", param->node_prov_enable_comp.err_code);
        break;
    case ESP_BLE_MESH_NODE_PROV_LINK_OPEN_EVT:
        LOGGER_I(TAG, "ESP_BLE_MESH_NODE_PROV_LINK_OPEN_EVT, bearer %s",
            param->node_prov_link_open.bearer == ESP_BLE_MESH_PROV_ADV ? "PB-ADV" : "PB-GATT");
        break;
    case ESP_BLE_MESH_NODE_PROV_LINK_CLOSE_EVT:
        LOGGER_I(TAG, "ESP_BLE_MESH_NODE_PROV_LINK_CLOSE_EVT, bearer %s",
            param->node_prov_link_close.bearer == ESP_BLE_MESH_PROV_ADV ? "PB-ADV" : "PB-GATT");
        break;
    case ESP_BLE_MESH_NODE_PROV_COMPLETE_EVT:
        LOGGER_I(TAG, "ESP_BLE_MESH_NODE_PROV_COMPLETE_EVT");
        prov_complete(param->node_prov_complete.net_idx, param->node_prov_complete.addr,
            param->node_prov_complete.flags, param->node_prov_complete.iv_index);
        break;
    case ESP_BLE_MESH_NODE_PROV_RESET_EVT:
        LOGGER_I(TAG, "ESP_BLE_MESH_NODE_PROV_RESET_EVT");
        break;
    case ESP_BLE_MESH_NODE_SET_UNPROV_DEV_NAME_COMP_EVT:
        LOGGER_I(TAG, "ESP_BLE_MESH_NODE_SET_UNPROV_DEV_NAME_COMP_EVT, err_code %d", param->node_set_unprov_dev_name_comp.err_code);
        break;
    default:
        break;
    }
}


// callback for configuration server
static void example_ble_mesh_config_server_cb(esp_ble_mesh_cfg_server_cb_event_t event,
                                              esp_ble_mesh_cfg_server_cb_param_t *param)
{
    if (event == ESP_BLE_MESH_CFG_SERVER_STATE_CHANGE_EVT) {
        switch (param->ctx.recv_op) {
        case ESP_BLE_MESH_MODEL_OP_APP_KEY_ADD:
            LOGGER_I(TAG, "ESP_BLE_MESH_MODEL_OP_APP_KEY_ADD");
            LOGGER_I(TAG, "net_idx 0x%04x, app_idx 0x%04x",
                param->value.state_change.appkey_add.net_idx,
                param->value.state_change.appkey_add.app_idx);
            ESP_LOG_BUFFER_HEX("AppKey", param->value.state_change.appkey_add.app_key, 16);
            break;
        case ESP_BLE_MESH_MODEL_OP_MODEL_APP_BIND:
            LOGGER_I(TAG, "ESP_BLE_MESH_MODEL_OP_MODEL_APP_BIND");
            LOGGER_I(TAG, "elem_addr 0x%04x, app_idx 0x%04x, cid 0x%04x, mod_id 0x%04x",
                param->value.state_change.mod_app_bind.element_addr,
                param->value.state_change.mod_app_bind.app_idx,
                param->value.state_change.mod_app_bind.company_id,
                param->value.state_change.mod_app_bind.model_id);
            store.app_idx = param->value.state_change.mod_app_bind.app_idx;
            mesh_example_info_store(); /* Store proper mesh example info */
            break;
        case ESP_BLE_MESH_MODEL_OP_MODEL_SUB_ADD:
            LOGGER_I(TAG, "ESP_BLE_MESH_MODEL_OP_MODEL_SUB_ADD");
            LOGGER_I(TAG, "elem_addr 0x%04x, sub_addr 0x%04x, cid 0x%04x, mod_id 0x%04x",
                param->value.state_change.mod_sub_add.element_addr,
                param->value.state_change.mod_sub_add.sub_addr,
                param->value.state_change.mod_sub_add.company_id,
                param->value.state_change.mod_sub_add.model_id);
            break;
        default:
            break;
        }
    }
}

// health server callback
// handles blinking of the LED when identifying device during provisioning
static void example_ble_mesh_health_server_cb(esp_ble_mesh_health_server_cb_event_t event,
                                                 esp_ble_mesh_health_server_cb_param_t *param)
{
    if(event == ESP_BLE_MESH_HEALTH_SERVER_ATTENTION_ON_EVT){
        board_start_blinking(&internal_rgb_conf, 200 * 1000);
    }
    if(event == ESP_BLE_MESH_HEALTH_SERVER_ATTENTION_OFF_EVT){
        board_stop_blinking(&internal_rgb_conf);
    }
}


// callback for RGB server
// state has changed
static void update_light(rgb_t rgb){
    LOGGER_I(TAG, "set light to R:%d G:%d B:%d", rgb.red, rgb.green, rgb.blue);
    board_set_rgb(&internal_rgb_conf, rgb);
}

// initialization of BLE-mesh
// ideally it is called from app_main function
static esp_err_t mesh_init(void)
{
    esp_err_t err = ESP_OK;

    esp_ble_mesh_register_prov_callback(example_ble_mesh_provisioning_cb);
    esp_ble_mesh_register_config_server_callback(example_ble_mesh_config_server_cb);
    esp_ble_mesh_register_generic_server_callback(example_ble_mesh_generic_server_cb);
    esp_ble_mesh_register_generic_client_callback(generic_client_cb);
    esp_ble_mesh_register_health_server_callback(example_ble_mesh_health_server_cb);
    ble_mesh_rgb_control_server_register_change_callback(update_light);
    ble_mesh_rgb_client_init();

    // indicate that it the device is on
    // if it stays lit it means that the device needs to be provisioned
    board_set_rgb(&internal_rgb_conf, BLUE);
    
    err = esp_ble_mesh_init(&provision, &composition);
    if (err != ESP_OK) {
        LOGGER_E(TAG, "Failed to initialize mesh stack (err %d)", err);
        return err;
    }
    
    bt_mesh_set_device_name("TEST1");

    err = esp_ble_mesh_node_prov_enable(ESP_BLE_MESH_PROV_ADV | ESP_BLE_MESH_PROV_GATT);
    if (err != ESP_OK) {
        LOGGER_E(TAG, "Failed to enable mesh node (err %d)", err);
        return err;
    }

    LOGGER_I(TAG, "BLE Mesh Node initialized");


    return err;
}

void ble_mesh_set_rgb_conf(rgb_conf_t *conf){
    rgb_conf = conf;
}

esp_err_t ble_mesh_set_rgb(uint16_t addr, rgb_t color, bool ack){
    esp_err_t err;
    esp_ble_mesh_client_common_param_t common = {0};
    esp_ble_mesh_rgb_set_t rgb_set = {0};

    // if(ack){
    //     common.opcode = BLE_MESH_MODEL_OP_RGB_SET;
    // } else{
    //     common.opcode = BLE_MESH_MODEL_OP_RGB_SET_UNACK;
    // }
    common.opcode = BLE_MESH_MODEL_OP_RGB_SET_UNACK;

    common.model = rgb_client.model;
    common.ctx.net_idx = store.net_idx;
    common.ctx.app_idx = store.app_idx;

    LOGGER_I(TAG, "Sending RGB set to 0x%04" PRIx16, addr);

    common.ctx.addr = addr;
    common.ctx.send_ttl = 3;
    common.ctx.send_rel = ack;
    common.msg_timeout = 0;     /* 0 indicates that timeout value from menuconfig will be used */
    common.msg_role = ROLE_NODE;

    rgb_set.op_en = false;
    // TID needs to be unique to this message
    // if it is not unique the message does not
    // have to propagate properly throughout the
    // network
    rgb_set.tid = store.tid++;

    rgb_set.rgb = color;

    err = ble_mesh_rgb_client_set_state(&common, &rgb_set);
    if(err != ESP_OK){
        LOGGER_E(TAG, "Send RGB Set Unack failed");
        return err;
    }

    // store the info because TID has changed
    mesh_example_info_store();
    
    return ESP_OK;
}

esp_err_t ble_mesh_get_rgb(uint16_t addr, rgb_t *color_out){
    esp_ble_mesh_client_common_param_t common = {0};

    common.opcode = BLE_MESH_MODEL_OP_RGB_GET;

    common.model = rgb_client.model;
    common.ctx.net_idx = store.net_idx;
    common.ctx.app_idx = store.app_idx;

    LOGGER_I(TAG, "Sending RGB get to 0x%04" PRIx16, addr);

    common.ctx.addr = addr;
    common.ctx.send_ttl = 3;
    common.ctx.send_rel = false;
    common.msg_timeout = 0;     /* 0 indicates that timeout value from menuconfig will be used */
    common.msg_role = ROLE_NODE;

    esp_err_t err = ble_mesh_rgb_client_get_state(&common, color_out);
    if(err != ESP_OK){
        LOGGER_E(TAG, "RGB Get failed");
        return err;
    }
    
    return ESP_OK;
}

esp_err_t ble_mesh_set_loc_local(uint16_t addr, const location_local_t *loc_local){
    esp_ble_mesh_client_common_param_t common = {0};
    esp_ble_mesh_generic_client_set_state_t set_state = {0};

    common.opcode = ESP_BLE_MESH_MODEL_OP_GEN_LOC_LOCAL_SET_UNACK;

    common.model = location_client.model;
    common.ctx.net_idx = store.net_idx;
    common.ctx.app_idx = store.app_idx;

    LOGGER_I(TAG, "Sending Loc Local set to 0x%04" PRIx16, addr);

    common.ctx.addr = addr;
    common.ctx.send_ttl = 3;
    common.ctx.send_rel = false;
    common.msg_timeout = 0;     /* 0 indicates that timeout value from menuconfig will be used */
    common.msg_role = ROLE_NODE;

    set_state.loc_local_set.local_north    = loc_local->local_north;
    set_state.loc_local_set.local_east     = loc_local->local_east;
    set_state.loc_local_set.local_altitude = loc_local->local_altitude;
    set_state.loc_local_set.floor_number   = loc_local->floor_number;
    set_state.loc_local_set.uncertainty    = loc_local->uncertainty;

    return esp_ble_mesh_generic_client_set_state(&common, &set_state);
}

esp_err_t ble_mesh_get_loc_local(uint16_t addr, location_local_t *result){
    EventBits_t bits;
    esp_ble_mesh_client_common_param_t common = {0};
    esp_ble_mesh_generic_client_get_state_t get_state = {0};

    common.opcode = ESP_BLE_MESH_MODEL_OP_GEN_LOC_LOCAL_GET;

    common.model = location_client.model;
    common.ctx.net_idx = store.net_idx;
    common.ctx.app_idx = store.app_idx;

    LOGGER_I(TAG, "Sending Loc Local get to 0x%04" PRIx16, addr);

    common.ctx.addr = addr;
    common.ctx.send_ttl = 3;
    common.ctx.send_rel = true;
    common.msg_timeout = 0;     /* 0 indicates that timeout value from menuconfig will be used */
    common.msg_role = ROLE_NODE;

    esp_ble_mesh_generic_client_get_state(&common, &get_state);

    while(true){
        bits = xEventGroupWaitBits(s_location_event_group, EVENT_GET_SUCCESS_BIT | EVENT_GET_FAIL_BIT,
                                            pdTRUE, pdFALSE, portMAX_DELAY);
        if(s_location_event_data.addr != addr){
            LOGGER_W(TAG, "Get Location local woke up to different address! Expected:0x%04" PRIx16 " Result for: 0x%04" PRIx16, addr, s_location_event_data.addr);
            continue;
        }
        
        if(bits & EVENT_GET_SUCCESS_BIT){
            (*result) = s_location_event_data.location;
            return ESP_OK;
        } else{
            return ESP_FAIL;
        }
    }
}

esp_err_t ble_mesh_set_onoff(uint16_t addr, bool onoff){
    esp_ble_mesh_client_common_param_t common = {0};
    esp_ble_mesh_generic_client_set_state_t set_state = {0};

    common.opcode = ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_SET_UNACK;

    common.model = onoff_client.model;
    common.ctx.net_idx = store.net_idx;
    common.ctx.app_idx = store.app_idx;

    LOGGER_I(TAG, "Sending OnOff set to 0x%04" PRIx16, addr);

    common.ctx.addr = addr;
    common.ctx.send_ttl = 3;
    common.ctx.send_rel = false;
    common.msg_timeout = 0;     /* 0 indicates that timeout value from menuconfig will be used */
    common.msg_role = ROLE_NODE;

    set_state.onoff_set.onoff = onoff;
    set_state.onoff_set.tid = store.tid++;

    esp_err_t err = esp_ble_mesh_generic_client_set_state(&common, &set_state);
    if(err != ESP_OK){
        LOGGER_E(TAG, "Could not send OnOff SET message! Err: %d", err);
        return ESP_FAIL;
    }

    mesh_example_info_store();

    return err;
}

esp_err_t ble_mesh_get_onoff(uint16_t addr, bool *onoff_out){
    EventBits_t bits;
    esp_ble_mesh_client_common_param_t common = {0};
    esp_ble_mesh_generic_client_get_state_t get_state = {0};

    common.opcode = ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_GET;

    common.model = onoff_client.model;
    common.ctx.net_idx = store.net_idx;
    common.ctx.app_idx = store.app_idx;

    LOGGER_I(TAG, "Sending OnOff get to 0x%04" PRIx16, addr);

    common.ctx.addr = addr;
    common.ctx.send_ttl = 3;
    common.ctx.send_rel = false;
    common.msg_timeout = 0;     /* 0 indicates that timeout value from menuconfig will be used */
    common.msg_role = ROLE_NODE;

    esp_ble_mesh_generic_client_get_state(&common, &get_state);

    while(true){
        bits = xEventGroupWaitBits(s_onoff_event_group, EVENT_GET_SUCCESS_BIT | EVENT_GET_FAIL_BIT,
                                            pdTRUE, pdFALSE, portMAX_DELAY);
        if(s_onoff_event_data.addr != addr){
            LOGGER_W(TAG, "Get OnOff woke up to different address! Expected:0x%04" PRIx16 " Result for: 0x%04" PRIx16, addr, s_onoff_event_data.addr);
            continue;
        }
        
        if(bits & EVENT_GET_SUCCESS_BIT){
            (*onoff_out) = s_onoff_event_data.onoff;
            return ESP_OK;
        } else{
            return ESP_FAIL;
        }
    }
}

esp_err_t ble_mesh_set_level(uint16_t addr, int16_t level){
    esp_ble_mesh_client_common_param_t common = {0};
    esp_ble_mesh_generic_client_set_state_t set_state = {0};

    common.opcode = ESP_BLE_MESH_MODEL_OP_GEN_LEVEL_SET_UNACK;

    common.model = level_client.model;
    common.ctx.net_idx = store.net_idx;
    common.ctx.app_idx = store.app_idx;

    LOGGER_I(TAG, "Sending Level set to 0x%04" PRIx16 " with value %" PRId16, addr, level);

    common.ctx.addr = addr;
    common.ctx.send_ttl = 3;
    common.ctx.send_rel = true;
    common.msg_timeout = 0;     /* 0 indicates that timeout value from menuconfig will be used */
    common.msg_role = ROLE_NODE;

    set_state.level_set.op_en = false;
    set_state.level_set.level = level;
    set_state.level_set.tid = store.tid++;
    set_state.level_set.delay = 0;

    esp_err_t err = esp_ble_mesh_generic_client_set_state(&common, &set_state);
    if(err != ESP_OK){
        LOGGER_E(TAG, "Could not send OnOff SET message! Err: %d", err);
        return ESP_FAIL;
    }

    mesh_example_info_store();

    return err;
}

esp_err_t ble_mesh_get_level(uint16_t addr, int16_t *level_out){
    EventBits_t bits;
    esp_ble_mesh_client_common_param_t common = {0};
    esp_ble_mesh_generic_client_get_state_t get_state = {0};

    common.opcode = ESP_BLE_MESH_MODEL_OP_GEN_LEVEL_GET;

    common.model = level_client.model;
    common.ctx.net_idx = store.net_idx;
    common.ctx.app_idx = store.app_idx;

    LOGGER_I(TAG, "Sending Level get to 0x%04" PRIx16, addr);

    common.ctx.addr = addr;
    common.ctx.send_ttl = 3;
    common.ctx.send_rel = false;
    common.msg_timeout = 0;     /* 0 indicates that timeout value from menuconfig will be used */
    common.msg_role = ROLE_NODE;

    esp_ble_mesh_generic_client_get_state(&common, &get_state);

    while(true){
        bits = xEventGroupWaitBits(s_level_event_group, EVENT_GET_SUCCESS_BIT | EVENT_GET_FAIL_BIT,
                                            pdTRUE, pdFALSE, portMAX_DELAY);
        if(s_level_event_data.addr != addr){
            LOGGER_W(TAG, "Get OnOff woke up to different address! Expected:0x%04" PRIx16 " Result for: 0x%04" PRIx16, addr, s_level_event_data.addr);
            continue;
        }
        
        if(bits & EVENT_GET_SUCCESS_BIT){
            (*level_out) = s_level_event_data.level;
            return ESP_OK;
        } else{
            return ESP_FAIL;
        }
    }
}

esp_err_t ble_mesh_get_addresses(uint16_t *primary_addr, uint8_t *addresses){
    (*primary_addr) = esp_ble_mesh_get_primary_element_address();
    (*addresses) = esp_ble_mesh_get_element_count();
    
    if(primary_addr == ESP_BLE_MESH_ADDR_UNASSIGNED){
        return ESP_FAIL;
    }
    
    return ESP_OK;
}


// before calling this function:
//  - init board
//  - initialize logger library
//  - init NVS flash
esp_err_t ble_mesh_init()
{
    esp_err_t err;

    LOGGER_I(TAG, "Initializing BLE-mesh...");

    LOGGER_V(TAG, "init bluetooth");
    err = bluetooth_init();
    if (err) {
        LOGGER_E(TAG, "esp32_bluetooth_init failed (err %d)", err);
        return err;
    }

    /* Open nvs namespace for storing/restoring mesh example info */
    LOGGER_V(TAG, "nvs open");
    err = ble_mesh_nvs_open(&NVS_HANDLE);
    if (err) {
        LOGGER_E(TAG, "Could not open NVS for BLE-mesh");
        return err;
    }

    // open nvs namespace that stores web config values
    err = nvs_open("config", NVS_READONLY, &config_nvs);
    if (err) {
        ESP_LOGI(TAG, "Could not open NVS for config");
    }

    // populate uuid
    ble_mesh_get_dev_uuid(dev_uuid);

    s_location_event_group = xEventGroupCreate();
    s_onoff_event_group    = xEventGroupCreate();
    s_level_event_group    = xEventGroupCreate();

    /* Initialize the Bluetooth Mesh Subsystem */
    LOGGER_V(TAG, "mesh init");
    err = mesh_init();
    if (err) {
        LOGGER_E(TAG, "Bluetooth mesh init failed (err %d)", err);
        return err;
    }
    return ESP_OK;
}
