idf_component_register(SRCS "ftm.c"
                    INCLUDE_DIRS "include"
                    PRIV_REQUIRES nvs_flash cmd_system)
target_compile_options(${COMPONENT_LIB} PRIVATE "-Wno-format")