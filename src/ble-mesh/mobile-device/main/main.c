/* main.c - Application main entry point */

/*
 * SPDX-FileCopyrightText: 2017 Intel Corporation
 * SPDX-FileContributor: 2018-2021 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include "esp_log.h"
#include "nvs_flash.h"

#include "esp_ble_mesh_common_api.h"
#include "esp_ble_mesh_provisioning_api.h"
#include "esp_ble_mesh_networking_api.h"
#include "esp_ble_mesh_config_model_api.h"
#include "esp_ble_mesh_generic_model_api.h"
#include "esp_ble_mesh_health_model_api.h"
#include "esp_ble_mesh_sensor_model_api.h"

#include "board.h"
#include "ble_mesh_example_init.h"
#include "ble_mesh_example_nvs.h"

#define TAG "EXAMPLE"

#define CID_ESP 0x02E5

static uint8_t dev_uuid[16] = { 0xdd, 0xdd };

/* Sensor Property ID */
#define SENSOR_PROPERTY_ID_0        0x0056  /* Present Indoor Ambient Temperature */
#define SENSOR_PROPERTY_ID_1        0x005B  /* Present Outdoor Ambient Temperature */

/* The characteristic of the two device properties is "Temperature 8", which is
 * used to represent a measure of temperature with a unit of 0.5 degree Celsius.
 * Minimum value: -64.0, maximum value: 63.5.
 * A value of 0xFF represents 'value is not known'.
 */
static int8_t indoor_temp = 40;     /* Indoor temperature is 20 Degrees Celsius */
static int8_t outdoor_temp = 60;    /* Outdoor temperature is 30 Degrees Celsius */

#define SENSOR_POSITIVE_TOLERANCE   ESP_BLE_MESH_SENSOR_UNSPECIFIED_POS_TOLERANCE
#define SENSOR_NEGATIVE_TOLERANCE   ESP_BLE_MESH_SENSOR_UNSPECIFIED_NEG_TOLERANCE
#define SENSOR_SAMPLE_FUNCTION      ESP_BLE_MESH_SAMPLE_FUNC_UNSPECIFIED
#define SENSOR_MEASURE_PERIOD       ESP_BLE_MESH_SENSOR_NOT_APPL_MEASURE_PERIOD
#define SENSOR_UPDATE_INTERVAL      ESP_BLE_MESH_SENSOR_NOT_APPL_UPDATE_INTERVAL

static struct example_info_store {
    uint16_t net_idx;   /* NetKey Index */
    uint16_t app_idx;   /* AppKey Index */
    uint8_t  onoff;     /* Remote OnOff */
    uint8_t  tid;       /* Message TID */
} __attribute__((packed)) store = {
    .net_idx = ESP_BLE_MESH_KEY_UNUSED,
    .app_idx = ESP_BLE_MESH_KEY_UNUSED,
    .onoff = LED_OFF,
    .tid = 0x0,
};

static nvs_handle_t NVS_HANDLE;
static const char * NVS_KEY = "onoff_client";

static esp_ble_mesh_client_t onoff_client;

static esp_ble_mesh_cfg_srv_t config_server = {
    .relay = ESP_BLE_MESH_RELAY_DISABLED,
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

NET_BUF_SIMPLE_DEFINE_STATIC(sensor_data_0, 1);
NET_BUF_SIMPLE_DEFINE_STATIC(sensor_data_1, 1);

static esp_ble_mesh_sensor_state_t sensor_states[2] = {
    /* Mesh Model Spec:
     * Multiple instances of the Sensor states may be present within the same model,
     * provided that each instance has a unique value of the Sensor Property ID to
     * allow the instances to be differentiated. Such sensors are known as multisensors.
     * In this example, two instances of the Sensor states within the same model are
     * provided.
     */
    [0] = {
        /* Mesh Model Spec:
         * Sensor Property ID is a 2-octet value referencing a device property
         * that describes the meaning and format of data reported by a sensor.
         * 0x0000 is prohibited.
         */
        .sensor_property_id = SENSOR_PROPERTY_ID_0,
        /* Mesh Model Spec:
         * Sensor Descriptor state represents the attributes describing the sensor
         * data. This state does not change throughout the lifetime of an element.
         */
        .descriptor.positive_tolerance = SENSOR_POSITIVE_TOLERANCE,
        .descriptor.negative_tolerance = SENSOR_NEGATIVE_TOLERANCE,
        .descriptor.sampling_function = SENSOR_SAMPLE_FUNCTION,
        .descriptor.measure_period = SENSOR_MEASURE_PERIOD,
        .descriptor.update_interval = SENSOR_UPDATE_INTERVAL,
        .sensor_data.format = ESP_BLE_MESH_SENSOR_DATA_FORMAT_A,
        .sensor_data.length = 0, /* 0 represents the length is 1 */
        .sensor_data.raw_value = &sensor_data_0,
    },
    [1] = {
        .sensor_property_id = SENSOR_PROPERTY_ID_1,
        .descriptor.positive_tolerance = SENSOR_POSITIVE_TOLERANCE,
        .descriptor.negative_tolerance = SENSOR_NEGATIVE_TOLERANCE,
        .descriptor.sampling_function = SENSOR_SAMPLE_FUNCTION,
        .descriptor.measure_period = SENSOR_MEASURE_PERIOD,
        .descriptor.update_interval = SENSOR_UPDATE_INTERVAL,
        .sensor_data.format = ESP_BLE_MESH_SENSOR_DATA_FORMAT_A,
        .sensor_data.length = 0, /* 0 represents the length is 1 */
        .sensor_data.raw_value = &sensor_data_1,
    },
};

/* 20 octets is large enough to hold two Sensor Descriptor state values. */
ESP_BLE_MESH_MODEL_PUB_DEFINE(sensor_pub, 20, ROLE_NODE);
static esp_ble_mesh_sensor_srv_t sensor_server = {
    .rsp_ctrl.get_auto_rsp = ESP_BLE_MESH_SERVER_RSP_BY_APP,
    .rsp_ctrl.set_auto_rsp = ESP_BLE_MESH_SERVER_RSP_BY_APP,
    .state_count = ARRAY_SIZE(sensor_states),
    .states = sensor_states,
};

ESP_BLE_MESH_MODEL_PUB_DEFINE(sensor_setup_pub, 20, ROLE_NODE);
static esp_ble_mesh_sensor_setup_srv_t sensor_setup_server = {
    .rsp_ctrl.get_auto_rsp = ESP_BLE_MESH_SERVER_RSP_BY_APP,
    .rsp_ctrl.set_auto_rsp = ESP_BLE_MESH_SERVER_RSP_BY_APP,
    .state_count = ARRAY_SIZE(sensor_states),
    .states = sensor_states,
};

ESP_BLE_MESH_MODEL_PUB_DEFINE(onoff_cli_pub, 2 + 1, ROLE_NODE);

uint8_t test_ids[1] = {0x00};

/** ESP BLE Mesh Health Server Model Context */
ESP_BLE_MESH_MODEL_PUB_DEFINE(health_pub, 2 + 11, ROLE_NODE);
static esp_ble_mesh_health_srv_t health_server = {
    .health_test.id_count = 1,
    .health_test.test_ids = test_ids,
};

static esp_ble_mesh_model_t root_models[] = {
    ESP_BLE_MESH_MODEL_CFG_SRV(&config_server),
    ESP_BLE_MESH_MODEL_GEN_ONOFF_CLI(&onoff_cli_pub, &onoff_client),
    ESP_BLE_MESH_MODEL_HEALTH_SRV(&health_server, &health_pub),
    ESP_BLE_MESH_MODEL_SENSOR_SRV(&sensor_pub, &sensor_server),
    ESP_BLE_MESH_MODEL_SENSOR_SETUP_SRV(&sensor_setup_pub, &sensor_setup_server),
};

static esp_ble_mesh_elem_t elements[] = {
    ESP_BLE_MESH_ELEMENT(0, root_models, ESP_BLE_MESH_MODEL_NONE),
};

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

static void mesh_example_info_store(void)
{
    ble_mesh_nvs_store(NVS_HANDLE, NVS_KEY, &store, sizeof(store));
}

static void mesh_example_info_restore(void)
{
    esp_err_t err = ESP_OK;
    bool exist = false;

    err = ble_mesh_nvs_restore(NVS_HANDLE, NVS_KEY, &store, sizeof(store), &exist);
    if (err != ESP_OK) {
        return;
    }

    if (exist) {
        ESP_LOGI(TAG, "Restore, net_idx 0x%04x, app_idx 0x%04x, onoff %u, tid 0x%02x",
            store.net_idx, store.app_idx, store.onoff, store.tid);
    }
}

static void prov_complete(uint16_t net_idx, uint16_t addr, uint8_t flags, uint32_t iv_index)
{
    ESP_LOGI(TAG, "net_idx: 0x%04x, addr: 0x%04x", net_idx, addr);
    ESP_LOGI(TAG, "flags: 0x%02x, iv_index: 0x%08" PRIx32, flags, iv_index);
    board_led_operation(LED_GREEN, LED_OFF);
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

    /* Initialize the indoor and outdoor temperatures for each sensor.  */
    net_buf_simple_add_u8(&sensor_data_0, indoor_temp);
    net_buf_simple_add_u8(&sensor_data_1, outdoor_temp);
}

static void example_ble_mesh_provisioning_cb(esp_ble_mesh_prov_cb_event_t event,
                                             esp_ble_mesh_prov_cb_param_t *param)
{
    switch (event) {
    case ESP_BLE_MESH_PROV_REGISTER_COMP_EVT:
        ESP_LOGI(TAG, "ESP_BLE_MESH_PROV_REGISTER_COMP_EVT, err_code %d", param->prov_register_comp.err_code);
        mesh_example_info_restore(); /* Restore proper mesh example info */
        break;
    case ESP_BLE_MESH_NODE_PROV_ENABLE_COMP_EVT:
        ESP_LOGI(TAG, "ESP_BLE_MESH_NODE_PROV_ENABLE_COMP_EVT, err_code %d", param->node_prov_enable_comp.err_code);
        break;
    case ESP_BLE_MESH_NODE_PROV_LINK_OPEN_EVT:
        ESP_LOGI(TAG, "ESP_BLE_MESH_NODE_PROV_LINK_OPEN_EVT, bearer %s",
            param->node_prov_link_open.bearer == ESP_BLE_MESH_PROV_ADV ? "PB-ADV" : "PB-GATT");
        break;
    case ESP_BLE_MESH_NODE_PROV_LINK_CLOSE_EVT:
        ESP_LOGI(TAG, "ESP_BLE_MESH_NODE_PROV_LINK_CLOSE_EVT, bearer %s",
            param->node_prov_link_close.bearer == ESP_BLE_MESH_PROV_ADV ? "PB-ADV" : "PB-GATT");
        break;
    case ESP_BLE_MESH_NODE_PROV_COMPLETE_EVT:
        ESP_LOGI(TAG, "ESP_BLE_MESH_NODE_PROV_COMPLETE_EVT");
        prov_complete(param->node_prov_complete.net_idx, param->node_prov_complete.addr,
            param->node_prov_complete.flags, param->node_prov_complete.iv_index);
        break;
    case ESP_BLE_MESH_NODE_PROV_RESET_EVT:
        break;
    case ESP_BLE_MESH_NODE_SET_UNPROV_DEV_NAME_COMP_EVT:
        ESP_LOGI(TAG, "ESP_BLE_MESH_NODE_SET_UNPROV_DEV_NAME_COMP_EVT, err_code %d", param->node_set_unprov_dev_name_comp.err_code);
        break;
    default:
        break;
    }
}

void example_ble_mesh_send_gen_onoff_set(void)
{
    esp_ble_mesh_generic_client_set_state_t set = {0};
    esp_ble_mesh_client_common_param_t common = {0};
    esp_err_t err = ESP_OK;

    common.opcode = ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_SET_UNACK;
    common.model = onoff_client.model;
    common.ctx.net_idx = store.net_idx;
    common.ctx.app_idx = store.app_idx;
    common.ctx.addr = 0xFFFF;   /* to all nodes */
    common.ctx.send_ttl = 3;
    common.ctx.send_rel = false;
    common.msg_timeout = 0;     /* 0 indicates that timeout value from menuconfig will be used */
    common.msg_role = ROLE_NODE;

    set.onoff_set.op_en = false;
    set.onoff_set.onoff = store.onoff;
    set.onoff_set.tid = store.tid++;

    err = esp_ble_mesh_generic_client_set_state(&common, &set);
    if (err) {
        ESP_LOGE(TAG, "Send Generic OnOff Set Unack failed");
        return;
    }

    store.onoff = !store.onoff;
    mesh_example_info_store(); /* Store proper mesh example info */
}

static void example_ble_mesh_generic_client_cb(esp_ble_mesh_generic_client_cb_event_t event,
                                               esp_ble_mesh_generic_client_cb_param_t *param)
{
    ESP_LOGI(TAG, "Generic client, event %u, error code %d, opcode is 0x%04" PRIx32,
        event, param->error_code, param->params->opcode);

    switch (event) {
    case ESP_BLE_MESH_GENERIC_CLIENT_GET_STATE_EVT:
        ESP_LOGI(TAG, "ESP_BLE_MESH_GENERIC_CLIENT_GET_STATE_EVT");
        if (param->params->opcode == ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_GET) {
            ESP_LOGI(TAG, "ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_GET, onoff %d", param->status_cb.onoff_status.present_onoff);
        }
        break;
    case ESP_BLE_MESH_GENERIC_CLIENT_SET_STATE_EVT:
        ESP_LOGI(TAG, "ESP_BLE_MESH_GENERIC_CLIENT_SET_STATE_EVT");
        if (param->params->opcode == ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_SET) {
            ESP_LOGI(TAG, "ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_SET, onoff %d", param->status_cb.onoff_status.present_onoff);
        }
        break;
    case ESP_BLE_MESH_GENERIC_CLIENT_PUBLISH_EVT:
        ESP_LOGI(TAG, "ESP_BLE_MESH_GENERIC_CLIENT_PUBLISH_EVT");
        break;
    case ESP_BLE_MESH_GENERIC_CLIENT_TIMEOUT_EVT:
        ESP_LOGI(TAG, "ESP_BLE_MESH_GENERIC_CLIENT_TIMEOUT_EVT");
        if (param->params->opcode == ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_SET) {
            /* If failed to get the response of Generic OnOff Set, resend Generic OnOff Set  */
            example_ble_mesh_send_gen_onoff_set();
        }
        break;
    default:
        break;
    }
}

static void example_ble_mesh_config_server_cb(esp_ble_mesh_cfg_server_cb_event_t event,
                                              esp_ble_mesh_cfg_server_cb_param_t *param)
{
    if (event == ESP_BLE_MESH_CFG_SERVER_STATE_CHANGE_EVT) {
        switch (param->ctx.recv_op) {
        case ESP_BLE_MESH_MODEL_OP_APP_KEY_ADD:
            ESP_LOGI(TAG, "ESP_BLE_MESH_MODEL_OP_APP_KEY_ADD");
            ESP_LOGI(TAG, "net_idx 0x%04x, app_idx 0x%04x",
                param->value.state_change.appkey_add.net_idx,
                param->value.state_change.appkey_add.app_idx);
            ESP_LOG_BUFFER_HEX("AppKey", param->value.state_change.appkey_add.app_key, 16);
            break;
        case ESP_BLE_MESH_MODEL_OP_MODEL_APP_BIND:
            ESP_LOGI(TAG, "ESP_BLE_MESH_MODEL_OP_MODEL_APP_BIND");
            ESP_LOGI(TAG, "elem_addr 0x%04x, app_idx 0x%04x, cid 0x%04x, mod_id 0x%04x",
                param->value.state_change.mod_app_bind.element_addr,
                param->value.state_change.mod_app_bind.app_idx,
                param->value.state_change.mod_app_bind.company_id,
                param->value.state_change.mod_app_bind.model_id);
            if (param->value.state_change.mod_app_bind.company_id == 0xFFFF &&
                param->value.state_change.mod_app_bind.model_id == ESP_BLE_MESH_MODEL_ID_GEN_ONOFF_CLI) {
                store.app_idx = param->value.state_change.mod_app_bind.app_idx;
                mesh_example_info_store(); /* Store proper mesh example info */
            }
            break;
        default:
            break;
        }
    }
}

static void example_ble_mesh_health_server_cb(esp_ble_mesh_health_server_cb_event_t event,
                                                 esp_ble_mesh_health_server_cb_param_t *param)
{
    if(event == ESP_BLE_MESH_HEALTH_SERVER_ATTENTION_ON_EVT){
        board_start_blinking();
    }
    if(event == ESP_BLE_MESH_HEALTH_SERVER_ATTENTION_OFF_EVT){
        board_stop_blinking();
    }
}

struct example_sensor_descriptor {
    uint16_t sensor_prop_id;
    uint32_t pos_tolerance:12,
             neg_tolerance:12,
             sample_func:8;
    uint8_t  measure_period;
    uint8_t  update_interval;
} __attribute__((packed));

static void example_ble_mesh_send_sensor_descriptor_status(esp_ble_mesh_sensor_server_cb_param_t *param)
{
    struct example_sensor_descriptor descriptor = {0};
    uint8_t *status = NULL;
    uint16_t length = 0;
    esp_err_t err;
    int i;

    status = calloc(1, ARRAY_SIZE(sensor_states) * ESP_BLE_MESH_SENSOR_DESCRIPTOR_LEN);
    if (!status) {
        ESP_LOGE(TAG, "No memory for sensor descriptor status!");
        return;
    }

    if (param->value.get.sensor_descriptor.op_en == false) {
        /* Mesh Model Spec:
         * Upon receiving a Sensor Descriptor Get message with the Property ID field
         * omitted, the Sensor Server shall respond with a Sensor Descriptor Status
         * message containing the Sensor Descriptor states for all sensors within the
         * Sensor Server.
         */
        for (i = 0; i < ARRAY_SIZE(sensor_states); i++) {
            descriptor.sensor_prop_id = sensor_states[i].sensor_property_id;
            descriptor.pos_tolerance = sensor_states[i].descriptor.positive_tolerance;
            descriptor.neg_tolerance = sensor_states[i].descriptor.negative_tolerance;
            descriptor.sample_func = sensor_states[i].descriptor.sampling_function;
            descriptor.measure_period = sensor_states[i].descriptor.measure_period;
            descriptor.update_interval = sensor_states[i].descriptor.update_interval;
            memcpy(status + length, &descriptor, ESP_BLE_MESH_SENSOR_DESCRIPTOR_LEN);
            length += ESP_BLE_MESH_SENSOR_DESCRIPTOR_LEN;
        }
        goto send;
    }

    for (i = 0; i < ARRAY_SIZE(sensor_states); i++) {
        if (param->value.get.sensor_descriptor.property_id == sensor_states[i].sensor_property_id) {
            descriptor.sensor_prop_id = sensor_states[i].sensor_property_id;
            descriptor.pos_tolerance = sensor_states[i].descriptor.positive_tolerance;
            descriptor.neg_tolerance = sensor_states[i].descriptor.negative_tolerance;
            descriptor.sample_func = sensor_states[i].descriptor.sampling_function;
            descriptor.measure_period = sensor_states[i].descriptor.measure_period;
            descriptor.update_interval = sensor_states[i].descriptor.update_interval;
            memcpy(status, &descriptor, ESP_BLE_MESH_SENSOR_DESCRIPTOR_LEN);
            length = ESP_BLE_MESH_SENSOR_DESCRIPTOR_LEN;
            goto send;
        }
    }

    /* Mesh Model Spec:
     * When a Sensor Descriptor Get message that identifies a sensor descriptor
     * property that does not exist on the element, the Descriptor field shall
     * contain the requested Property ID value and the other fields of the Sensor
     * Descriptor state shall be omitted.
     */
    memcpy(status, &param->value.get.sensor_descriptor.property_id, ESP_BLE_MESH_SENSOR_PROPERTY_ID_LEN);
    length = ESP_BLE_MESH_SENSOR_PROPERTY_ID_LEN;

send:
    ESP_LOG_BUFFER_HEX("Sensor Descriptor", status, length);

    err = esp_ble_mesh_server_model_send_msg(param->model, &param->ctx,
            ESP_BLE_MESH_MODEL_OP_SENSOR_DESCRIPTOR_STATUS, length, status);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send Sensor Descriptor Status");
    }
    free(status);
}

static void example_ble_mesh_send_sensor_cadence_status(esp_ble_mesh_sensor_server_cb_param_t *param)
{
    esp_err_t err;

    /* Sensor Cadence state is not supported currently. */
    err = esp_ble_mesh_server_model_send_msg(param->model, &param->ctx,
            ESP_BLE_MESH_MODEL_OP_SENSOR_CADENCE_STATUS,
            ESP_BLE_MESH_SENSOR_PROPERTY_ID_LEN,
            (uint8_t *)&param->value.get.sensor_cadence.property_id);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send Sensor Cadence Status");
    }
}

static void example_ble_mesh_send_sensor_settings_status(esp_ble_mesh_sensor_server_cb_param_t *param)
{
    esp_err_t err;

    /* Sensor Setting state is not supported currently. */
    err = esp_ble_mesh_server_model_send_msg(param->model, &param->ctx,
            ESP_BLE_MESH_MODEL_OP_SENSOR_SETTINGS_STATUS,
            ESP_BLE_MESH_SENSOR_PROPERTY_ID_LEN,
            (uint8_t *)&param->value.get.sensor_settings.property_id);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send Sensor Settings Status");
    }
}

struct example_sensor_setting {
    uint16_t sensor_prop_id;
    uint16_t sensor_setting_prop_id;
} __attribute__((packed));

static void example_ble_mesh_send_sensor_setting_status(esp_ble_mesh_sensor_server_cb_param_t *param)
{
    struct example_sensor_setting setting = {0};
    esp_err_t err;

    /* Mesh Model Spec:
     * If the message is sent as a response to the Sensor Setting Get message or
     * a Sensor Setting Set message with an unknown Sensor Property ID field or
     * an unknown Sensor Setting Property ID field, the Sensor Setting Access
     * field and the Sensor Setting Raw field shall be omitted.
     */

    setting.sensor_prop_id = param->value.get.sensor_setting.property_id;
    setting.sensor_setting_prop_id = param->value.get.sensor_setting.setting_property_id;

    err = esp_ble_mesh_server_model_send_msg(param->model, &param->ctx,
            ESP_BLE_MESH_MODEL_OP_SENSOR_SETTING_STATUS,
            sizeof(setting), (uint8_t *)&setting);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send Sensor Setting Status");
    }
}

static uint16_t example_ble_mesh_get_sensor_data(esp_ble_mesh_sensor_state_t *state, uint8_t *data)
{
    uint8_t mpid_len = 0, data_len = 0;
    uint32_t mpid = 0;

    if (state == NULL || data == NULL) {
        ESP_LOGE(TAG, "%s, Invalid parameter", __func__);
        return 0;
    }

    if (state->sensor_data.length == ESP_BLE_MESH_SENSOR_DATA_ZERO_LEN) {
        /* For zero-length sensor data, the length is 0x7F, and the format is Format B. */
        mpid = ESP_BLE_MESH_SENSOR_DATA_FORMAT_B_MPID(state->sensor_data.length, state->sensor_property_id);
        mpid_len = ESP_BLE_MESH_SENSOR_DATA_FORMAT_B_MPID_LEN;
        data_len = 0;
    } else {
        if (state->sensor_data.format == ESP_BLE_MESH_SENSOR_DATA_FORMAT_A) {
            mpid = ESP_BLE_MESH_SENSOR_DATA_FORMAT_A_MPID(state->sensor_data.length, state->sensor_property_id);
            mpid_len = ESP_BLE_MESH_SENSOR_DATA_FORMAT_A_MPID_LEN;
        } else {
            mpid = ESP_BLE_MESH_SENSOR_DATA_FORMAT_B_MPID(state->sensor_data.length, state->sensor_property_id);
            mpid_len = ESP_BLE_MESH_SENSOR_DATA_FORMAT_B_MPID_LEN;
        }
        /* Use "state->sensor_data.length + 1" because the length of sensor data is zero-based. */
        data_len = state->sensor_data.length + 1;
    }

    memcpy(data, &mpid, mpid_len);
    memcpy(data + mpid_len, state->sensor_data.raw_value->data, data_len);

    return (mpid_len + data_len);
}

static void example_ble_mesh_send_sensor_status(esp_ble_mesh_sensor_server_cb_param_t *param)
{
    uint8_t *status = NULL;
    uint16_t buf_size = 0;
    uint16_t length = 0;
    uint32_t mpid = 0;
    esp_err_t err;
    int i;

    /**
     * Sensor Data state from Mesh Model Spec
     * |--------Field--------|-Size (octets)-|------------------------Notes-------------------------|
     * |----Property ID 1----|-------2-------|--ID of the 1st device property of the sensor---------|
     * |-----Raw Value 1-----|----variable---|--Raw Value field defined by the 1st device property--|
     * |----Property ID 2----|-------2-------|--ID of the 2nd device property of the sensor---------|
     * |-----Raw Value 2-----|----variable---|--Raw Value field defined by the 2nd device property--|
     * | ...... |
     * |----Property ID n----|-------2-------|--ID of the nth device property of the sensor---------|
     * |-----Raw Value n-----|----variable---|--Raw Value field defined by the nth device property--|
     */

    // compute size of the msg that will be send
    for (i = 0; i < ARRAY_SIZE(sensor_states); i++) {
        esp_ble_mesh_sensor_state_t *state = &sensor_states[i];
        if (state->sensor_data.length == ESP_BLE_MESH_SENSOR_DATA_ZERO_LEN) {
            buf_size += ESP_BLE_MESH_SENSOR_DATA_FORMAT_B_MPID_LEN;
        } else {
            /* Use "state->sensor_data.length + 1" because the length of sensor data is zero-based. */
            if (state->sensor_data.format == ESP_BLE_MESH_SENSOR_DATA_FORMAT_A) {
                buf_size += ESP_BLE_MESH_SENSOR_DATA_FORMAT_A_MPID_LEN + state->sensor_data.length + 1;
            } else {
                buf_size += ESP_BLE_MESH_SENSOR_DATA_FORMAT_B_MPID_LEN + state->sensor_data.length + 1;
            }
        }
    }

    // allocate space for the msg
    status = calloc(1, buf_size);
    if (!status) {
        ESP_LOGE(TAG, "No memory for sensor status!");
        return;
    }

    // detect what should be sent from the request
    // can send all sensors, just one sensor or nothing

    if (param->value.get.sensor_data.op_en == false) {
        /* Mesh Model Spec:
         * If the message is sent as a response to the Sensor Get message, and if the
         * Property ID field of the incoming message is omitted, the Marshalled Sensor
         * Data field shall contain data for all device properties within a sensor.
         */
        for (i = 0; i < ARRAY_SIZE(sensor_states); i++) {
            length += example_ble_mesh_get_sensor_data(&sensor_states[i], status + length);
        }
        goto send;
    }

    /* Mesh Model Spec:
     * Otherwise, the Marshalled Sensor Data field shall contain data for the requested
     * device property only.
     */
    for (i = 0; i < ARRAY_SIZE(sensor_states); i++) {
        if (param->value.get.sensor_data.property_id == sensor_states[i].sensor_property_id) {
            length = example_ble_mesh_get_sensor_data(&sensor_states[i], status);
            goto send;
        }
    }

    /* Mesh Model Spec:
     * Or the Length shall represent the value of zero and the Raw Value field shall
     * contain only the Property ID if the requested device property is not recognized
     * by the Sensor Server.
     */
    mpid = ESP_BLE_MESH_SENSOR_DATA_FORMAT_B_MPID(ESP_BLE_MESH_SENSOR_DATA_ZERO_LEN,
            param->value.get.sensor_data.property_id);
    memcpy(status, &mpid, ESP_BLE_MESH_SENSOR_DATA_FORMAT_B_MPID_LEN);
    length = ESP_BLE_MESH_SENSOR_DATA_FORMAT_B_MPID_LEN;

send:
    // send the msg
    
    ESP_LOG_BUFFER_HEX("Sensor Data", status, length);

    err = esp_ble_mesh_server_model_send_msg(param->model, &param->ctx,
            ESP_BLE_MESH_MODEL_OP_SENSOR_STATUS, length, status);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send Sensor Status");
    }
    free(status);
}

static void example_ble_mesh_send_sensor_column_status(esp_ble_mesh_sensor_server_cb_param_t *param)
{
    uint8_t *status = NULL;
    uint16_t length = 0;
    esp_err_t err;

    length = ESP_BLE_MESH_SENSOR_PROPERTY_ID_LEN +param->value.get.sensor_column.raw_value_x->len;

    status = calloc(1, length);
    if (!status) {
        ESP_LOGE(TAG, "No memory for sensor column status!");
        return;
    }

    memcpy(status, &param->value.get.sensor_column.property_id, ESP_BLE_MESH_SENSOR_PROPERTY_ID_LEN);
    memcpy(status + ESP_BLE_MESH_SENSOR_PROPERTY_ID_LEN, param->value.get.sensor_column.raw_value_x->data,
        param->value.get.sensor_column.raw_value_x->len);

    err = esp_ble_mesh_server_model_send_msg(param->model, &param->ctx,
            ESP_BLE_MESH_MODEL_OP_SENSOR_COLUMN_STATUS, length, status);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send Sensor Column Status");
    }
    free(status);
}

static void example_ble_mesh_send_sensor_series_status(esp_ble_mesh_sensor_server_cb_param_t *param)
{
    esp_err_t err;

    err = esp_ble_mesh_server_model_send_msg(param->model, &param->ctx,
            ESP_BLE_MESH_MODEL_OP_SENSOR_SERIES_STATUS,
            ESP_BLE_MESH_SENSOR_PROPERTY_ID_LEN,
            (uint8_t *)&param->value.get.sensor_series.property_id);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send Sensor Column Status");
    }
}

static void example_ble_mesh_sensor_server_cb(esp_ble_mesh_sensor_server_cb_event_t event,
                                              esp_ble_mesh_sensor_server_cb_param_t *param)
{
    ESP_LOGI(TAG, "Sensor server, event %d, src 0x%04x, dst 0x%04x, model_id 0x%04x",
        event, param->ctx.addr, param->ctx.recv_dst, param->model->model_id);

    switch (event) {
    case ESP_BLE_MESH_SENSOR_SERVER_RECV_GET_MSG_EVT:
        switch (param->ctx.recv_op) {
        case ESP_BLE_MESH_MODEL_OP_SENSOR_DESCRIPTOR_GET:
            ESP_LOGI(TAG, "ESP_BLE_MESH_MODEL_OP_SENSOR_DESCRIPTOR_GET");
            example_ble_mesh_send_sensor_descriptor_status(param);
            break;
        case ESP_BLE_MESH_MODEL_OP_SENSOR_CADENCE_GET:
            ESP_LOGI(TAG, "ESP_BLE_MESH_MODEL_OP_SENSOR_CADENCE_GET");
            example_ble_mesh_send_sensor_cadence_status(param);
            break;
        case ESP_BLE_MESH_MODEL_OP_SENSOR_SETTINGS_GET:
            ESP_LOGI(TAG, "ESP_BLE_MESH_MODEL_OP_SENSOR_SETTINGS_GET");
            example_ble_mesh_send_sensor_settings_status(param);
            break;
        case ESP_BLE_MESH_MODEL_OP_SENSOR_SETTING_GET:
            ESP_LOGI(TAG, "ESP_BLE_MESH_MODEL_OP_SENSOR_SETTING_GET");
            example_ble_mesh_send_sensor_setting_status(param);
            break;
        case ESP_BLE_MESH_MODEL_OP_SENSOR_GET:
            ESP_LOGI(TAG, "ESP_BLE_MESH_MODEL_OP_SENSOR_GET");
            example_ble_mesh_send_sensor_status(param);
            break;
        case ESP_BLE_MESH_MODEL_OP_SENSOR_COLUMN_GET:
            ESP_LOGI(TAG, "ESP_BLE_MESH_MODEL_OP_SENSOR_COLUMN_GET");
            example_ble_mesh_send_sensor_column_status(param);
            break;
        case ESP_BLE_MESH_MODEL_OP_SENSOR_SERIES_GET:
            ESP_LOGI(TAG, "ESP_BLE_MESH_MODEL_OP_SENSOR_SERIES_GET");
            example_ble_mesh_send_sensor_series_status(param);
            break;
        default:
            ESP_LOGE(TAG, "Unknown Sensor Get opcode 0x%04" PRIx32, param->ctx.recv_op);
            return;
        }
        break;
    case ESP_BLE_MESH_SENSOR_SERVER_RECV_SET_MSG_EVT:
        switch (param->ctx.recv_op) {
        case ESP_BLE_MESH_MODEL_OP_SENSOR_CADENCE_SET:
            ESP_LOGI(TAG, "ESP_BLE_MESH_MODEL_OP_SENSOR_CADENCE_SET");
            example_ble_mesh_send_sensor_cadence_status(param);
            break;
        case ESP_BLE_MESH_MODEL_OP_SENSOR_CADENCE_SET_UNACK:
            ESP_LOGI(TAG, "ESP_BLE_MESH_MODEL_OP_SENSOR_CADENCE_SET_UNACK");
            break;
        case ESP_BLE_MESH_MODEL_OP_SENSOR_SETTING_SET:
            ESP_LOGI(TAG, "ESP_BLE_MESH_MODEL_OP_SENSOR_SETTING_SET");
            example_ble_mesh_send_sensor_setting_status(param);
            break;
        case ESP_BLE_MESH_MODEL_OP_SENSOR_SETTING_SET_UNACK:
            ESP_LOGI(TAG, "ESP_BLE_MESH_MODEL_OP_SENSOR_SETTING_SET_UNACK");
            break;
        default:
            ESP_LOGE(TAG, "Unknown Sensor Set opcode 0x%04" PRIx32, param->ctx.recv_op);
            break;
        }
        break;
    default:
        ESP_LOGE(TAG, "Unknown Sensor Server event %d", event);
        break;
    }
}


static esp_err_t ble_mesh_init(void)
{
    esp_err_t err = ESP_OK;

    esp_ble_mesh_register_prov_callback(example_ble_mesh_provisioning_cb);
    esp_ble_mesh_register_generic_client_callback(example_ble_mesh_generic_client_cb);
    esp_ble_mesh_register_config_server_callback(example_ble_mesh_config_server_cb);
    esp_ble_mesh_register_health_server_callback(example_ble_mesh_health_server_cb);
    esp_ble_mesh_register_sensor_server_callback(example_ble_mesh_sensor_server_cb);


    err = esp_ble_mesh_init(&provision, &composition);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize mesh stack (err %d)", err);
        return err;
    }

    err = esp_ble_mesh_node_prov_enable(ESP_BLE_MESH_PROV_ADV | ESP_BLE_MESH_PROV_GATT);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable mesh node (err %d)", err);
        return err;
    }

    ESP_LOGI(TAG, "BLE Mesh Node initialized");

    board_led_operation(LED_GREEN, LED_ON);

    return err;
}

void app_main(void)
{
    esp_err_t err;

    ESP_LOGI(TAG, "Initializing...");

    board_init();

    err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    err = bluetooth_init();
    if (err) {
        ESP_LOGE(TAG, "esp32_bluetooth_init failed (err %d)", err);
        return;
    }

    /* Open nvs namespace for storing/restoring mesh example info */
    err = ble_mesh_nvs_open(&NVS_HANDLE);
    if (err) {
        return;
    }

    ble_mesh_get_dev_uuid(dev_uuid);

    /* Initialize the Bluetooth Mesh Subsystem */
    err = ble_mesh_init();
    if (err) {
        ESP_LOGE(TAG, "Bluetooth mesh init failed (err %d)", err);
    }
}
