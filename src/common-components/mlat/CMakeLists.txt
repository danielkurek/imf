idf_component_register(SRCS "mlat.cpp"
                    INCLUDE_DIRS "include"
                    REQUIRES eigen)
# esp-idf-cxx
target_compile_options(${COMPONENT_LIB} PRIVATE -std=gnu++23)