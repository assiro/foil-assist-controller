# TecnoFly remote controller

# Controller for electrical hydrofoil assist system

Basically it's a wireless remote controller for any motor conneted to an ESC motor control. It was born for hydrofoiling motor assist, but it cold be used for electrical skate an others device.

The protocol used for the transmition data is ESP-NOW

transmitter based on ESP8266 D1 Mini

receiver based on ESP32 WROOM

Output of the receiver is PPM signal for VESC 75100 and serial data form it are managed.

VESC data are stored on SDCARD of the reiceiver and showed on trasmitter's display

The receiver module generates a wifi hot spot where is possible to connect by a browser for data analyzing and settings

The Lora module is optional in the receiver and transimetter. The firmware auto detect and use LoRa protocoll if the module is connected to the ESP. The trigger value from hall sensor could be sent by the LoRa channel in parallel to the ESP-NOW communication. LoRa transmition could improve the performance of the transmitions in the water

