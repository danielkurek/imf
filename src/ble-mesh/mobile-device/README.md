ESP BLE Mesh Client Model Demo
========================

This demo shows how to use the Generic OnOff Client Model to get/set the generic on/off state.

Connect button to GP1 (pull-up configuration)

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
 - expand item in Elements
 - click on Generic On Off Client
   - click on Bind Key (this will make it functional)
     - select the only Application key
 - now everything is setup and when pushing the connected button all servers should lit up red
