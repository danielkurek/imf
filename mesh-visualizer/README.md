# Mesh visualizer

Connect ESP32-S3 (or other bluetooth capable ESP32) with [mesh-firmware](src/ble-mesh/mesh-firmware) to USB-UART converter according to the following table:

| ESP32 pins | USB-UART |
|------------|----------|
| GPIO41     | TX       |
| GPIO42     | RX       |
| GND        | GND      |
| VSYS       | +5V      |

## Usage

Install required libraries from `requirements.txt`.

```bash
python pyside_visualizer.py
```

Click on setting icon and select serial port of connected device and apply settings. Click on connect button to start the program. Nodes will appear in the main part of the window (with the grid). You can move around the grid using your mouse. Click and drag on empty space of the grid will move the grid (or using scroll bars). Using scroll wheel you can zoom in/out. Click and drag on a node moves the node and send the new coordinate to the node.

By clicking buttons `flip x` or `flip y` axis of the visualizer will be flipped. This will not effect positions stored on the device.

Using the right side of the main window we can change properties of the nodes. Select from the dropdown which node we will send the commands to. Then we can change individual properties. Each property needs to applied separately by clicking button next to the field.