idf_component_register(SRCS "ble_mesh.c"
                    INCLUDE_DIRS "include"
                    REQUIRES board color rgb_control interactive-mesh-framework
                    PRIV_REQUIRES logger bt example_init example_nvs)