set(EXTRA_COMPONENT_DIRS $ENV{IDF_PATH}/examples/system/console/advanced/components)

idf_component_register(SRCS "ftm.c"
                    INCLUDE_DIRS "include")
