set(srcs "main.cpp")

idf_component_register(SRCS "${srcs}"
                    INCLUDE_DIRS  "."
                    PRIV_REQUIRES interactive-mesh-framework serial_comm ble_mesh board web_config logger nvs_flash driver)
