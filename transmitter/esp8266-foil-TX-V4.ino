/*
    TecnoFly cremote controller for Foil Assist system
    2024 writed by Roberto Assiro
    
    This program is free software under the terms of the GNU General Public License 
    as published by the Free Software Foundation.
    VERSION 4.xx Aug 2024
*/
#include <ESP8266WiFi.h>
#include <espnow.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ESP_EEPROM.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <U8g2lib.h>
#include <ESP8266WiFi.h>
#include <LoRa.h>
#include <Adafruit_SH110X.h>
#include <ESP8266httpUpdate.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266HTTPClient.h>
ESP8266WiFiMulti WiFiMulti;
String VERSION = "4.32";
const char* ssid     = "tecnofly";
const char* password = "tecnofly";
String upgradeServer = "http://www.eoloonline.it/firmware/foilAssist/";
String upgradeFile = "foilAssistController.bin";
//const int ESP_BUILTIN_LED = 2;
const int VIBRATOR = D8;
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 128 // OLED display height, in pixels
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SH1107 display = Adafruit_SH1107(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET, 1000000, 100000);
#define EEPROM_SIZE 16
#define ss D4
#define rst D3
#define dio0 D0
bool firstConnection = false;
uint TXmode = 0; // trasmission mode ESPNOW = 0, Lora 1, BOTH = 2
uint RXmode = 1; // receiver select
uint THmode = 0; // Throttle mode
uint displayMode; // display view mode
bool espStatus = false;
bool loraStatus = false;
bool noLora = false;
byte localAddress;     // address of this device
byte destination;      // destination to send to
unsigned long lastTime;  
unsigned long timerDelay = 100;  // send throttle to receiver
unsigned long vibrationTimeOff;
unsigned long vibrationTime = 0; 
unsigned long timerOff = 0; 
unsigned long timerNoConnect = 0; 
int hallPin = A0;
int hallValue;
int throttle;
int triggerValue;
int counter;
bool lowBatt = false;
bool lock = true;
uint loraCounter;
uint valMin = 0;
uint valMax = 0;
uint valMiddle = ((valMax - valMin) / 2) + valMin;

// data from vesc
struct vescValues {
  float voltage;  
  float tempMosfet;
  float current;
  int power;
};
struct vescValues data;
  long rpm;
  float inpVoltage;  
  float tempMosfet;
  float avgInputCurrent;
  float watt;
  int batpercentage;
  int file;
  int minutes;
  int seconds;
  uint powerCounter;
  uint powerAverage;
  uint powerIndex;
  float receiverVersion;
  char message1[8];
  char message2[8];
  char message3[8];  
  int Tminutes;
  int Tseconds;
  
uint8_t broadcastAddress[] = {0x4C, 0x11, 0xAE, 0x0D, 0xE5, 0x35};// RECEIVER MAC Address
uint8_t newMACAddress[] = {0xA4, 0xCF, 0x12, 0xDC, 0xB5, 0x8E};//TRANSMITTER MAC ADDRESS

// Must match the receiver structure// Structure example to send data
typedef struct struct_message {
  float a;
  float tempMosfet;  
  float avgInputCurrent;  
  int watt;
  int batt;
  int file;
  int minutes;
  int seconds;
  uint powerCounter;
  uint powerAverage;
  uint powerIndex;
  float receiverVersion;
  char message1[8];
  char message2[8];
  char message3[8];  
  int Tminutes;
  int Tseconds;   
} struct_message;

struct myData {
  int a;
};

// Create a struct_message called myData
struct_message myData;
struct_message incomingReadings;

// Callback when data is sent
void OnDataSent(uint8_t *mac_addr, uint8_t sendStatus) {
  if (sendStatus == 0){
//      Serial.println("Delivery success by ESP-NOW");
      espStatus = true;
  }else{
  //  Serial.println("ESP_NOW delivery fail...");
    espStatus = false;
  }
}

// data is received by ESP-NOW// Callback when data is received
void OnDataRecv(uint8_t * mac_addr, uint8_t *incomingData, uint8_t len) {
      memcpy(&incomingReadings, incomingData, sizeof(incomingReadings));
      inpVoltage = incomingReadings.a;
      tempMosfet = incomingReadings.tempMosfet;
      avgInputCurrent = incomingReadings.avgInputCurrent;
      watt = incomingReadings.watt;
      batpercentage = incomingReadings.batt;
      file = incomingReadings.file+1;
      minutes = incomingReadings.minutes;
      seconds = incomingReadings.seconds;      
      powerCounter = incomingReadings.powerCounter;
      powerAverage = incomingReadings.powerAverage;
      powerIndex = incomingReadings.powerIndex;
      receiverVersion = incomingReadings.receiverVersion;
      Tminutes = incomingReadings.Tminutes;
      Tseconds = incomingReadings.Tseconds;      
//Serial.println(incomingReadings.message1);
//Serial.println(incomingReadings.message2);  
      firstConnection = true;
}
 
void setup() {
      Serial.begin(115200);
      ESPhttpUpdate.setClientTimeout(8000);
      WiFi.mode(WIFI_STA);  // Set device as a Wi-Fi Station
//      pinMode(ESP_BUILTIN_LED, OUTPUT);
      pinMode(VIBRATOR, OUTPUT);
      EEPROM.begin(32);
      EEPROM.get(12, RXmode);
      if (RXmode > 9 or RXmode == 0){RXmode = 1;}
      if (RXmode == 1){
            uint8_t Broadcast[] = {0x4C, 0x11, 0xAE, 0x0D, 0xE5, 0x35};
            memcpy(broadcastAddress, Broadcast, sizeof(broadcastAddress));
            uint8_t newMAC[] = {0xA4, 0xCF, 0x12, 0xDC, 0xB5, 0x8E};
            memcpy(newMACAddress, newMAC, sizeof(broadcastAddress));
      }else if (RXmode == 2){
            uint8_t Broadcast[] = {0x4C, 0x11, 0xAE, 0x0D, 0xE5, 0x36};
            memcpy(broadcastAddress, Broadcast, sizeof(broadcastAddress));
            uint8_t newMAC[] = {0xA4, 0xCF, 0x12, 0xDC, 0xB5, 0x8F};
            memcpy(newMACAddress, newMAC, sizeof(broadcastAddress));
      }else if (RXmode == 3){
            uint8_t Broadcast[] = {0x4C, 0x11, 0xAE, 0x0D, 0xE5, 0x37};
            memcpy(broadcastAddress, Broadcast, sizeof(broadcastAddress));
            uint8_t newMAC[] = {0xA4, 0xCF, 0x12, 0xDC, 0xB5, 0x03};
            memcpy(newMACAddress, newMAC, sizeof(broadcastAddress));
      }else if (RXmode == 4){
            uint8_t Broadcast[] = {0x4C, 0x11, 0xAE, 0x0D, 0xE5, 0x38};
            memcpy(broadcastAddress, Broadcast, sizeof(broadcastAddress));
            uint8_t newMAC[] = {0xA4, 0xCF, 0x12, 0xDC, 0xB5, 0x04};
            memcpy(newMACAddress, newMAC, sizeof(broadcastAddress));
      }else if (RXmode == 5){
            uint8_t Broadcast[] = {0x4C, 0x11, 0xAE, 0x0D, 0xE5, 0x39};
            memcpy(broadcastAddress, Broadcast, sizeof(broadcastAddress));
            uint8_t newMAC[] = {0xA4, 0xCF, 0x12, 0xDC, 0xB5, 0x05};
            memcpy(newMACAddress, newMAC, sizeof(broadcastAddress));
      }else if (RXmode == 6){
            uint8_t Broadcast[] = {0x4C, 0x11, 0xAE, 0x0D, 0xE5, 0x40};
            memcpy(broadcastAddress, Broadcast, sizeof(broadcastAddress));
            uint8_t newMAC[] = {0xA4, 0xCF, 0x12, 0xDC, 0xB5, 0x06};
            memcpy(newMACAddress, newMAC, sizeof(broadcastAddress));
      }else if (RXmode == 7){
            uint8_t Broadcast[] = {0x4C, 0x11, 0xAE, 0x0D, 0xE5, 0x41};
            memcpy(broadcastAddress, Broadcast, sizeof(broadcastAddress));
            uint8_t newMAC[] = {0xA4, 0xCF, 0x12, 0xDC, 0xB5, 0x07};
            memcpy(newMACAddress, newMAC, sizeof(broadcastAddress));
      }else if (RXmode == 8){
            uint8_t Broadcast[] = {0x4C, 0x11, 0xAE, 0x0D, 0xE5, 0x42};
            memcpy(broadcastAddress, Broadcast, sizeof(broadcastAddress));
            uint8_t newMAC[] = {0xA4, 0xCF, 0x12, 0xDC, 0xB5, 0x08};
            memcpy(newMACAddress, newMAC, sizeof(broadcastAddress));
      }else if (RXmode == 9){
            uint8_t Broadcast[] = {0x4C, 0x11, 0xAE, 0x0D, 0xE5, 0x43};
            memcpy(broadcastAddress, Broadcast, sizeof(broadcastAddress));
            uint8_t newMAC[] = {0xA4, 0xCF, 0x12, 0xDC, 0xB5, 0x09};
            memcpy(newMACAddress, newMAC, sizeof(broadcastAddress));
      }
      localAddress = newMACAddress[5];//0xBB;     // address of this device
      destination = broadcastAddress[5];//0xAA;      // destination to send to

      Serial.println(newMACAddress[5], HEX);  
      Serial.println(broadcastAddress[5], HEX);  

      Serial.print("MAC:  ");  
      Serial.println(WiFi.macAddress());  
      wifi_set_macaddr(STATION_IF, &newMACAddress[0]);
      //wifi_set_macaddr(SOFTAP_IF, &newMACAddress[0]);
      Serial.print("NEW MAC:  ");
      Serial.println(WiFi.macAddress());
 // Init ESP-NOW
      if (esp_now_init() != 0) {  // Init ESP-NOW
          Serial.println("Error initializing ESP-NOW");
          return;
      }   
      esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
      esp_now_register_send_cb(OnDataSent);
      esp_now_register_recv_cb(OnDataRecv);
      esp_now_add_peer(broadcastAddress, ESP_NOW_ROLE_COMBO, 0, NULL, 0);

/////LORA SETUP
//      LoRa.setSPIFrequency(1E6);
      LoRa.setPins(ss, rst, dio0);
      while (!Serial);
        if (!LoRa.begin(433E6)) {
            Serial.println("Starting LoRa failed!");
            noLora = true;
        }else{
        LoRa.setPreambleLength(6);
        LoRa.setTxPower(20, PA_OUTPUT_PA_BOOST_PIN);
        LoRa.setSpreadingFactor(8); // 8 max
//LoRa.setSignalBandwidth(250E3);
//        LoRa.setSignalBandwidth(62.5E3); // default 125E3 - 62.5E3 - 41.7E3 - 31.25E3
//        LoRa.onTxDone(onTxDone);  // attiva il callback
//        LoRa.setCodingRate4(8);
        Serial.println("LoRa Starting Ok");
        }

    //// DISPLAY SETUP
      display.begin(0x3C, true);
      display.clearDisplay();  // Clear the buffer
      starting();     
      config_menu();
}

void loop() {
  if ((millis() - lastTime) > timerDelay) {
        loraCounter ++;
        hallValue = analogRead(hallPin);
        throttle = map(hallValue, valMin, valMax, 0, 100);
        if(throttle > 99 ){throttle = 99;}
        if(throttle < 0 ){throttle = 0;}
        if(throttle > 10) {
            timerOff = 0;
            timerNoConnect = 0;
            if(firstConnection){lock = false;}
        }else{
            timerOff ++;
        }
         
        if(THmode == 1) throttle = strong_throttle_curve(throttle);
        else if(THmode == 2) throttle = mid_throttle_curve(throttle);
        else if(THmode == 3) throttle = soft_throttle_curve(throttle);
        else if(THmode == 4) throttle = angular_throttle_curve(throttle);
        else if(THmode == 5) throttle = eco_throttle_curve(throttle);
        
        show_display();   
        if (TXmode == 0){// send by esp-now
            esp_now_send(broadcastAddress, (uint8_t *) &throttle, sizeof(throttle));// Send data via ESP-NOW
        }else if (TXmode == 1){        // send by LoRa
            String message = String(throttle);  // Send data by LoRa      
            sendMessage(message);
        }else if (TXmode == 2){ //send by both
//            if (loraCounter > 1){
                  String message = String(throttle);  // Send throttle by LoRa      
                  sendMessage(message);
                  loraCounter = 0;
//            }           
            esp_now_send(broadcastAddress, (uint8_t *) &throttle, sizeof(throttle));// Send data via ESP-NOW
        } 
        display.display(); 
        lastTime = millis();
        counter ++; 
        vibration();
  }

}

void sendMessage(String outgoing) {
//      Serial.println("Send throttle by LoRa... ");
      LoRa.beginPacket();                   // start packet
      LoRa.write(destination);              // add destination address
      LoRa.write(localAddress);             // add sender address
//      LoRa.write(outgoing.length());        // add payload length
//Serial.println("Payload: ");
//Serial.println(outgoing.length());
      LoRa.print(outgoing);                 // add payload
      LoRa.endPacket(true);   // true = async / non-blocking mode
}

void starting(){
       // Display Text
      display.setTextSize(4);
      display.setTextColor(WHITE);
      display.setCursor(5,0);
      display.println("Tecno");
      display.setCursor(30,35);
      display.println("Fly");
      display.setTextSize(2);      
      display.setCursor(15,75);
      display.println("eBooster");
      display.setTextSize(1);
      display.setCursor(20,105);
      display.println("R. & L. Assiro");
      display.setCursor(5,120);
      display.print("Ver. ");
      display.print(VERSION);
      if(noLora == false) {display.println(" LoRa");}
      display.display();
      delay(2000);
  for (int i = 0; i <= 5; i++) {
//      digitalWrite(ESP_BUILTIN_LED,0);
      digitalWrite(VIBRATOR,1);
      delay(200);          
//      digitalWrite(ESP_BUILTIN_LED,1);
      digitalWrite(VIBRATOR,0);
      delay(200);          
  }    
}

void OTAstart(){      //ARDUINO OTA
  ArduinoOTA.setHostname("Tecnofly-controller");
  ArduinoOTA.onStart([]() {
      Serial.println("Start");
      display.clearDisplay();  // Clear the buffer
  });
  ArduinoOTA.onEnd([]() {
      Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
            display.clearDisplay();
            display.setTextSize(2);
            display.setCursor(5,10);
            display.println("On The Air");
            display.setCursor(20,45);      
            display.println("Upgrade"); 
            display.drawRect(12, 100, 102,20, WHITE);
            display.setTextSize(3);
            display.setCursor(35,70);
            display.print(progress / (total / 100));
            display.print("%");
            for(int prog=0; prog<(progress / (total / 100));prog++){
                  display.drawRect(13, 100, prog,20, WHITE);                  
            }
            display.display();
            display.clearDisplay();
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
  Serial.println("OTA Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void progressBarr(){      
            display.clearDisplay(); 
            display.setTextSize(2);
            display.setCursor(18,0);
            display.println("DOWNLOAD");   
      for(int prog=0; prog<101;prog++){
            display.drawRect(12, 43, 102,20, WHITE);
            //display.setCursor(50,22);
            //display.println(prog);
            display.drawRect(13, 43, prog,20, WHITE);
            display.display();
            delay(10);
      }
      delay(2000);   
}

boolean checkUpgrade(){
      String payload;
      WiFiClient client;
      HTTPClient http;
      String serverPath = upgradeServer + "foilAssistUpgrade.php?T=C&V=" + VERSION;
      Serial.println(serverPath);
      http.begin(client, serverPath.c_str());
      int httpResponseCode = http.GET();
      if (httpResponseCode>0) {
        Serial.print("HTTP Response code: ");
        Serial.println(httpResponseCode);
        payload = http.getString();
        Serial.println(payload);
      }
      else {
        Serial.print("Error code: ");
        Serial.println(httpResponseCode);
      }
      http.end();
      if(payload == "true"){
            WiFiMulti.addAP(ssid, password);
            if ((WiFiMulti.run() == WL_CONNECTED)) {
                    WiFiClient client;
                    ESPhttpUpdate.setLedPin(LED_BUILTIN, LOW);
                    ESPhttpUpdate.onProgress(update_progress);
                    ESPhttpUpdate.onError(update_error);
                    t_httpUpdate_return ret = ESPhttpUpdate.update(client, upgradeServer + upgradeFile);
                    switch (ret) {
                        case HTTP_UPDATE_FAILED: Serial.printf("HTTP_UPDATE_FAILD Error (%d): %s\n", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str()); break;
                        case HTTP_UPDATE_NO_UPDATES: Serial.println("HTTP_UPDATE_NO_UPDATES"); break;
                        case HTTP_UPDATE_OK: Serial.println("HTTP_UPDATE_OK"); break;
                    }  
            }    
      }else{
            display.setCursor(0,105);
            display.print("No Upgrade available");
            display.display();         
      } 
      return false;
}

void update_progress(int cur, int total) {
            display.clearDisplay();
            display.setTextSize(2);
            display.setCursor(16,0);
            display.println("Firmware");
            display.setCursor(20,23);      
            display.println("Upgrade"); 
            display.setCursor(11,46);  
            display.println(" Download");                 
            int download = map(cur, 0, total, 0, 100); 
//            display.setTextSize(2);
//            display.setCursor(20,0);
//            display.println("DOWNLOAD");
            display.setCursor(20,25);
            display.drawRect(12, 100, 102,20, WHITE);
            display.setTextSize(3);
            display.setCursor(40,70);
            display.print(download);
            display.print("%");
            for(int prog=0; prog<download;prog++){
                  display.drawRect(13, 100, prog,20, WHITE);                  
            }
            display.display();
            display.clearDisplay();
}

void update_error(int err) {
      Serial.print("Update error... ");
      Serial.print(err);
}
/*
void showData(){
      display.clearDisplay();
      display.setTextSize(2);
      display.setCursor(0,5);
      display.print("RIDE N. ");
      display.print(file);
      display.setCursor(0,40);
      display.print("AvP ");
      display.print(powerAverage);
      display.print("W");
      display.setCursor(0,60);
      display.print("P index "); 
      display.print(powerIndex);                    
      display.setCursor(0,80);
      display.print("Time ");  
      display.print(minutes); 
      display.print(":");   
      display.print(seconds);  
      display.setCursor(0,100);
      display.print("BATT ");      
      display.print(batpercentage); 
      display.print("%");     
}

*/