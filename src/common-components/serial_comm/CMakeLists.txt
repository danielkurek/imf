idf_component_register(SRCS "serial_comm_client.cpp" "serial_comm_server.cpp" "serial_comm_common.cpp"
                    INCLUDE_DIRS "include"
                    PRIV_REQUIRES driver)
# esp-idf-cxx
target_compile_options(${COMPONENT_LIB} PRIVATE -std=gnu++23)