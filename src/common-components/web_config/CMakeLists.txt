idf_component_register(SRCS "http_common.c" "web_config.c"
                    INCLUDE_DIRS "include"
                    REQUIRES nvs_flash
                    PRIV_REQUIRES "wifi_connect" logger esp_event esp_netif esp_wifi esp_http_server json
                    EMBED_TXTFILES ${CMAKE_CURRENT_SOURCE_DIR}/web/index.html)
