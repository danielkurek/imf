ESP BLE Mesh Node demo
==========================

This demo shows how BLE Mesh device can be set up as a node with the following features:

- One element
- Two SIG models
	- **Configuration Server model**: The role of this model is mainly to configure Provisioner device’s AppKey and set up its relay function, TTL size, subscription, etc.
   - **OnOff Server model**: This model implements the most basic function of turning the lights on and off.

The default purpose of this demo is to enable the advertising function with 20-ms non-connectable interval in BLE 5.0. You can disable this function through menuconfig: `idf.py menuconfig --> Example Configuration --> This option facilitates sending with 20ms non-connectable interval...`

For demostration, the internal LED of ESP32C3 is used.

# HOW TO

 - download nRF Mesh app
 - open the app
 - flash the code to ESP32C3
 - LED should be green when the board starts up
 - read the Bluetooth MAC from console (for the ESP32C3 device)
 - in the nRF Mesh app click on add node
 - select the device with the MAC address that we read
 - we can change the App Keys parameter (default is ok)
 - click on Identify
 - click on Provision
 - LED should not go off
 - wait for the provisioning to finish
 - click on the latest device (should have 4 models)
 - expand all items in Elements
 - click on each Generic On Off Server (each item corresponds to one color: RED, GREEN, BLUE)
   - click on Bind Key (this will make it functional)
     - select the only Application key
   - we can control the LED color by the buttons at the bottom of the screen
     - READ STATE - reads the state if it is UNKNOWN (or updates it)
     - ON/OFF - turns on/off (one button, changes labels)
