# TecnoFly remote controller

# Controller for electrical hydrofoil assist system

Basically it's a wireless remote controller for any motor conneted to an ESC motor control. It was born for hydrofoil motor assist, but it cold be used for electrical skate, tow boogie an others device.

The protocol used for the transmition data is ESP-NOW and LoRa.

ESP-NOW operates as a peer-to-peer protocol, meaning it allows direct communication between two devices. Each ESP device has a unique MAC address which is used to identify the receiving board. LoRa is a wireless modulation technique derived from Chirp Spread Spectrum technology. LoRa modulated transmission is robust against disturbances and can be received across great distances.

The transmitter is based on ESP8266 D1 Mini, the receiver is based on ESP32 WROOM 32U

Output of the receiver is PPM signal for VESC 75100 and serial datas from it are managed.

VESC data are stored on SDCARD of the reiceiver and showed on trasmitter's display

The receiver module generates a wifi hot spot where is possible to connect by a browser for data analyzing and settings

The Lora module is optional in the receiver and transimetter. The firmware auto detect and use LoRa protocoll if the module is connected to the ESP. The trigger value from hall sensor could be sent by the LoRa channel in parallel to the ESP-NOW communication. LoRa transmition improves the performance of the transmition datas in the water

