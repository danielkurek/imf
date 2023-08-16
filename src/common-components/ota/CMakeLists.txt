idf_component_register(SRCS "ota.c"
                    INCLUDE_DIRS "include"
                    REQUIRES esp_https_ota esp_partition
                    PRIV_REQUIRES "wifi_connect" esp_event app_update esp_http_client nvs_flash esp_netif esp_wifi
                    EMBED_TXTFILES ${project_dir}/server_certs/ca_cert.pem)
