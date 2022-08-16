#pragma once

#include <esp_http_server.h>
#include <esp_log.h>
#include "esp_wifi.h"
#include "esp_err.h"
#include <string.h>
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
    httpd_resp_set_type(req, "text/html");

    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_APSTA) );
    if (ESP_OK != esp_wifi_scan_start(&scan_config, true)) {
        ESP_LOGI(TAG_STA, "Failed to perform scan");
        return false;
    }

    uint16_t g_scan_ap_num;
    wifi_ap_record_t *g_ap_list_buffer;

    esp_wifi_scan_get_ap_num(&g_scan_ap_num);
    if (g_scan_ap_num == 0) {
        ESP_LOGI(TAG_STA, "No matching AP found");
        return false;
    }

    g_ap_list_buffer = malloc(g_scan_ap_num * sizeof(wifi_ap_record_t));
    if (g_ap_list_buffer == NULL) {
        ESP_LOGE(TAG_STA, "Failed to malloc buffer to print scan results");
        return false;
    }

    int size = 0;
    char* ap_list;
    int index = 0;
    if (esp_wifi_scan_get_ap_records(&g_scan_ap_num, (wifi_ap_record_t *)g_ap_list_buffer) == ESP_OK) {
        // TODO: make JSON response
        for (i = 0; i < g_scan_ap_num; i++) {
            // show just ftm_responders
            // if (g_ap_list_buffer[i].ftm_responder)
            size += strlen((char*)g_ap_list_buffer[i].ssid);
            ESP_LOGI(TAG_FTM, "%s", g_ap_list_buffer[i].ssid);
        }
        ESP_LOGI(TAG_FTM, "%d", size);
        ap_list = malloc(size + (g_scan_ap_num-1));
        for (i = 0; i < g_scan_ap_num; i++) {
            // show just ftm_responders
            // if (g_ap_list_buffer[i].ftm_responder)
            int str_size = strlen((char*) g_ap_list_buffer[i].ssid);
            for(int j = 0; j < str_size; ++j){
                ap_list[index] = g_ap_list_buffer[i].ssid[j];
                ++index;
            }
            if(i+1 < g_scan_ap_num) {
                ap_list[index] = ',';
                index++;
            }
        }
    }

    ESP_LOGI(TAG_STA, "sta scan done");
    if(size == 0){
        httpd_resp_send(req, "", 0);
    }
    else {
        httpd_resp_send(req, ap_list, index);
        ESP_LOGI(TAG_FTM, "%s", ap_list);
    }

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