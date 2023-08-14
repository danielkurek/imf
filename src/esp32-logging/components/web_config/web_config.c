/*
 * TODO list:
 *  - support for multiple int types NVS (c++ templates?)
 *  - api get firmware version
 *  - perform OTA from requested URL (with version check)
 */

#include <stdio.h>
#include "web_config.h"
#include "wifi_connect.h"
#include "http_common.h"

#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include "esp_netif.h"
#include <esp_http_server.h>
#include "esp_mac.h"

#define SCRATCH_BUFSIZE (10240)

#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))

#define API_PREFIX "/api/v1"
#define API_PATH(path) API_PREFIX path

static const char *TAG = "web_config";

const char *config_keys[] = {
    "SSID", "password", "channel",
};

const nvs_type_t config_types[] = {
    NVS_TYPE_STR, NVS_TYPE_STR, NVS_TYPE_I16,
};

static const size_t config_keys_len = sizeof(config_keys) / sizeof(config_keys[0]);

typedef struct {
    nvs_handle_t config_handle;
    char scratch_buf[SCRATCH_BUFSIZE];
} web_config_data_t;

static esp_err_t get_key_from_uri(char *dest, size_t prefix_len, const char *full_uri, size_t destsize)
{
    const char *uri = full_uri + prefix_len;
    size_t pathlen = strlen(uri);

    const char *quest = strchr(uri, '?');
    if (quest) {
        pathlen = MIN(pathlen, quest - uri);
    }
    const char *hash = strchr(uri, '#');
    if (hash) {
        pathlen = MIN(pathlen, hash - uri);
    }
    const char *slash = strchr(uri, '/');
    if (slash) {
        // nvs key should not have slash
        return ESP_FAIL;
    }

    if (pathlen + 1 > destsize) {
        return ESP_FAIL;
    }

    // key cannot be empty
    if (pathlen == 0){
        return ESP_FAIL;
    }

    /* Construct full path (base + path) */
    strlcpy(dest, uri, pathlen + 1);

    /* Return pointer to path, skipping the base */
    return ESP_OK;
}

static int config_find_key(const char* key){
    for (int i = 0; i < config_keys_len; i++){
        if(strcmp(key, config_keys[i]) == 0){
            return i;
        }
    }
    return -1;
}

static esp_err_t hello_world_handler(httpd_req_t *req){
    httpd_resp_sendstr(req, "Hello world!");
    return ESP_OK;
}

static esp_err_t nvs_read_get_handler_str(httpd_req_t *req, size_t key_index){
    web_config_data_t *data = (web_config_data_t *) req->user_ctx;

    char value[32];
    size_t value_len = sizeof(value);
    esp_err_t err = nvs_get_str(data->config_handle, config_keys[key_index], value, &value_len);
    if(err != ESP_OK){
        return httpd_resp_sendstr(req, "Cannot get key");
    }

    return httpd_resp_sendstr(req, value);
}

static esp_err_t nvs_read_get_handler_i16(httpd_req_t *req, size_t key_index){
    web_config_data_t *data = (web_config_data_t *) req->user_ctx;
    int16_t value;

    esp_err_t err = nvs_get_i16(data->config_handle, config_keys[key_index], &value);
    if(err != ESP_OK){
        return httpd_resp_sendstr(req, "Cannot get key");
    }

    char response[32];
    sprintf(response, "%d", value);

    return httpd_resp_sendstr(req, response);
}


static esp_err_t nvs_read_get_handler(httpd_req_t *req){
    web_config_data_t *data = (web_config_data_t *) req->user_ctx;
    static const size_t prefix_len = sizeof(API_PATH("/nvs/")) - 1;

    char nvs_key[NVS_KEY_NAME_MAX_SIZE];
    esp_err_t ret = get_key_from_uri(nvs_key, prefix_len, req->uri, sizeof(nvs_key));
    if(ret != ESP_OK){
        return httpd_resp_sendstr(req, "Cannot parse key");
    }

    int pos = config_find_key(nvs_key);
    if(pos < 0){
        return httpd_resp_sendstr(req, "Invalid key");
    }

    switch(config_types[pos]){
        case NVS_TYPE_STR:
            return nvs_read_get_handler_str(req, pos);
        case NVS_TYPE_I16:
            return nvs_read_get_handler_i16(req, pos);
        default:
            ESP_LOGE(TAG, "Config type not implemented!!!");
            return ESP_FAIL;
    }
}

static esp_err_t nvs_write_post_handler_str(httpd_req_t *req, size_t key_index, char *received_value){
    web_config_data_t *data = (web_config_data_t *) req->user_ctx;

    char value[32];
    strlcpy(value, received_value, sizeof(value));

    esp_err_t err = nvs_set_str(data->config_handle, config_keys[key_index], value);
    if(err != ESP_OK){
        return httpd_resp_sendstr(req, "Cannot set key");
    }
    
    return httpd_resp_sendstr(req, value);
}

static esp_err_t nvs_write_post_handler_i16(httpd_req_t *req, size_t key_index, char *received_value){
    web_config_data_t *data = (web_config_data_t *) req->user_ctx;

    int16_t value = 0;
    sscanf(received_value, "%" SCNd16, &value);

    esp_err_t err = nvs_set_i16(data->config_handle, config_keys[key_index], value);
    if(err != ESP_OK){
        return httpd_resp_sendstr(req, "Cannot set key");
    }

    char response[32];
    sprintf(response, "%d", value);

    return httpd_resp_sendstr(req, response);
}

static esp_err_t nvs_write_post_handler(httpd_req_t *req){
    web_config_data_t *data = (web_config_data_t *) req->user_ctx;
    static const size_t prefix_len = sizeof(API_PATH("/nvs/")) - 1;

    char nvs_key[NVS_KEY_NAME_MAX_SIZE];
    esp_err_t ret = get_key_from_uri(nvs_key, prefix_len, req->uri, sizeof(nvs_key));
    if(ret != ESP_OK){
        return httpd_resp_sendstr(req, "Cannot parse key");
    }

    int pos = config_find_key(nvs_key);
    if(pos < 0){
        return httpd_resp_sendstr(req, "Invalid key");
    }
    
    long int received = http_load_post_req_to_buf(req, data->scratch_buf, sizeof(data->scratch_buf));
    if(received < 0){
        return ESP_FAIL;
    }

    switch(config_types[pos]){
        case NVS_TYPE_STR:
            return nvs_write_post_handler_str(req, pos, data->scratch_buf);
        case NVS_TYPE_I16:
            return nvs_write_post_handler_i16(req, pos, data->scratch_buf);
        default:
            ESP_LOGE(TAG, "Config type not implemented!!!");
            return ESP_FAIL;
    }
    
}

static esp_err_t nvs_save_get_handler(httpd_req_t *req){
    web_config_data_t *data = (web_config_data_t *) req->user_ctx;
    esp_err_t err = nvs_commit(data->config_handle);
    if(err != ESP_OK){
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Could not save cahnges.");
        return ESP_OK;
    }

    httpd_resp_sendstr(req, "Ok");
    return ESP_OK;
}


static void web_config_register_uri(httpd_handle_t server, web_config_data_t *user_ctx)
{
    // char *scratch_buffer = malloc(SCRATCH_BUFSIZE);

    const httpd_uri_t index = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = hello_world_handler,
        .user_ctx = user_ctx};
    httpd_register_uri_handler(server, &index);

    const httpd_uri_t nvs_read = {
        .uri = API_PATH("/nvs/*"),
        .method = HTTP_GET,
        .handler = nvs_read_get_handler,
        .user_ctx = user_ctx};
    httpd_register_uri_handler(server, &nvs_read);

    const httpd_uri_t nvs_write = {
        .uri = API_PATH("/nvs/*"),
        .method = HTTP_POST,
        .handler = nvs_write_post_handler,
        .user_ctx = user_ctx};
    httpd_register_uri_handler(server, &nvs_write);

    const httpd_uri_t nvs_save = {
        .uri = API_PATH("/nvs_save"),
        .method = HTTP_GET,
        .handler = nvs_save_get_handler,
        .user_ctx = user_ctx};
    httpd_register_uri_handler(server, &nvs_save);
}

static httpd_handle_t start_webserver(void)
{
    static web_config_data_t data;

    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    // allow wildcard matching
    config.uri_match_fn = httpd_uri_match_wildcard;

    // Start the httpd server
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        if(nvs_open("config", NVS_READWRITE, &data.config_handle) != ESP_OK){
            ESP_LOGI(TAG, "Error opening NVS!");
            return NULL;
        }

        ESP_LOGI(TAG, "Registering URI handlers");
        web_config_register_uri(server, &data);
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


