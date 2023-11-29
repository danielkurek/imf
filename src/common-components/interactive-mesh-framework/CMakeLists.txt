idf_component_register(SRCS "interactive-mesh-framework.cpp"
                    INCLUDE_DIRS "include"
                    REQUIRES board distance_meter logger web_config serial_comm color esp_event
                    PRIV_REQUIRES wifi_connect)