set(srcs "main.c")

idf_component_register(SRCS "${srcs}"
                    INCLUDE_DIRS  "."
                    PRIV_REQUIRES ble_mesh board web_config logger nvs_flash driver)
