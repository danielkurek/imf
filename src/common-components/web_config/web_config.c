/*
 * TODO list:
 *  - support for multiple int types NVS (c++ templates?)
 *  - api get firmware version
 *  - perform OTA from requested URL (with version check)
 *  - log download
 */

#include <stdio.h>
#include "web_config.h"
#include "wifi_connect.h"
#include "http_common.h"
#include "logger.h"
#include "ota.h"

#include <regex.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include "esp_netif.h"
#include <esp_http_server.h>
#include "esp_mac.h"
#include <errno.h>

#define SCRATCH_BUFSIZE (10240)

#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))

#define API_PREFIX "/api/v1"
#define API_PATH(path) API_PREFIX path

static const char *TAG = "web_config";

#define STA_SSID_KEY "sta/SSID"
#define STA_PASSWORD_KEY "sta/password"
#define AP_SSID_KEY "ap/SSID"
#define AP_PASSWORD_KEY "ap/password"
#define AP_CHANNEL_KEY "ap/channel"

typedef struct {
    char *key;
    nvs_type_t type;
    bool value_to_log;
} config_option_t;

const config_option_t options[] ={
    {
        .key = STA_SSID_KEY,
        .type = NVS_TYPE_STR,
        .value_to_log = true,
    },
    {
        .key = STA_PASSWORD_KEY,
        .type = NVS_TYPE_STR,
        .value_to_log = false,
    },
    {
        .key = AP_SSID_KEY,
        .type = NVS_TYPE_STR,
        .value_to_log = true,
    },
    {
        .key = AP_PASSWORD_KEY,
        .type = NVS_TYPE_STR,
        .value_to_log = false,
    },
    {
        .key = AP_CHANNEL_KEY,
        .type = NVS_TYPE_I16,
        .value_to_log = true,
    },
    
};

static const size_t options_len = sizeof(options) / sizeof(options[0]);

typedef struct {
    nvs_handle_t config_handle;
    char scratch_buf[SCRATCH_BUFSIZE];
    httpd_handle_t server;
    regex_t url_regex;
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
    for (int i = 0; i < options_len; i++){
        if(strcmp(key, options[i].key) == 0){
            return i;
        }
    }
    return -1;
}

static esp_err_t hello_world_handler(httpd_req_t *req){
    httpd_resp_sendstr(req, "Hello world!");
    return ESP_OK;
}

static esp_err_t nvs_read_get_handler_str(httpd_req_t *req, size_t option_index){
    web_config_data_t *data = (web_config_data_t *) req->user_ctx;

    char value[32];
    size_t value_len = sizeof(value) / sizeof(value[0]);
    esp_err_t err = nvs_get_str(data->config_handle, options[option_index].key, value, &value_len);
    if(err != ESP_OK){
        return httpd_resp_sendstr(req, "Cannot get key");
    }

    return httpd_resp_sendstr(req, value);
}

static esp_err_t nvs_read_get_handler_i16(httpd_req_t *req, size_t option_index){
    web_config_data_t *data = (web_config_data_t *) req->user_ctx;
    int16_t value;

    esp_err_t err = nvs_get_i16(data->config_handle, options[option_index].key, &value);
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

    switch(options[pos].type){
        case NVS_TYPE_STR:
            return nvs_read_get_handler_str(req, pos);
        case NVS_TYPE_I16:
            return nvs_read_get_handler_i16(req, pos);
        default:
            ESP_LOGE(TAG, "Config type not implemented!!!");
            return ESP_FAIL;
    }
}

static esp_err_t nvs_write_post_handler_str(httpd_req_t *req, size_t option_index, char *received_value){
    web_config_data_t *data = (web_config_data_t *) req->user_ctx;

    char value[32];
    strlcpy(value, received_value, sizeof(value));

    esp_err_t err = nvs_set_str(data->config_handle, options[option_index].key, value);
    if(err != ESP_OK){
        return httpd_resp_sendstr(req, "Cannot set key");
    }

    LOGGER_I(TAG, "changed %s to %s", options[option_index].key, value);
    
    return httpd_resp_sendstr(req, value);
}

static esp_err_t nvs_write_post_handler_i16(httpd_req_t *req, size_t option_index, char *received_value){
    web_config_data_t *data = (web_config_data_t *) req->user_ctx;

    int16_t value = 0;
    sscanf(received_value, "%" SCNd16, &value);

    esp_err_t err = nvs_set_i16(data->config_handle, options[option_index].key, value);
    if(err != ESP_OK){
        return httpd_resp_sendstr(req, "Cannot set key");
    }

    char response[32];
    sprintf(response, "%d", value);

    LOGGER_I(TAG, "changed %s to %d", options[option_index].key, value);

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

    switch(options[pos].type){
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

static esp_err_t log_get_handler(httpd_req_t *req){
    web_config_data_t *data = (web_config_data_t *) req->user_ctx;

    // open log
    FILE *fd = logger_get_file();
    if(fd == NULL){
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Could not open log file");
        return ESP_FAIL;
    }

    fpos_t orig_pos;
    ESP_LOGI(TAG, "getting position");
    if(fgetpos(fd, &orig_pos) != 0){
        ESP_LOGE(TAG, "cannot get position of log file; %s", strerror(errno));
        return false;
    }

    ESP_LOGI(TAG, "rewinding");
    fseek(fd, 0, SEEK_SET);

    httpd_resp_set_type(req, "text/plain");

    char *chunk = data->scratch_buf;
    size_t chunksize;
    do{
        chunksize = fread(chunk, 1, SCRATCH_BUFSIZE, fd);
        if(chunksize == 0){
            break;
        }
        esp_err_t err = httpd_resp_send_chunk(req, chunk, chunksize);
        if(err != ESP_OK){
            ESP_LOGI(TAG, "setting position");
            if(fsetpos(fd, &orig_pos) != 0){
                ESP_LOGE(TAG, "cannot set position of log file; %s", strerror(errno));
            }
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to read the log");
            return ESP_FAIL;
        }
    } while(chunksize >= SCRATCH_BUFSIZE);

    ESP_LOGI(TAG, "setting position");
    if(fsetpos(fd, &orig_pos) != 0){
        ESP_LOGE(TAG, "cannot set position of log file; %s", strerror(errno));
        return ESP_FAIL;
    }

    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

static esp_err_t reboot_get_handler(httpd_req_t *req){
    web_config_data_t *data = (web_config_data_t *) req->user_ctx;

    httpd_resp_sendstr(req, "Done");
    web_config_stop(data->server);
    ESP_LOGI(TAG, "restarting");
    esp_restart();

    return ESP_OK;
}

static esp_err_t ota_post_handler(httpd_req_t *req){
    static char url[128];
    web_config_data_t *data = (web_config_data_t *) req->user_ctx;
    
    // load url from POST body
    long int received = http_load_post_req_to_buf(req, data->scratch_buf, sizeof(data->scratch_buf));
    if(received < 0){
        return ESP_FAIL;
    }

    strlcpy(url, data->scratch_buf, sizeof(url));

    // validate URL
    int reg_err = regexec(&data->url_regex, url, 0, NULL, 0);
    if (reg_err != 0) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed validate URL");
        return ESP_FAIL;
    }

    esp_https_ota_config_t ota_config = ota_config_default(url);

    esp_err_t err = ota_start(&ota_config);
    if(err != ESP_OK){
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed OTA update");
        return ESP_FAIL;
    }

    httpd_resp_sendstr(req, "Done");
    return ESP_OK;
}

static void web_config_register_uri(httpd_handle_t server, web_config_data_t *user_ctx)
{
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

    const httpd_uri_t log_download = {
        .uri = API_PATH("/logs"),
        .method = HTTP_GET,
        .handler = log_get_handler,
        .user_ctx = user_ctx};
    httpd_register_uri_handler(server, &log_download);

    const httpd_uri_t reboot = {
        .uri = API_PATH("/reboot"),
        .method = HTTP_GET,
        .handler = reboot_get_handler,
        .user_ctx = user_ctx};
    httpd_register_uri_handler(server, &reboot);

    const httpd_uri_t ota = {
        .uri = API_PATH("/ota"),
        .method = HTTP_POST,
        .handler = ota_post_handler,
        .user_ctx = user_ctx};
    httpd_register_uri_handler(server, &ota);
}

static esp_err_t prepare_url_regex(web_config_data_t *data){
    int err = regcomp(&data->url_regex, "https?:\\/\\/(www\\.)?[-a-zA-Z0-9@:%._\\+~#=]{1,256}\\.[a-zA-Z0-9()]{1,6}\\b([-a-zA-Z0-9()@:%_\\+.~#?&//=]*)", REG_EXTENDED);
    if(err != 0){
        return ESP_FAIL;
    }
    return ESP_OK;
}

static httpd_handle_t start_webserver(void)
{
    static web_config_data_t data;

    prepare_url_regex(&data);

    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    // allow wildcard matching
    config.uri_match_fn = httpd_uri_match_wildcard;

    // Start the httpd server
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        data.server = server;
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

static esp_err_t wifi_connect_nvs(){
    nvs_handle_t handle;

    if(nvs_open("config", NVS_READWRITE, &handle) != ESP_OK){
        ESP_LOGI(TAG, "Error opening NVS!");
        return ESP_FAIL;
    }
    char ssid[MAX_SSID_LEN];
    size_t ssid_len = sizeof(ssid) / sizeof(ssid[0]);
    char password[MAX_PASSPHRASE_LEN];
    size_t password_len = sizeof(password) / sizeof(password[0]);

    esp_err_t err = nvs_get_str(handle, STA_SSID_KEY, ssid, &ssid_len);
    if(err != ESP_OK){
        return ESP_FAIL;
    }

    err = nvs_get_str(handle, STA_PASSWORD_KEY, password, &password_len);
    if(err != ESP_OK){
        return ESP_FAIL;
    }

    return wifi_connect_simple(ssid, password);
}

static esp_err_t wifi_ap_nvs(){
    nvs_handle_t handle;

    if(nvs_open("config", NVS_READWRITE, &handle) != ESP_OK){
        ESP_LOGI(TAG, "Error opening NVS!");
        return ESP_FAIL;
    }
    char ssid[MAX_SSID_LEN];
    size_t ssid_len = sizeof(ssid) / sizeof(ssid[0]);
    char password[MAX_PASSPHRASE_LEN];
    size_t password_len = sizeof(password) / sizeof(password[0]);
    int16_t channel = 0;

    esp_err_t err = nvs_get_str(handle, AP_SSID_KEY, ssid, &ssid_len);
    if(err != ESP_OK){
        return ESP_FAIL;
    }

    err = nvs_get_str(handle, AP_PASSWORD_KEY, password, &password_len);
    if(err != ESP_OK){
        return ESP_FAIL;
    }

    err = nvs_get_str(handle, AP_CHANNEL_KEY, password, &password_len);
    if(err != ESP_OK){
        return ESP_FAIL;
    }

    wifi_init_ap_simple(ssid, password, channel);
    return ESP_OK;
}

static esp_err_t wifi_initialize(){
    wifi_start();

    // connect to preconfigured wifi if available
    esp_err_t ret = wifi_connect_nvs();
    if(ret == ESP_OK){
        return ESP_OK;
    }
    
    // otherwise try to connect to firmware default
    ret = wifi_connect_default();
    if(ret == ESP_OK){
        return ESP_OK;
    }
    
    // create AP from NVS values
    ret = wifi_ap_nvs();
    if(ret == ESP_OK){
        return ESP_OK;
    }

    // otherwise create firmware default AP
    wifi_init_ap_default();

    return ESP_OK;
}

static void init_logging(){
    logger_init(ESP_LOG_INFO);
    logger_output_to_default();
    logger_init_storage();
    const char* filename = "/logs/log.txt";
    logger_output_to_file(filename);
}

static void deinit_logging(){
    logger_sync_file();
    logger_stop();
}

void web_config_stop(httpd_handle_t server){
    ESP_LOGI(TAG, "Stopping server");
    // stop_webserver(server);
    deinit_logging();
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

    init_logging();
    LOGGER_I(TAG, "Booted to Web config");

    server = start_webserver();
}
