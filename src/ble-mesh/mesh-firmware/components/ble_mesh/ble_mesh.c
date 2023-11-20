/* main.c - Application main entry point */

/*
 * SPDX-FileCopyrightText: 2017 Intel Corporation
 * SPDX-FileContributor: 2018-2021 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */


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

#include "color.h"
#include "hsl.h"

#include "logger.h"

// tag for logging
static const char* TAG = "EXAMPLE-STA";

static rgb_conf_t *rgb_conf;

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

/*
 * Definitions of BLE-mesh Elements
 */

// definition of the main element
// it is the main one because it contains config server
static esp_ble_mesh_model_t root_models[] = {
    ESP_BLE_MESH_MODEL_CFG_SRV(&config_server),
    ESP_BLE_MESH_MODEL_GEN_ONOFF_SRV(&onoff_pub_0, &onoff_server_0),
    ESP_BLE_MESH_MODEL_HEALTH_SRV(&health_server, &health_pub),
    BLE_MESH_MODEL_RGB_SRV(&rgb_srv_pub, &rgb_srv),
    BLE_MESH_MODEL_RGB_SETUP_SRV(&rgb_setup_pub, &rgb_setup_srv),
    BLE_MESH_MODEL_RGB_CLI(&rgb_cli_pub, &rgb_client),
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
            store.net_idx, store.app_idx, store.onoff, store.tid);
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

// helper function that is called when the OnOff state is changed
static void example_change_led_state(esp_ble_mesh_model_t *model,
                                     esp_ble_mesh_msg_ctx_t *ctx, uint8_t onoff)
{
    uint16_t primary_addr = esp_ble_mesh_get_primary_element_address();
    uint8_t elem_count = esp_ble_mesh_get_element_count();
    uint8_t i;

    if (ESP_BLE_MESH_ADDR_IS_UNICAST(ctx->recv_dst)) {
        if(onoff == 0){
            board_set_rgb(&internal_rgb_conf, WHITE);
        } else{
            board_set_rgb(&internal_rgb_conf, NONE);
        }
    } else if (ESP_BLE_MESH_ADDR_IS_GROUP(ctx->recv_dst)) {
        if (esp_ble_mesh_is_model_subscribed_to_group(model, ctx->recv_dst)) {
            if(onoff == 0){
                board_set_rgb(&internal_rgb_conf, NONE);
            } else{
                board_set_rgb(&internal_rgb_conf, WHITE);
            }
        }
    } else if (ctx->recv_dst == 0xFFFF) {
        if(onoff == 0){
            board_set_rgb(&internal_rgb_conf, NONE);
        } else{
            board_set_rgb(&internal_rgb_conf, WHITE);
        }
    }
}

// callback for OnOff model (called from generic server callback)
static void example_handle_gen_onoff_msg(esp_ble_mesh_model_t *model,
                                         esp_ble_mesh_msg_ctx_t *ctx,
                                         esp_ble_mesh_server_recv_gen_onoff_set_t *set)
{
    esp_ble_mesh_gen_onoff_srv_t *srv = model->user_data;

    switch (ctx->recv_op) {
    case ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_GET:
        esp_ble_mesh_server_model_send_msg(model, ctx,
            ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_STATUS, sizeof(srv->state.onoff), &srv->state.onoff);
        break;
    case ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_SET:
    case ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_SET_UNACK:
        if (set->op_en == false) {
            srv->state.onoff = set->onoff;
        } else {
            /* TODO: Delay and state transition */
            srv->state.onoff = set->onoff;
        }
        if (ctx->recv_op == ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_SET) {
            esp_ble_mesh_server_model_send_msg(model, ctx,
                ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_STATUS, sizeof(srv->state.onoff), &srv->state.onoff);
        }
        esp_ble_mesh_model_publish(model, ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_STATUS,
            sizeof(srv->state.onoff), &srv->state.onoff, ROLE_NODE);
        example_change_led_state(model, ctx, srv->state.onoff);
        break;
    default:
        break;
    }
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
        LOGGER_I(TAG, "ESP_BLE_MESH_GENERIC_SERVER_STATE_CHANGE_EVT");
        if (param->ctx.recv_op == ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_SET ||
            param->ctx.recv_op == ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_SET_UNACK) {
            LOGGER_I(TAG, "onoff 0x%02x", param->value.state_change.onoff_set.onoff);
            example_change_led_state(param->model, &param->ctx, param->value.state_change.onoff_set.onoff);
        }
        break;
    case ESP_BLE_MESH_GENERIC_SERVER_RECV_GET_MSG_EVT:
        LOGGER_I(TAG, "ESP_BLE_MESH_GENERIC_SERVER_RECV_GET_MSG_EVT");
        if (param->ctx.recv_op == ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_GET) {
            srv = param->model->user_data;
            LOGGER_I(TAG, "onoff 0x%02x", srv->state.onoff);
            example_handle_gen_onoff_msg(param->model, &param->ctx, NULL);
        }
        break;
    case ESP_BLE_MESH_GENERIC_SERVER_RECV_SET_MSG_EVT:
        LOGGER_I(TAG, "ESP_BLE_MESH_GENERIC_SERVER_RECV_SET_MSG_EVT");
        if (param->ctx.recv_op == ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_SET ||
            param->ctx.recv_op == ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_SET_UNACK) {
            LOGGER_I(TAG, "onoff 0x%02x, tid 0x%02x", param->value.set.onoff.onoff, param->value.set.onoff.tid);
            if (param->value.set.onoff.op_en) {
                LOGGER_I(TAG, "trans_time 0x%02x, delay 0x%02x",
                    param->value.set.onoff.trans_time, param->value.set.onoff.delay);
            }
            example_handle_gen_onoff_msg(param->model, &param->ctx, &param->value.set.onoff);
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

    LOGGER_I(TAG, "Sending RGB set to %d", addr);

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

rgb_response_t ble_mesh_get_rgb(uint16_t addr){
    esp_ble_mesh_client_common_param_t common = {0};

    common.opcode = BLE_MESH_MODEL_OP_RGB_GET;

    common.model = rgb_client.model;
    common.ctx.net_idx = store.net_idx;
    common.ctx.app_idx = store.app_idx;

    LOGGER_I(TAG, "Sending RGB set to %d", addr);

    common.ctx.addr = addr;
    common.ctx.send_ttl = 3;
    common.ctx.send_rel = false;
    common.msg_timeout = 0;     /* 0 indicates that timeout value from menuconfig will be used */
    common.msg_role = ROLE_NODE;

    rgb_response_t response = ble_mesh_rgb_client_get_state(&common);
    if(!response.valid){
        LOGGER_E(TAG, "RGB Get failed");
        return response;
    }

    // store the info because TID has changed
    mesh_example_info_store();
    
    return response;
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

    /* Initialize the Bluetooth Mesh Subsystem */
    LOGGER_V(TAG, "mesh init");
    err = mesh_init();
    if (err) {
        LOGGER_E(TAG, "Bluetooth mesh init failed (err %d)", err);
        return err;
    }
    return ESP_OK;
}
