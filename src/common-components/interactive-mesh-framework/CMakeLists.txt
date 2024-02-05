idf_component_register(SRCS "interactive-mesh-framework.cpp" "location_common.c" "location_topology.cpp"
                    INCLUDE_DIRS "include"
                    REQUIRES board distance_meter logger web_config serial_comm color esp_event nvs_flash graphviz
                    PRIV_REQUIRES wifi_connect)