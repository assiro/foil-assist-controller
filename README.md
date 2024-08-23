# Controller for electrical hydrofoil assist system

The protocol used for the transmition data is ESP-NOW

transmitter based on ESP8266 D1 Mini

receiver based on ESP32 WROOM

Output of the receiver is PPM signal for VESC 75100

VESC data are stored on SDCARD of the reiceiver and showed on trasmitter's display

The receiver module generates a wifi hot spot where is possible to connect by a browser for data analyze and settings

The Lora module is optional in the recever and transimetter. The firmware auto detect and use LoRa protocoll ift te module is connected to the ESP, and the trigger value could be sent by the channel in parallel. This could increare the performance of the transmitions in the water

