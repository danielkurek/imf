# Interactive mesh framework (IMF)

Framework for interactive distributed network of luminous devices. Devices are based on ESP32-S3.

To initialize repository run the following command. Some external libraries are added as git submodule.
```bash
git submodule update --init --recursive
```

## Folder structure

- `boards` - source files for devices designed for this framework (official dev modules can also be used)
- `distance-analysis` - analysis of WiFi FTM distance measurement (with measured data)
- `mesh-visualizer` - helper application for visualization of positions of devices and control states of devices through Bluetooth mesh
- `result` - results from testing of the framework on example application
- `src` - source code of the framework