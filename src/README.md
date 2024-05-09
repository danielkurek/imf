# Source code of Interactive mesh framework

To initialize repository run the following command. Some external libraries are added as git submodule.
```bash
git submodule update --init --recursive
```

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
- `scripts/` - shell scripts that automates flashing and monitoring devices (only for Linux with tmux installed)
- `Doxyfile` - Doxygen configuration

## Installing ESP-IDF

You can refer to [official installation guide](https://docs.espressif.com/projects/esp-idf/en/v5.1.3/esp32/get-started/index.html#installation) or follow the guide bellow.

This guide will focus on installing ESP-IDF through VSCode extension. However, you can also install it through an [separate installer](https://dl.espressif.com/dl/esp-idf/) but it will not set up any IDE (make sure to download the correct version).

### VSCode extension

[Official guide is in github repository of this extension](https://github.com/espressif/vscode-esp-idf-extension/blob/master/docs/tutorial/install.md).

1. In VSCode download the extension "Espressif IDF" (id: `espressif.esp-idf-extension`)
2. Install required prerequisites
3. Click on `View`, then `Command Palette`; or press `CTRL+SHIFT+P`
4. Type `configure esp-idf extension` and click on `ESP-IDF: Configure ESP-IDF extension`. New tab will open with configuration screen.
5. Select `Express` option
6. Select ESP-IDF version v5.1.3 (or other v5.1.x version)
7. Click install

## Project compilation

1. Open project folder in VSCode (`finding-color/finding-color-mobile` or `finding-color/finding-color-station`)
2. Click on `View`, then `Command Palette`; or press `CTRL+SHIFT+P` and select `ESP-IDF: Set Espressif device target`
3. Select your device's chip (ESP32S3)
4. Click on `View`, then `Command Palette`; or press `CTRL+SHIFT+P` and select `ESP-IDF: Build your project`
5. Compilation will begin and the project will be build to folder `build`

## Upload project to device

1. Open project folder in VSCode (`finding-color/finding-color-mobile` or `finding-color/finding-color-station`)
2. Click on `View`, then `Command Palette`; or press `CTRL+SHIFT+P` and select `ESP-IDF: Set Espressif device target`
3. Select your device's chip (ESP32C3)
4. Connect device to computer
5. Click on `View`, then `Command Palette`; or press `CTRL+SHIFT+P` and select `ESP-IDF: Select port to use (COM, tty, usbserial)`
6. Select port that corresponds to the device
7. Click on `View`, then `Command Palette`; or press `CTRL+SHIFT+P` and select `ESP-IDF: Select Flash Method`
8. Select `UART` from the list
9. Click on `View`, then `Command Palette`; or press `CTRL+SHIFT+P` and select `ESP-IDF: Build, Flash and start a monitor on your device`
10. Compilation will begin and the project will be build to folder `build`, then it will flash it to the device and start a monitor that displays log messages from the device

If during flashing it has a problem connecting to the device, make sure that you have closed all Serial monitors. It does not work when something is listening to the same port as we are using for flashing.

If we need to upload new version of the project, we need to repeat only step 9 and 10. Logs and configuration that is stored on the device will persist. If we would want to to erase it, we need to erase the whole flash chip and upload the project again on the device. 

We can erase the flash chip using the following guide.

1. Click on `View`, then `Command Palette`; or press `CTRL+SHIFT+P` and select `ESP-IDF: Open ESP-IDF Terminal`
2. Execute the following command in the terminal: `idf.py erase-flash --port <PORT>`
   - where we replace `<PORT>` with the port that corresponds to the device (same as the port for uploading)
   - if it has a problem connecting to the device, make sure that you have closed all Serial monitors

## Configure project

If we want to change settings of ESP-IDF or our components, we can Click on `View`, then `Command Palette`; or press `CTRL+SHIFT+P` and open `ESP-IDF: SDK Configuration editor`. Navigate to section `Interactive Mesh Framework` and change `Mesh Role` according to your need. In this section you can also find configuration of GPIO Pins used for Serial communication between modules.