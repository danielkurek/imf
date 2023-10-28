idf_component_register(SRCS "main.cpp"
                    INCLUDE_DIRS "."
                    REQUIRES distance_meter wifi_connect driver nvs_flash)

target_compile_options(${COMPONENT_LIB} PRIVATE -std=gnu++23)