#pragma once

#include <esp_http_server.h>
#include <esp_log.h>
#include "esp_wifi.h"
#include "esp_err.h"
#include <string.h>
#include "cJSON.h"

static const char *TAG_STA = "ftm_station";
static const char *TAG_FTM = "ftm_control";

extern const char ftm_control_start[] asm("_binary_ftm_control_html_start");
extern const char ftm_control_end[] asm("_binary_ftm_control_html_end");

static esp_err_t ftm_control_handler(httpd_req_t *req){
    const uint32_t ftm_control_len = ftm_control_end - ftm_control_start;

    ESP_LOGI(TAG_FTM, "Serve ftm control");
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, ftm_control_start, ftm_control_len);

    return ESP_OK;
}

static esp_err_t wifi_perform_scan(httpd_req_t *req)
{
    wifi_scan_config_t scan_config = { 0 };
    scan_config.ssid = (uint8_t *) NULL;
    uint8_t i;

    ESP_LOGI(TAG_FTM, "Serve /ftm/ssid-list");

    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_APSTA) );
    if (ESP_OK != esp_wifi_scan_start(&scan_config, true)) {
        ESP_LOGI(TAG_STA, "Failed to perform scan");
        return ESP_FAIL;
    }

    uint16_t g_scan_ap_num;
    wifi_ap_record_t *g_ap_list_buffer;

    esp_wifi_scan_get_ap_num(&g_scan_ap_num);
    if (g_scan_ap_num == 0) {
        ESP_LOGI(TAG_STA, "No matching AP found");
        return ESP_FAIL;
    }

    g_ap_list_buffer = malloc(g_scan_ap_num * sizeof(wifi_ap_record_t));
    if (g_ap_list_buffer == NULL) {
        ESP_LOGE(TAG_STA, "Failed to malloc buffer to print scan results");
        return ESP_FAIL;
    }

    cJSON *root = cJSON_CreateObject();
    if(root == NULL){
        ESP_LOGE(TAG_FTM, "Failed to create JSON root");
        return ESP_FAIL;
    }
    cJSON *ap_list = cJSON_CreateArray();
    if(ap_list == NULL){
        ESP_LOGE(TAG_FTM, "Failed to create JSON array");
        return ESP_FAIL;
    }
    cJSON_AddItemToObject(root, "ap_list", ap_list);
    if (esp_wifi_scan_get_ap_records(&g_scan_ap_num, (wifi_ap_record_t *)g_ap_list_buffer) == ESP_OK) {
        cJSON *ap_entry = NULL;
        cJSON *name = NULL;
        cJSON *FTM_responder = NULL;
        cJSON *rssi = NULL;
        for (i = 0; i < g_scan_ap_num; i++) {
            ap_entry = cJSON_CreateObject();
            if(ap_entry == NULL){
                ESP_LOGE(TAG_FTM, "Failed to create JSON AP entry");
                return ESP_FAIL;
            }
            cJSON_AddItemToArray(ap_list, ap_entry);

            name = cJSON_CreateString((char *) g_ap_list_buffer[i].ssid);
            if(name == NULL){
                ESP_LOGE(TAG_FTM, "Failed to create JSON AP name");
                return ESP_FAIL;
            }
            cJSON_AddItemToObject(ap_entry, "SSID", name);

            FTM_responder = g_ap_list_buffer[i].ftm_responder ? cJSON_CreateTrue() : cJSON_CreateFalse();
            if(FTM_responder == NULL){
                ESP_LOGE(TAG_FTM, "Failed to create JSON AP ftm_responder");
                return ESP_FAIL;
            }
            cJSON_AddItemToObject(ap_entry, "ftm_responder", FTM_responder);

            rssi = cJSON_CreateNumber(g_ap_list_buffer[i].rssi);
            if(rssi == NULL){
                ESP_LOGE(TAG_FTM, "Failed to create JSON AP rssi");
                return ESP_FAIL;
            }
            cJSON_AddItemToObject(ap_entry, "RSSI", rssi);
        }
    }

    ESP_LOGI(TAG_STA, "sta scan done");

    httpd_resp_set_type(req, "application/json");
    
    char *json_string = cJSON_PrintUnformatted(root);
    httpd_resp_send(req, json_string, strlen(json_string));
    ESP_LOGI(TAG_FTM, "%s", json_string);

    return ESP_OK;
}

const httpd_uri_t ftm_control = {
        .uri        = "/ftm",
        .method     = HTTP_GET,
        .handler    = ftm_control_handler,
        .user_ctx   = NULL,
        .is_websocket = false
};

const httpd_uri_t ftm_control_ssid_list = {
        .uri        = "/ftm/ssid-list",
        .method     = HTTP_GET,
        .handler    = wifi_perform_scan,
        .user_ctx   = NULL,
        .is_websocket = false
};