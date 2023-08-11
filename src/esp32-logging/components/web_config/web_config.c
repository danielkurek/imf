#include <stdio.h>
#include "web_config.h"
#include "wifi_connect.h"

#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include "esp_netif.h"
#include <esp_http_server.h>
#include "esp_mac.h"

#define SCRATCH_BUFSIZE (10240)

static const char *TAG = "web_config";

static esp_err_t hello_world_handler(httpd_req_t *req){
    httpd_resp_sendstr(req, "Hello world!");
    return ESP_OK;
}

static void web_config_register_uri(httpd_handle_t server)
{
    char *scratch_buffer = malloc(SCRATCH_BUFSIZE);

    const httpd_uri_t index = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = hello_world_handler,
        .user_ctx = scratch_buffer};
    httpd_register_uri_handler(server, &index);

    // const httpd_uri_t ftm_control = {
    //     .uri = "/ftm",
    //     .method = HTTP_GET,
    //     .handler = ftm_control_handler,
    //     .user_ctx = scratch_buffer};
    // httpd_register_uri_handler(server, &ftm_control);

    // const httpd_uri_t ftm_control_ssid_list = {
    //     .uri = "/api/v1/ftm/ssid-list",
    //     .method = HTTP_GET,
    //     .handler = wifi_perform_scan,
    //     .user_ctx = scratch_buffer};
    // httpd_register_uri_handler(server, &ftm_control_ssid_list);

    // const httpd_uri_t ftm_control_start_ap = {
    //     .uri = "/api/v1/ftm/start_ap",
    //     .method = HTTP_POST,
    //     .handler = ftm_start_ap,
    //     .user_ctx = scratch_buffer};
    // httpd_register_uri_handler(server, &ftm_control_start_ap);

    // const httpd_uri_t ftm_control_measurement = {
    //     .uri = "/api/v1/ftm/measure_distance",
    //     .method = HTTP_GET,
    //     .handler = ftm_measurement,
    //     .user_ctx = scratch_buffer};
    // httpd_register_uri_handler(server, &ftm_control_measurement);

    // const httpd_uri_t ftm_calibrate = {
    //     .uri = "/api/v1/ftm/calibration",
    //     .method = HTTP_GET,
    //     .handler = ftm_calibration,
    //     .user_ctx = scratch_buffer};
    // httpd_register_uri_handler(server, &ftm_calibrate);
}

static httpd_handle_t start_webserver(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    // Start the httpd server
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        // Registering the ws handler
        ESP_LOGI(TAG, "Registering URI handlers");
        web_config_register_uri(server);
        return server;
    }

    ESP_LOGI(TAG, "Error starting server!");
    return NULL;
}

static esp_err_t stop_webserver(httpd_handle_t server)
{
    // Stop the httpd server
    return httpd_stop(server);
}

static void disconnect_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
    httpd_handle_t* server = (httpd_handle_t*) arg;
    if (*server) {
        ESP_LOGI(TAG, "Stopping webserver");
        if (stop_webserver(*server) == ESP_OK) {
            *server = NULL;
        } else {
            ESP_LOGE(TAG, "Failed to stop http server");
        }
    }
}

static void connect_handler(void* arg, esp_event_base_t event_base,
                            int32_t event_id, void* event_data)
{
    httpd_handle_t* server = (httpd_handle_t*) arg;
    if (*server == NULL) {
        ESP_LOGI(TAG, "Starting webserver");
        *server = start_webserver();
    }
}

static esp_err_t wifi_initialize(){
    // connect to preconfigured wifi if available
    // TODO: load from NVS
    // wifi_config_t wifi_config = {
    //     .sta = {
    //         .ssid = CONFIG_WIFI_CONNECT_SSID,
    //         .password = CONFIG_WIFI_CONNECT_PASSWORD,
    //         .scan_method = WIFI_CONNECT_SCAN_METHOD,
    //         .sort_method = WIFI_CONNECT_AP_SORT_METHOD,
    //         .threshold = {
    //             .rssi = CONFIG_WIFI_CONNECT_SCAN_RSSI_THRESHOLD,
    //             .authmode = WIFI_CONNECT_SCAN_AUTH_MODE_THRESHOLD
    //         }
    //     },
    // };
    
    // esp_err_t ret = wifi_connect(wifi_config);
    esp_err_t ret = wifi_connect_default();
    
    // otherwise create WiFi AP
    if(ret != ESP_OK){
        // create WIFI AP
        wifi_init_ap_default();
        // TODO: load from NVS
        // void wifi_init_ap_simple(ssid, password, channel);
    }
    return ESP_OK;
}

void web_config_start()
{
    static httpd_handle_t server = NULL;

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }

    ESP_ERROR_CHECK( ret );
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();

    ESP_ERROR_CHECK(wifi_initialize());

    // register http server
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &connect_handler, &server));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &disconnect_handler, &server));

    server = start_webserver();
}


