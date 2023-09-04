set(srcs "main.c"
         "board.c")

idf_component_register(SRCS "${srcs}"
                    INCLUDE_DIRS  "."
                    PRIV_REQUIRES "rgb_control" color web_config logger button nvs_flash bt driver color example_init example_nvs)
