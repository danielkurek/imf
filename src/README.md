# Source code of Interactive mesh framework

## Documentation

This project uses Doxygen code documentation. To generate documentation run the following command.

```bash
doxygen
```

Alternatively run the following command that specifies configuration.

```bash
doxygen Doxyfile
```


## File structure

- `ble-mesh/` - Bluetooth mesh related code
  - `common-components/` - common components for Bluetooth mesh
  - `mesh-firmware/` - firmware for Bluetooth module that communicates with Bluetooth mesh
- `common-components/` - main source code of this project
- `example-project/` - minimal example project that can be used for starting new project using this framework
- `finding-color/` - example projects
  - `finding-color-mobile/` - project for mobile device
  - `finding-color-station/` - project for station device
- `ftm-testing/` - application used for collection of WiFi FTM measurements for distance analysis
- `scripts/` - shell scripts that automates flashing and monitoring devices (only for Linux with tmux installed)
- `Doxyfile` - Doxygen configuration