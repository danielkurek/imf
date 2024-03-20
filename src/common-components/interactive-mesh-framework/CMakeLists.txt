idf_component_register(SRCS "mlat_localization.cpp" "graph_localization.cpp" "interactive-mesh-framework.cpp" "location_common.c"
                    INCLUDE_DIRS "include"
                    REQUIRES board distance_meter logger web_config serial_comm color esp_event nvs_flash graphviz
                    PRIV_REQUIRES wifi_connect mlat)