// Based on protocol example from esp-idf

#include <string.h>
#include "esp_err.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "esp_mac.h"

static const char *TAG = "WiCON";
static esp_netif_t *s_sta_netif = NULL;
static SemaphoreHandle_t s_semph_get_ip_addrs = NULL;

#define WIFI_CONNECT_NETIF_DESC_STA "wifi_connect_netif_sta"

#if CONFIG_WIFI_CONNECT_SCAN_METHOD_FAST
#define WIFI_CONNECT_SCAN_METHOD WIFI_FAST_SCAN
#elif CONFIG_WIFI_CONNECT_SCAN_METHOD_ALL_CHANNEL
#define WIFI_CONNECT_SCAN_METHOD WIFI_ALL_CHANNEL_SCAN
#endif

#if CONFIG_WIFI_CONNECT_AP_BY_SIGNAL
#define WIFI_CONNECT_AP_SORT_METHOD WIFI_CONNECT_AP_BY_SIGNAL
#elif CONFIG_WIFI_CONNECT_AP_BY_SECURITY
#define WIFI_CONNECT_AP_SORT_METHOD WIFI_CONNECT_AP_BY_SECURITY
#endif

#if CONFIG_WIFI_CONNECT_AUTH_OPEN
#define WIFI_CONNECT_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_OPEN
#elif CONFIG_WIFI_CONNECT_AUTH_WEP
#define WIFI_CONNECT_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WEP
#elif CONFIG_WIFI_CONNECT_AUTH_WPA_PSK
#define WIFI_CONNECT_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA_PSK
#elif CONFIG_WIFI_CONNECT_AUTH_WPA2_PSK
#define WIFI_CONNECT_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_PSK
#elif CONFIG_WIFI_CONNECT_AUTH_WPA_WPA2_PSK
#define WIFI_CONNECT_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA_WPA2_PSK
#elif CONFIG_WIFI_CONNECT_AUTH_WPA2_ENTERPRISE
#define WIFI_CONNECT_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_ENTERPRISE
#elif CONFIG_WIFI_CONNECT_AUTH_WPA3_PSK
#define WIFI_CONNECT_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA3_PSK
#elif CONFIG_WIFI_CONNECT_AUTH_WPA2_WPA3_PSK
#define WIFI_CONNECT_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_WPA3_PSK
#elif CONFIG_WIFI_CONNECT_AUTH_WAPI_PSK
#define WIFI_CONNECT_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WAPI_PSK
#endif

static int s_retry_num = 0;

wifi_config_t wifi_sta_config_default() {
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = {},
            .password = {},
            .scan_method = WIFI_CONNECT_SCAN_METHOD,
            .sort_method = WIFI_CONNECT_AP_SORT_METHOD,
            .threshold = {
                .rssi = CONFIG_WIFI_CONNECT_SCAN_RSSI_THRESHOLD,
                .authmode = WIFI_CONNECT_SCAN_AUTH_MODE_THRESHOLD
            }
        },
    };
    return wifi_config;
}

static bool is_our_netif(const char *prefix, esp_netif_t *netif)
{
    return strncmp(prefix, esp_netif_get_desc(netif), strlen(prefix) - 1) == 0;
}

static void handler_on_wifi_disconnect(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    s_retry_num++;
    if (s_retry_num > CONFIG_WIFI_CONN_MAX_RETRY) {
        ESP_LOGI(TAG, "WiFi Connect failed %d times, stop reconnect.", s_retry_num);
        if (s_semph_get_ip_addrs) {
            xSemaphoreGive(s_semph_get_ip_addrs);
        }
        return;
    }
    ESP_LOGI(TAG, "Wi-Fi disconnected, trying to reconnect...");
    esp_err_t err = esp_wifi_connect();
    if (err == ESP_ERR_WIFI_NOT_STARTED) {
        return;
    }
    ESP_ERROR_CHECK(err);
}


static void handler_on_sta_got_ip(void *arg, esp_event_base_t event_base,
                      int32_t event_id, void *event_data)
{
    s_retry_num = 0;
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    if (!is_our_netif(WIFI_CONNECT_NETIF_DESC_STA, event->esp_netif)) {
        return;
    }
    ESP_LOGI(TAG, "Got IPv4 event: Interface \"%s\" address: " IPSTR, esp_netif_get_desc(event->esp_netif), IP2STR(&event->ip_info.ip));
    if (s_semph_get_ip_addrs) {
        xSemaphoreGive(s_semph_get_ip_addrs);
    } else {
        ESP_LOGI(TAG, "- IPv4 address: " IPSTR ",", IP2STR(&event->ip_info.ip));
    }
}



void wifi_start(void)
{
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_netif_inherent_config_t esp_netif_config = ESP_NETIF_INHERENT_DEFAULT_WIFI_STA();
    // Warning: the interface desc is used in tests to capture actual connection details (IP, gw, mask)
    esp_netif_config.if_desc = WIFI_CONNECT_NETIF_DESC_STA;
    esp_netif_config.route_prio = 128;
    s_sta_netif = esp_netif_create_wifi(WIFI_IF_STA, &esp_netif_config);
    esp_wifi_set_default_wifi_sta_handlers();

    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
}


void wifi_stop(void)
{
    esp_err_t err = esp_wifi_stop();
    if (err == ESP_ERR_WIFI_NOT_INIT) {
        return;
    }
    ESP_ERROR_CHECK(err);
    ESP_ERROR_CHECK(esp_wifi_deinit());
    ESP_ERROR_CHECK(esp_wifi_clear_default_wifi_driver_and_handlers(s_sta_netif));
    esp_netif_destroy(s_sta_netif);
    s_sta_netif = NULL;
}


esp_err_t wifi_sta_do_connect(wifi_config_t wifi_config, bool wait)
{
    if (wait) {
        s_semph_get_ip_addrs = xSemaphoreCreateBinary();
        if (s_semph_get_ip_addrs == NULL) {
            return ESP_ERR_NO_MEM;
        }
    }
    s_retry_num = 0;
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &handler_on_wifi_disconnect, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &handler_on_sta_got_ip, NULL));

    ESP_LOGI(TAG, "Connecting to %s...", wifi_config.sta.ssid);
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    esp_err_t ret = esp_wifi_connect();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WiFi connect failed! ret:%x", ret);
        return ret;
    }
    if (wait) {
        ESP_LOGI(TAG, "Waiting for IP(s)");
        xSemaphoreTake(s_semph_get_ip_addrs, portMAX_DELAY);
        if (s_retry_num > CONFIG_WIFI_CONN_MAX_RETRY) {
            return ESP_FAIL;
        }
    }
    return ESP_OK;
}

esp_err_t wifi_sta_do_disconnect(void)
{
    ESP_ERROR_CHECK(esp_event_handler_unregister(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &handler_on_wifi_disconnect));
    ESP_ERROR_CHECK(esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, &handler_on_sta_got_ip));
    if (s_semph_get_ip_addrs) {
        vSemaphoreDelete(s_semph_get_ip_addrs);
    }
    return esp_wifi_disconnect();
}

void wifi_shutdown(void)
{
    wifi_sta_do_disconnect();
    wifi_stop();
}

esp_err_t wifi_connect(wifi_config_t wifi_config)
{
    ESP_LOGI(TAG, "Start wifi_connect.");
    return wifi_sta_do_connect(wifi_config, true);
}

esp_err_t wifi_connect_default(void)
{
    ESP_LOGI(TAG, "Start wifi_connect_default.");
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = CONFIG_WIFI_CONNECT_SSID,
            .password = CONFIG_WIFI_CONNECT_PASSWORD,
            .scan_method = WIFI_CONNECT_SCAN_METHOD,
            .sort_method = WIFI_CONNECT_AP_SORT_METHOD,
            .threshold = {
                .rssi = CONFIG_WIFI_CONNECT_SCAN_RSSI_THRESHOLD,
                .authmode = WIFI_CONNECT_SCAN_AUTH_MODE_THRESHOLD
            }
        },
    };
    return wifi_connect(wifi_config);
}

esp_err_t wifi_connect_simple(const char* ssid, const char* password){
    wifi_config_t wifi_config = wifi_sta_config_default();

    strlcpy((char*) wifi_config.sta.ssid, ssid, MAX_SSID_LEN);
    strlcpy((char*) wifi_config.sta.password, password, MAX_PASSPHRASE_LEN);

    return wifi_connect(wifi_config);
}

esp_err_t wifi_init_ap(wifi_config_t wifi_config)
{
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_err_t err;
    err = esp_wifi_init(&cfg);
    if(err != ESP_OK){
        return err;
    }

    err = esp_wifi_set_storage(WIFI_STORAGE_RAM);
    if(err != ESP_OK){
        return err;
    }
    err = esp_wifi_set_mode(WIFI_MODE_AP);
    if(err != ESP_OK){
        return err;
    }
    err = esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config);
    if(err != ESP_OK){
        return err;
    }
    err = esp_wifi_start();
    if(err != ESP_OK){
        return err;
    }

    ESP_LOGI(TAG, "wifi_init_softap finished. SSID:%s password:%s channel:%d",
             wifi_config.ap.ssid, wifi_config.ap.password, wifi_config.ap.channel);
    return err;
}

esp_err_t wifi_init_ap_simple(const char* ssid, const char* password, uint8_t channel){
    wifi_config_t wifi_config = {
        .ap = {
            .ssid = {},
            .ssid_len = strlen(ssid),
            .channel = channel,
            .password = {},
            .max_connection = CONFIG_WIFI_CONNECT_AP_DEFAULT_MAX_CONNECTION,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK,
            .ftm_responder = true
        },
    };
    strlcpy((char*) wifi_config.ap.ssid, ssid, MAX_SSID_LEN);
    strlcpy((char*) wifi_config.ap.password, password, MAX_PASSPHRASE_LEN);

    if (strlen(password) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }
    return wifi_init_ap(wifi_config);
}

esp_err_t wifi_init_ap_default(){
    int64_t mac_addr = 0LL;
    esp_read_mac((uint8_t*) (&mac_addr), ESP_MAC_WIFI_SOFTAP);
    
    char ssid[MAX_SSID_LEN];
    snprintf(ssid, MAX_SSID_LEN, "NODE-%llX", mac_addr);
    return wifi_init_ap_simple(ssid, CONFIG_WIFI_CONNECT_AP_DEFAULT_PASSWORD, CONFIG_WIFI_CONNECT_AP_DEFAULT_CHANNEL);
}