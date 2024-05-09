# Example project of IMF

If you are using this project as starting point to create IMF application follow these steps:

- Open `CMakeLists.txt`
  - update path to IMF
    - update path on line `list(APPEND EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/../common-components/")` 
  - change project name on line `project(example-projet)`
- Change default configuration in `sdkconfig.defaults`
  - choose device type
    - this project is configured for Station device
    - to change it comment line with `CONFIG_IMF_STATION_DEVICE=y` and uncomment line `CONFIG_IMF_MOBILE_DEVICE=y`
  - make sure that pins for serial communication are valid
    - lines `CONFIG_IMF_SERIAL_TX_GPIO=42` and `CONFIG_IMF_SERIAL_RX_GPIO=41`
- write your application in file `main/main.cpp`