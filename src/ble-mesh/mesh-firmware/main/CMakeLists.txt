set(srcs "main.c")

idf_component_register(SRCS "${srcs}"
                    INCLUDE_DIRS  "."
                    PRIV_REQUIRES "rgb_control" color web_config logger nvs_flash bt driver color example_init example_nvs)
