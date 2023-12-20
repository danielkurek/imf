idf_component_register(SRCS "main.cpp"
                    INCLUDE_DIRS "."
                    REQUIRES interactive-mesh-framework esp_event driver nvs_flash)

target_compile_options(${COMPONENT_LIB} PRIVATE -std=gnu++23)