set(EXTRA_COMPONENT_DIRS $ENV{IDF_PATH}/examples/system/console/advanced/components)

idf_component_register(SRCS "ftm.c"
                    INCLUDE_DIRS "include"
                    PRIV_REQUIRES nvs_flash cmd_system)
target_compile_options(${COMPONENT_LIB} PRIVATE "-Wno-format")