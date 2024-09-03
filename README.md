# TecnoFly remote controller

# Controller for electrical hydrofoil assist system

Basically it's a wireless remote controller for any motor conneted to an ESC motor control. It was born for hydrofoil motor assist, but it cold be used for electrical skate, tow boogie an others device.

The protocol used for the transmition data is ESP-NOW and LoRa.

transmitter based on ESP8266 D1 Mini

receiver based on ESP32 WROOM 32U

Output of the receiver is PPM signal for VESC 75100 and serial data from it are managed.

VESC data are stored on SDCARD of the reiceiver and showed on trasmitter's display

The receiver module generates a wifi hot spot where is possible to connect by a browser for data analyzing and settings

The Lora module is optional in the receiver and transimetter. The firmware auto detect and use LoRa protocoll if the module is connected to the ESP. The trigger value from hall sensor could be sent by the LoRa channel in parallel to the ESP-NOW communication. LoRa transmition improves the performance of the transmition datas in the water

