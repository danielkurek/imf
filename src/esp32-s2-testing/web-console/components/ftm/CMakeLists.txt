idf_component_register(SRCS "ftm.c"
                    INCLUDE_DIRS "include"
                    PRIV_REQUIRES nvs_flash cmd_system
                    EMBED_FILES ftm_control.html)
target_compile_options(${COMPONENT_LIB} PRIVATE "-Wno-format")