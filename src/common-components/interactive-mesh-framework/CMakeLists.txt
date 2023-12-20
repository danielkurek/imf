idf_component_register(SRCS "interactive-mesh-framework.cpp"
                    INCLUDE_DIRS "include"
                    REQUIRES board distance_meter logger web_config serial_comm color location esp_event nvs_flash
                    PRIV_REQUIRES wifi_connect)