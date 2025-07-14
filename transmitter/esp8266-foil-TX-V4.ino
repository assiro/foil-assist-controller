/*
    TecnoFly cremote controller for Foil Assist system
    2025 writed by Roberto Assiro
    
    This program is free software under the terms of the GNU General Public License 
    as published by the Free Software Foundation.
    VERSION 4.5x Feb 2025 WITHOUT LORA
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
#include <Adafruit_SH110X.h>
#include <ESP8266httpUpdate.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266HTTPClient.h>
#include "image.h"
ESP8266WiFiMulti WiFiMulti;
String VERSION = "4.55";
const char* ssid     = "tecnofly";
const char* password = "tecnofly";
String upgradeServer = "http://www.eoloonline.it/firmware/foilAssist/";
String upgradeFile = "foilAssistController.bin";
const int VIBRATOR = D8;
//const int LED = D4;
//const int POWER = D0; // push button on / off

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 128 // OLED display height, in pixels
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SH1107 display = Adafruit_SH1107(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET, 1000000, 100000);
#define EEPROM_SIZE 16
bool espState = false;      // Stato dell'ESP
bool firstConnection = false;

uint THmode = 0; // Throttle mode
uint displayMode; // display view mode
bool espStatus = false;

unsigned long lastTime;  
unsigned long timerDelay = 80;  // send throttle to receiver
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
uint valMin = 0;
uint valMax = 0;
uint valMiddle = ((valMax - valMin) / 2) + valMin;

uint waveBuffer[11];
uint takeOffBuffer[11];
uint waveBestTimeBuffer[11];


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
  uint takeOff;
  uint wave;
  uint waveBestTime;
  uint rideNumber;
  bool gravityAlarm;
  
uint boardCode;
uint8_t broadcastAddress[6] = {0x4C, 0x11, 0xAE, 0x0D, 0xE5, 0x35};//RECEIVER MAC ADDRESS

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
  int watthour;
  uint takeOff;
  uint wave;
  uint waveBestTime;
  uint rideNumber;
  bool gravityAlarm;

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
      gravityAlarm = incomingReadings.gravityAlarm;
//Serial.println(incomingReadings.message1);
//Serial.println(incomingReadings.message2);  
      waveBuffer[incomingReadings.rideNumber] = incomingReadings.wave;
      takeOffBuffer[incomingReadings.rideNumber] = incomingReadings.takeOff;
      waveBestTimeBuffer[incomingReadings.rideNumber] = incomingReadings.waveBestTime; 
//      if(incomingReadings.rideNumber != 0){Serial.println(incomingReadings.rideNumber);}
      firstConnection = true;
  //    if(incomingReadings.gravityAlarm){lock = 1;}
//      Serial.println(incomingReadings.gravityAlarm);

}
 
void setup() {
      Serial.begin(115200);
      ESPhttpUpdate.setClientTimeout(8000);
      WiFi.mode(WIFI_STA);  // Set device as a Wi-Fi Station

      pinMode(LED_BUILTIN, OUTPUT);
      pinMode(VIBRATOR, OUTPUT);
      digitalWrite(LED_BUILTIN, 0);

      EEPROM.begin(32);
      macSetting();

 // Init ESP-NOW
      if (esp_now_init() != 0) {  // Init ESP-NOW
          Serial.println("Error initializing ESP-NOW");
          return;
      }   
      esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
      esp_now_register_send_cb(OnDataSent);
      esp_now_register_recv_cb(OnDataRecv);
      esp_now_add_peer(broadcastAddress, ESP_NOW_ROLE_COMBO, 0, NULL, 0);

    //// DISPLAY SETUP
      display.begin(0x3C, true);
      display.clearDisplay();  // Clear the buffer
      starting();     

      display.clearDisplay(); // Clear the display buffer
      display.drawBitmap(0, 0, tecnofly_logo, 128, 128, SH110X_WHITE);
      display.display(); // Show the display buffer on the screen

      delay(2500);

      config_menu();
}



void loop() {
      if ((millis() - lastTime) > timerDelay) {
            read_throttle();
            if(throttle > 10) {
                timerOff = 0;
                timerNoConnect = 0;
//              if(firstConnection){lock = false;}
                if(lock){unlock();}
            }else{
                timerOff ++;
            }
            
            if(THmode == 1) throttle = strong_throttle_curve(throttle);
            else if(THmode == 2) throttle = mid_throttle_curve(throttle);
            else if(THmode == 3) throttle = soft_throttle_curve(throttle);
            else if(THmode == 4) throttle = angular_throttle_curve(throttle);
            else if(THmode == 5) throttle = eco_throttle_curve(throttle);

            if(gravityAlarm){
                  if(!lock){vibration_confirm();}
                  throttle = 0;
                  lock = 1;
            }
            show_display();   
            esp_now_send(broadcastAddress, (uint8_t *) &throttle, sizeof(throttle));// Send data via ESP-NOW

 //           display.display(); 
            lastTime = millis();
            vibration();
      }

}

void read_throttle(){
      hallValue = analogRead(hallPin);
      throttle = map(hallValue, valMin, valMax, 0, 100);
      throttle = constrain(throttle, 0, 99);
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
      display.display();
      delay(2000);
//      digitalWrite(LED_BUILTIN, 0);
 //     delay(100);
//      digitalWrite(LED_BUILTIN, 1);
 /*     
  for (int i = 0; i <= 5; i++) {
    vibration();
      delay(200);          
  }  */
   
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
            display.setCursor(8,46);  
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

void vibration_confirm(){
      digitalWrite(LED_BUILTIN,1);
      digitalWrite(VIBRATOR,1);
      delay(200);          
      digitalWrite(LED_BUILTIN,0);
      digitalWrite(VIBRATOR,0);
}

void macSetting(){
      uint8_t currentMAC[6];
      WiFi.macAddress(currentMAC);
      Serial.println("");
      Serial.print("MAC originale: ");
      for (int i = 0; i < 6; i++) {
            if (i > 0) Serial.print(":");
            Serial.printf("%02X", currentMAC[i]);
      }
      Serial.println();
      uint8_t lsb = EEPROM.read(13);
      uint8_t msb = EEPROM.read(14);
      broadcastAddress[4] = msb;  // aggiorna il terzo byte
      broadcastAddress[5] = lsb;  // aggiorna il quarto byte
      boardCode = (msb << 8) | lsb;

      Serial.print("EEPROM serial number: ");
      Serial.println(boardCode);  

      Serial.print("BROADCAST MAC ADDRESS: ");
      for (int i = 0; i < 6; i++) {
            if (i > 0) Serial.print(":");
            Serial.printf("%02X", broadcastAddress[i]);
      }
      Serial.println();

      uint8_t customMAC[6] = {0xA4, 0xCF, 0x12, 0xDC, msb, lsb};//TRANSMITTER MAC ADDRESS
        // Imposta il nuovo MAC address
      if (wifi_set_macaddr(STATION_IF, customMAC)) {
          Serial.println("Error to change the MAC address!");
      } else {
          Serial.print("MAC address changed: ");
          for (int i = 0; i < 6; i++) {
                  if (i > 0) Serial.print(":");
                  Serial.printf("%02X", customMAC[i]);
          }
          Serial.println();
      }

//      Serial.print("OLD MAC: ");  
//      Serial.println(WiFi.macAddress());  
 //     wifi_set_macaddr(STATION_IF, &newMACAddress[0]);

      Serial.print("NEW MAC: ");
      Serial.println(WiFi.macAddress());


}
