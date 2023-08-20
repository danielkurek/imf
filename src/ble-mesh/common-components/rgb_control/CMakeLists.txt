idf_component_register(SRCS "rgb_control_server.c" "rgb_control_client.c"
                    INCLUDE_DIRS "include"
                    PRIV_REQUIRES "color" bt)