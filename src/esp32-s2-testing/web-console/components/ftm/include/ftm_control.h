#pragma once

#include <esp_http_server.h>
#include <esp_log.h>
#include "esp_wifi.h"
#include "esp_err.h"
#include <string.h>
#include "cJSON.h"

#include "ftm.h"

#define SCRATCH_BUFSIZE (10240)

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
    // TODO: option to send only FTM responders
    wifi_scan_config_t scan_config = { 0 };
    scan_config.ssid = (uint8_t *) NULL;
    uint8_t i;

    ESP_LOGI(TAG_FTM, "Serve /ftm/ssid-list");
    ESP_LOGI(TAG_FTM, "%s", req->uri);

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
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    
    char *json_string = cJSON_PrintUnformatted(root);
    if(json_string == NULL){
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed creating JSON");
        return ESP_FAIL;
    }
    httpd_resp_send(req, json_string, strlen(json_string));
    ESP_LOGI(TAG_FTM, "%s", json_string);

    free(g_ap_list_buffer);
    cJSON_Delete(root);
    free(json_string);
    return ESP_OK;
}

static esp_err_t ftm_start_ap(httpd_req_t *req)
{
    int total_len = req->content_len;
    int cur_len = 0;
    char *buf = (char *)(req->user_ctx);
    int received = 0;
    if (total_len >= SCRATCH_BUFSIZE) {
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "content too long");
        return ESP_FAIL;
    }
    while (cur_len < total_len) {
        received = httpd_req_recv(req, buf + cur_len, total_len);
        if (received <= 0) {
            /* Respond with 500 Internal Server Error */
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to post control value");
            return ESP_FAIL;
        }
        cur_len += received;
    }
    buf[total_len] = '\0';

    cJSON *root = cJSON_Parse(buf);
    cJSON *ap_ssid_obj = cJSON_GetObjectItemCaseSensitive(root, "SSID");
    if(!(cJSON_IsString(ap_ssid_obj) && (ap_ssid_obj->valuestring != NULL))){
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to parse SSID");
        return ESP_FAIL;
    }
    char *ap_ssid = ap_ssid_obj->valuestring;
    ESP_LOGI(TAG_FTM, "Parsed SSID: %s", ap_ssid);
    httpd_resp_sendstr(req, "Post control value successfully");

    wifi_cmd_ap_set(ap_ssid, "");

    cJSON_Delete(root);

    return ESP_OK;
}

static esp_err_t ftm_measurement(httpd_req_t *req){
    char *buf = (char *)(req->user_ctx);
    char ssid[MAX_SSID_LEN];

    ESP_ERROR_CHECK(httpd_req_get_url_query_str(req, buf, SCRATCH_BUFSIZE));
    ESP_ERROR_CHECK(httpd_query_key_value(buf, "ssid", (char*) (&ssid), MAX_SSID_LEN));


    ESP_LOGI(TAG_FTM, "Parsed SSID from query: %s", ssid);

    wifi_event_ftm_report_t *ftm_report;
    ftm_report = wifi_cmd_ftm(ssid);

    if(ftm_report == NULL){
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to get FTM report");
        return ESP_FAIL;
    }
    cJSON *root = cJSON_CreateObject();
    if(root == NULL){
        ESP_LOGE(TAG_FTM, "Failed to create JSON root");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to create JSON");
        return ESP_FAIL;
    }
    if(cJSON_AddNumberToObject(root, "dist_est", ftm_report->dist_est) == NULL){
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to create JSON");
        return ESP_FAIL;
    }
    if(cJSON_AddNumberToObject(root, "rtt_est", ftm_report->rtt_est) == NULL){
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to create JSON");
        return ESP_FAIL;
    }
    if(cJSON_AddNumberToObject(root, "rtt_raw", ftm_report->rtt_raw) == NULL){
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to create JSON");
        return ESP_FAIL;
    }
    if(cJSON_AddNumberToObject(root, "rtt_raw", ftm_report->rtt_raw) == NULL){
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to create JSON");
        return ESP_FAIL;
    }

    cJSON *raw_data_list = cJSON_CreateArray();
    if(raw_data_list == NULL){
        ESP_LOGE(TAG_FTM, "Failed to create JSON array");
        return ESP_FAIL;
    }
    cJSON_AddItemToObject(root, "raw_data_list", raw_data_list);
    
    cJSON *item = NULL;
    for(int i = 0; i < ftm_report->ftm_report_num_entries; i++){
        item = cJSON_CreateObject();
        if(item == NULL){
            cJSON_AddTrueToObject(root, "failed_raw_data");
            break;
        }
        cJSON_AddItemToArray(raw_data_list, item);
        if(cJSON_AddNumberToObject(item, "token", ftm_report->ftm_report_data[i].dlog_token) == NULL){
            cJSON_AddTrueToObject(root, "failed_raw_data");
            break;
        }
        if(cJSON_AddNumberToObject(item, "rtt", ftm_report->ftm_report_data[i].rtt) == NULL){
            cJSON_AddTrueToObject(root, "failed_raw_data");
            break;
        }
        if(cJSON_AddNumberToObject(item, "t1", ftm_report->ftm_report_data[i].t1) == NULL){
            cJSON_AddTrueToObject(root, "failed_raw_data");
            break;
        }
        if(cJSON_AddNumberToObject(item, "t2", ftm_report->ftm_report_data[i].t2) == NULL){
            cJSON_AddTrueToObject(root, "failed_raw_data");
            break;
        }
        if(cJSON_AddNumberToObject(item, "t3", ftm_report->ftm_report_data[i].t2) == NULL){
            cJSON_AddTrueToObject(root, "failed_raw_data");
            break;
        }
        if(cJSON_AddNumberToObject(item, "t4", ftm_report->ftm_report_data[i].t2) == NULL){
            cJSON_AddTrueToObject(root, "failed_raw_data");
            break;
        }
        if(cJSON_AddNumberToObject(item, "rssi", ftm_report->ftm_report_data[i].rssi) == NULL){
            cJSON_AddTrueToObject(root, "failed_raw_data");
            break;
        }
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    
    char *json_string = cJSON_PrintUnformatted(root);
    if(json_string == NULL){
        ESP_LOGE(TAG_FTM, "JSON could not print");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to create JSON");
        return ESP_FAIL;
    }
    httpd_resp_send(req, json_string, strlen(json_string));

    ESP_LOGI(TAG_FTM, "JSON FTM report sent: %s", json_string);

    cJSON_Delete(root);
    free(json_string);

    return ESP_OK;
}


esp_err_t ftm_register_uri(httpd_handle_t server){
    char *scratch_buffer = malloc(SCRATCH_BUFSIZE);

    const httpd_uri_t ftm_control = {
        .uri        = "/ftm",
        .method     = HTTP_GET,
        .handler    = ftm_control_handler,
        .user_ctx   = scratch_buffer
    };
    httpd_register_uri_handler(server, &ftm_control);

    const httpd_uri_t ftm_control_ssid_list = {
            .uri        = "/api/v1/ftm/ssid-list",
            .method     = HTTP_GET,
            .handler    = wifi_perform_scan,
            .user_ctx   = scratch_buffer
    };
    httpd_register_uri_handler(server, &ftm_control_ssid_list);

    const httpd_uri_t ftm_control_start_ap = {
            .uri        = "/api/v1/ftm/start_ap",
            .method     = HTTP_POST,
            .handler    = ftm_start_ap,
            .user_ctx   = scratch_buffer
    };
    httpd_register_uri_handler(server, &ftm_control_start_ap);

    const httpd_uri_t ftm_control_measurement = {
            .uri        = "/api/v1/ftm/measure_distance",
            .method     = HTTP_GET,
            .handler    = ftm_measurement,
            .user_ctx   = scratch_buffer
    };
    httpd_register_uri_handler(server, &ftm_control_measurement);

    return ESP_OK;
}