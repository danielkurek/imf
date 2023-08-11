idf_component_register(SRCS "web_config.c"
                    INCLUDE_DIRS "include"
                    PRIV_REQUIRES "wifi_connect" esp_event nvs_flash esp_netif esp_wifi esp_http_server)
