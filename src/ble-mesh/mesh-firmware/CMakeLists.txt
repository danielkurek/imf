# The following lines of boilerplate have to be in your project's CMakeLists
# in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS $ENV{IDF_PATH}/examples/bluetooth/esp_ble_mesh/common_components/example_init
                         $ENV{IDF_PATH}/examples/bluetooth/esp_ble_mesh/common_components/example_nvs
                         ${CMAKE_CURRENT_SOURCE_DIR}/../common-components
                         ${CMAKE_CURRENT_SOURCE_DIR}/../../common-components)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(mesh-firmware)
