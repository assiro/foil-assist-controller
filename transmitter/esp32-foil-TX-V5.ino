/*
    TecnoFly remote controller for Foil Assist system
    2025 writed by Roberto Assiro
    
    This program is free software under the terms of the GNU General Public License 
    as published by the Free Software Foundation.
    VERSION 5.xx jan 2025 - EPAPER DISPLAY version GDEH0154D67 200x200, SSD1681
    ESP32 Core 3.5.5 + FreeRTOS
*/
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_now.h>
#include <FS.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <U8g2lib.h>
#include <EEPROM.h>
#include <DNSServer.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <ESP32httpUpdate.h>
#include <Update.h>
#include <AsyncTCP.h>
#include <HTTPClient.h>
#include "image.h"
#define ENABLE_GxEPD2_GFX 1
#include <GxEPD2_BW.h>
#include <GxEPD2_3C.h>
#include <Adafruit_NeoPixel.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

# define LED  2
# define CHARGING 3
# define VIBRATOR  8
# define hallPin  0

#define WS_LED_PIN 10            // patch to turn off led ws2812 on esp32 c3 zero
#define WS_LED_COUNT 1
Adafruit_NeoPixel wsLED(WS_LED_COUNT, WS_LED_PIN, NEO_GRB + NEO_KHZ800);

// 1.54'' EPD Module
GxEPD2_BW<GxEPD2_154_D67, GxEPD2_154_D67::HEIGHT> 
display(GxEPD2_154_D67(/*CS=*/ 7, /*DC=*/ 1, /*RES=*/ 9, /*BUSY=*/ 10)); // GDEH0154D67 200x200, SSD1681

String VERSION = "5.36";
const char* ssidService     = "tecnofly";
const char* passwordService = "tecnofly";

String upgradeServer = "http://www.eoloonline.it/firmware/foilAssist/";
String upgradeFile = "foilAssistController5.bin"; 
#define EEPROM_SIZE 16
bool espState = false;   
bool firstConnection = false;

uint8_t THmode = 0; // Throttle mode
uint displayMode; // display view mode
bool espStatus = false;

unsigned long lastTime;  
const unsigned long transmissionInterval = 100;  // send throttle to receiver in ms
const unsigned long displayUpdateInterval = 800; // update epaper display every xxx ms
unsigned long vibrationTimeOff;
unsigned long vibrationTime = 0; 
unsigned long timerOff = 0; 
unsigned long timerNoConnect = 0; 
int hallValue;
int throttle;
int triggerValue;
int counter;
bool lowBatt = false;
bool lock = true;
uint valMin = 1600;
uint valMax = 2600;
uint valMiddle = ((valMax - valMin) / 2) + valMin;
//uint waveBuffer[10];
//uint takeOffBuffer[10];
//uint waveBestTimeBuffer[10];
int partialCount = 0;
uint8_t maxPower;  // max power to send by throttle TEST
uint8_t TxPower;
uint8_t surfingSeconds;  // timer for seconds in surfing mode (throttle off)
uint16_t surfingMinutes;

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
uint powerCounter;
uint powerAverage;
uint powerIndex;
float receiverVersion;
char message1[9];
char message2[9];
char message3[9];  
int Tminutes;
int Tseconds;
int Mminutes;
int Mseconds;  
uint takeOff;
uint wave;
uint waveBestTime;
uint rideNumber;
bool gravityAlarm;
bool riderAlert;
bool vibrationAlarm = false;
bool justOneAlert;

uint boardCode;
uint8_t broadcastAddress[6] = {0x4C, 0x11, 0xAE, 0x0D, 0xE5, 0x35};//RECEIVER MAC ADDRESS
esp_now_peer_info_t peerInfo;

// Must match the receiver structure// Structure example to send data
typedef struct struct_message {
        float a;
        float tempMosfet;  
        float avgInputCurrent;  
        int watt;
        int batt;
        int file;
        uint powerCounter;
        uint powerAverage;
        uint powerIndex;
        float receiverVersion;
        char message1[9];
        char message2[9];
        char message3[9];  
        int Tminutes;
        int Tseconds;   
        int Mminutes;
        int Mseconds;  
        uint takeOff;
        uint wave;
        uint waveBestTime;
        uint rideNumber;
        bool gravityAlarm;
        bool riderAlert = false;
        //float watthour;
} struct_message;

struct myData {int a;};

// Create a struct_message called myData
struct_message myData;
struct_message incomingReadings;

// ================== MUTEX & DATA ==================
SemaphoreHandle_t dataMutex;
struct TxData {
    uint16_t trigger;
    uint8_t status;
    uint32_t counter;
};

TxData txData;

// Callback when data is sent
void OnDataSent(const esp_now_send_info_t *info, esp_now_send_status_t status) {  
  if (status == 0){
        espStatus = true;
  }else{
//    Serial.println("ESP_NOW delivery fail...");
        espStatus = false;
  }
}

void OnDataRecv(const esp_now_recv_info_t *info, const uint8_t *incomingData, int len){    
      memcpy(&incomingReadings, incomingData, sizeof(incomingReadings));
      inpVoltage = incomingReadings.a;
      tempMosfet = incomingReadings.tempMosfet;
      avgInputCurrent = incomingReadings.avgInputCurrent;
      watt = incomingReadings.watt;
      batpercentage = incomingReadings.batt;
      file = incomingReadings.file+1;    
      powerCounter = incomingReadings.powerCounter;
      powerAverage = incomingReadings.powerAverage;
      powerIndex = incomingReadings.powerIndex;
      receiverVersion = incomingReadings.receiverVersion;
      Tminutes = incomingReadings.Tminutes;
      Tseconds = incomingReadings.Tseconds;      
      Mminutes = incomingReadings.Mminutes;
      Mseconds = incomingReadings.Mseconds;    
      wave = incomingReadings.wave;
      takeOff = incomingReadings.takeOff;
      waveBestTime = incomingReadings.waveBestTime;    
      gravityAlarm = incomingReadings.gravityAlarm;  
      riderAlert = incomingReadings.riderAlert;      
//      waveBuffer[incomingReadings.rideNumber] = incomingReadings.wave;
//      takeOffBuffer[incomingReadings.rideNumber] = incomingReadings.takeOff;
//      waveBestTimeBuffer[incomingReadings.rideNumber] = incomingReadings.waveBestTime; 

      firstConnection = true;
  //    if(incomingReadings.rideNumber != 0){Serial.println(incomingReadings.rideNumber);}
  //    Serial.println(receiverVersion);           
 //Serial.println(riderAlert);    
}


////  MODIFICA 
// ================== PROTOTYPES ==================
void espnowTask(void *pvParameters);
void logicTask(void *pvParameters);
void displayTask(void *pvParameters);


void setup() {
    Serial.begin(115200); 
    WiFi.mode(WIFI_STA);    

    pinMode(WS_LED_PIN, INPUT);
    wsLED.begin();      // Abilita il controllo WS2812
    wsLED.clear();      // Imposta 0,0,0
    wsLED.show();       // ⚡ Spegne il LED in <1ms
      

    pinMode(LED, OUTPUT);
    pinMode(VIBRATOR, OUTPUT);
    pinMode(CHARGING, INPUT);
    digitalWrite(LED , 0);
    digitalWrite(VIBRATOR , 0);
    analogReadResolution(12);
    EEPROM.begin(32);


delay (200);    
TxPower = EEPROM.read(9);
if(TxPower > 5){TxPower = 5;}
Serial.print("Tx Power: "); Serial.println(TxPower);

switch (TxPower) {
        case 0: WiFi.setTxPower(WIFI_POWER_2dBm); break;
        case 1: WiFi.setTxPower(WIFI_POWER_5dBm); break;
        case 2: WiFi.setTxPower(WIFI_POWER_7dBm);break;
        case 3: WiFi.setTxPower(WIFI_POWER_8_5dBm);break;
        case 4: WiFi.setTxPower(WIFI_POWER_11dBm); break;
        case 5: WiFi.setTxPower(WIFI_POWER_15dBm); break;
}
//    WiFi.setTxPower(WIFI_POWER_15dBm); // best work for esp32 c3 super mini and zero

//    esp_wifi_set_max_tx_power(80); // ≈ +21 dBm valori: 8 → 84 (unità = 0.25 dBm)
//    esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);  // fixed channel (es. 1)
//    esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_LR);  // enable long range

    display.init(); 
    macSetting();  
    // Init ESP-NOW
    if (esp_now_init() != 0) {
        Serial.println("Error initializing ESP-NOW");
        return;
    }else{
        Serial.println("ESP-NOW initializing OK!");
    }

    esp_now_register_send_cb(OnDataSent);
    esp_now_register_recv_cb(OnDataRecv);

    // Register peer
    memcpy(peerInfo.peer_addr, broadcastAddress, 6);
    peerInfo.channel = 0;  
    peerInfo.encrypt = false;
    
    // Add peer        
    if (esp_now_add_peer(&peerInfo) != ESP_OK){
        Serial.println("Failed to add peer");
        return;
    }

    // ---------- MUTEX ----------
    dataMutex = xSemaphoreCreateMutex();

    startingEpaperDisplay();
    firstInit();
    //hallTest();  // ONLY FOR TEST WITH USB CONNECTED 

     // ---------- TASKS ----------
    xTaskCreate(espnowTask, "ESPNow", 4096, NULL, 3, NULL);
    xTaskCreate(logicTask, "Logic", 4096, NULL, 2, NULL);
    xTaskCreate(displayTask, "Display", 8192, NULL,1, NULL);

}


void loop() {vTaskDelay(portMAX_DELAY);}


// ================== TASK ESP-NOW ==================
void espnowTask(void *pvParameters) {
    TickType_t lastWake = xTaskGetTickCount();
    for (;;) {
            if (xSemaphoreTake(dataMutex, portMAX_DELAY)) {
                xSemaphoreGive(dataMutex);
            }
            read_throttle();
            if (throttle > 10) {
                        timerOff = 0;
                        timerNoConnect = 0;
                        if(lock){unlock();}
            } else {
                        timerOff++;
                        surfingSeconds = (timerOff / 10) / 60;
                        surfingMinutes = (timerOff / 10) % 60;
            }
            switch (THmode) {
                case 1: throttle = strong_throttle_curve(throttle); break;
                case 2: throttle = mid_throttle_curve(throttle); break;
                case 3: throttle = soft_throttle_curve(throttle); break;
                case 4: throttle = angular_throttle_curve(throttle); break;
                case 5: throttle = eco_throttle_curve(throttle); break;
            }
            esp_now_send(broadcastAddress, (uint8_t *)&throttle, sizeof(throttle));
            vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(transmissionInterval));
        }
}

// ================== TASK LOGIC ==================
void logicTask(void *pvParameters) {
    for (;;) {
        if (xSemaphoreTake(dataMutex, portMAX_DELAY)) {
                if(lowBatt){
                    digitalWrite(LED , 1);
                }else{
                    digitalWrite(LED , 0);
                }
                if(gravityAlarm){
                        if(!lock){
                            throttle = 0;
                            lock = 1;                    
                            vibration_confirm(800);
                        }
                }

                if((riderAlert) and (displayMode == 3)){vibrationTime = 1500;}   //new time record on surfing or pump by vibration alert

                vibration();
                checkCharging();

                txData.status = 1;
                txData.counter++;
                xSemaphoreGive(dataMutex);
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}


///////////////////////////////
//######## FUNCTIONS #########

void startingEpaperDisplay(){  
        display.setRotation(0);
        display.setFont(NULL); 
        display.setTextColor(GxEPD_BLACK);
        display.fillScreen(GxEPD_WHITE);     
        delay(500);
        display.setFullWindow();
//        display.setPartialWindow(0, 0, 200, 200); 
        display.nextPage();
        delay(2000);
        checkCharging();
        display.setPartialWindow(0, 0, 200, 200);
        display.firstPage();
        display.setTextSize(6); 
        display.setCursor(15, 5); 
        display.print("Tecno"); 
        display.setCursor(50, 55); 
        display.print("Fly"); 
        display.setTextSize(3);      
        display.setCursor(0,115);
        display.println("Foil Assist");
        display.setTextSize(2);
        display.setCursor(20,155);
        display.println("R. & L. Assiro");
        display.setCursor(5,180);
        display.print("Ver. ");
        display.print(VERSION);
        display.nextPage();
        delay(3000);
        digitalWrite(LED,1);
        displayBitmap(0, 0, TecnoFlyLogo, 200, 200);
        delay(2000);  
        digitalWrite(LED,0);

}


void vibration(){
          int timeOn = vibrationTime;
          if ((vibrationTimeOff + vibrationTime + timeOn < millis()) and (vibrationTime > 0)) {
                digitalWrite(VIBRATOR,1);
                vibrationTimeOff = millis();
          }else if(vibrationTimeOff + vibrationTime < millis()) {
                digitalWrite(VIBRATOR,0);
                vibrationTime = 0;
          }      
}


void vibration_confirm(int vibrationTime){
      digitalWrite(LED,1);
      digitalWrite(VIBRATOR,1);
      delay(vibrationTime);          
      digitalWrite(LED,0);
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
        esp_err_t result = esp_wifi_set_mac(WIFI_IF_STA, customMAC);
        
        if (result == ESP_OK) {
            Serial.println("MAC address cambiato con successo!");
            Serial.print("NEW MAC:  ");
            Serial.println(WiFi.macAddress());                
        } else {
            Serial.print("Errore nel cambiare il MAC address. Codice errore: ");
            Serial.println(result);
        }
}


void checkCharging(){
      if (digitalRead(CHARGING) == HIGH) {
          do {
              displayBitmap(0, 0, chargerLogo, 200, 200); 
          } while (display.nextPage()); 
          delay(1000);                   
          display.hibernate();             
          esp_deep_sleep_start();       
      }
}

void unlock(){       
      bool exit = true;
      while(exit){
          digitalWrite(LED,0);  
          read_throttle();
          if(throttle < 10) {exit = false;}
          delay(500);
          digitalWrite(LED,1);
      }
      lock = false;
      throttle = 0;
      vibrationTime = 800;  // alert vibration before unlock
      delay(1000);
      vibrationTime = 600;
}