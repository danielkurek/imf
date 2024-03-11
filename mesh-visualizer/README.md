# Mesh visualizer

Connect ESP32-C3 (or other bluetooth capable ESP32) with [mesh-firmware](src/ble-mesh/mesh-firmware) to USB-UART converter according to the following table:

| ESP32 pins | USB-UART |
|------------|----------|
| GPIO1      | TX       |
| GPIO10     | RX       |
| GND        | GND      |
| +5V        | +5V      |

## Usage

```bash
python visualizer.py <serial-port>
```

Replace `<serial-port>` with serial port of the USB-UART converter (e.g. `/dev/ttyUSB0` on linux)