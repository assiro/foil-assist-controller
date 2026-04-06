/*
    TecnoFly receiver for Foil Assist system
    2025 writed by Roberto Assiro
    
    This program is free software under the terms of the GNU General Public License 
    as published by the Free Software Foundation.
    VERSION 1.6x May 2025
    ESP32 FOIL ASSIST RECEIVER AND VESC DATA LOGGER
    Connections VESC: GPIO 16= white, 17= black, PPM white = GPIO 4
    Connections to SD CARD CS = GPIO 5
    // EEPROM byte usage: 0-3 = gravity check; 4-7 = service flag to upgrade; 8-11 = VESC offset; 12-15 life counter
    -REBOOT ISSUE .Downgrading to the previous version of esp32 Library board by espressif to version 3.0.7 solved the problem.
    -Library modify: ElegantOTA flag to enable AsyncWebServer mode file ElegantOTA.h
    VERSION 1.60 last firmware with LoRa module management
    -1.65 new pairing system and activated gravity sensor
    -2.00 NO SDCARD EXTERNAL - REPLACED WITH LITTLE FS on - all webserver file in flash - graph by uPlot
    -3.00 ADXL345 implement sensor 3 axis accelerometer and ESP core 3.3.6
*/
#include <WiFi.h>
#include <esp_now.h>
#include <AsyncTCP.h>
#include <FS.h>
#include <VescUart.h>
#include <ESP32Servo.h>
#include <esp_wifi.h>
#include <ESPAsyncWebServer.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <HTTPClient.h>
#include <ESP32httpUpdate.h>
#include <EEPROM.h>
#include <DNSServer.h>
#include <ArduinoJson.h>
#include <ElegantOTA.h> // modificated of elegantOTA.h to enable AsyncWebServer mode
#include <Arduino.h>
#include <LittleFS.h>

#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_ADXL345_U.h>

#define EEPROM_SIZE 32
#define escpin 4

const char* ssidService     = "tecnofly";
const char* passwordService = "tecnofly";

String upgradeServer = "http://www.eoloonline.it/firmware/foilAssist/";
String upgradeFile = "foilAssistReceiver.bin";
const char ssidName[] = "TecnoFly ";
char ssid[20];

const char* password = "";
String VERSION = "3.10";
const char* PARAM_INPUT_1 = "number";
const int GRAVITY1 = 33; // gravity sensor 1 for auto lock
const int GRAVITY2 = 32; // gravity sensor 2 for auto lock
Adafruit_ADXL345_Unified accel = Adafruit_ADXL345_Unified(12345);
IPAddress local_IP(192,168,10,1);//Set the IP address of ESP32 itself
IPAddress gateway(192,168,10,1);   //Set the gateway of ESP32 itself
IPAddress subnet(255,255,255,0);  //Set the subnet mask for ESP32 itself
DNSServer dnsServer;
AsyncWebServer server(80);
unsigned long ota_progress_millis = 0;
#define FORMAT_LITTLEFS_IF_FAILED true
VescUart UART;/** Initiate VescUart class */
File file;
int Poles = 14;                  //Poles of motor
// battery 10s
int batEmptyValue = 320;    // value x 10
int batFullValue = 410;  // max voltage x 10
int minvoltage = 32;

// battery 12s
int batEmptyValue12s = 384;    // value x 10
int batFullValue12s = 492;  // max voltage x 10
int minvoltage12s = 38;

uint battFullCapacity = 400;//*(42-minvoltage)/(42-32); //Wh - for Molicel P42A 21700 - 10s3p

// accelerometer
float axOffset = 0;
float ayOffset = 0;
float azOffset = 0;
bool accelerometerConnected = false;
bool gravityAlarm;
bool checkPositionEnable = false;
uint thresholdDegrees;
unsigned long gravityStartThreshold = 3000; // ms time to lock due tilt over
unsigned long gravityPositionStartTime = 0;
unsigned long gravityReleaseThreshold = 2000; // unlock time from tilt over
unsigned long gravityPositionReleseTime = 0;
bool switchPressed = false;
unsigned long switchPressStartTime = 0;
unsigned long switchReleaseStartTime = 0;

// VESC
struct VESCData {
        char time[6];   // mm:ss format
        int rpm;
        float voltage;
        float current;
        int watts;
        int throttle;
};

struct vescValues {
        float voltage;  
        float tempMosfet;
        float current;
        int power;
        int batt;
        int file;
        uint powerCounter;
        uint powerAverage;
        int powerIndex;
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
//        float watthour;
};
struct vescValues data;

bool espStatus = true;
bool vescReady = false;
float tempMosfet;
float rpm;
float voltage;
float current;
float motorCurrent;
int power;
float watthour;
int batpercentage;
uint batCapacity;
uint percentageOfCapacity;
unsigned long totalWatt;
uint powerCounter;
uint powerAverage;
int powerIndex;
uint voltageReadCounter;
float voltageAverage;
int fileName;
int Tminutes;
int Tseconds;
int Mminutes;
int Mseconds;
float receiverVersion;
float minVoltage = 50;
float maxTemp;
float maxCurrent;
int maxPower;
float voltageOffset;
char message1[9];
char message2[9];
char message3[9];
uint takeOff;
uint wave;
uint waveBestTime;
uint rideNumber;
bool riderAlert = false;
uint waveBestTimeOfTheRide;

Servo esc; 

//variables
int timeoutMax = 1500; //delay to stop motor without signal of trigger
int minPulseRate = 900; 
int maxPulseRate = 2150;
int throttle = 0;
bool receivedData = false;
uint32_t lastTimeReceived = 0;
uint32_t EspLastTimeReceived = 0;
uint32_t delays = 0;
uint32_t EspDelays = 0;
int rowCounter = 0;
int totalTime = 0;
char fileNameChar[1];
unsigned long lastDataVesc;
char dataFileName[20];
char jsonFileName[20];
bool recordEnable = false;
uint inactivityCounter;
bool serviceMode = false;
bool firstConnection = false;
bool oneTimeFlag = false;
uint timeNoRecord = 180; //max time in seconds to record data without trigger activity
uint lastAnalysis = 5; //minutes of total time after to do the data analysis every minute

volatile bool pendingServiceFlag = false;

// ---- RF signal quality ----
volatile int8_t  rssiLast    = 0;    
volatile int32_t rssiSum     = 0;    
volatile uint32_t packetCount = 0;   
volatile uint32_t packetLost  = 0;   
volatile float   rssiAvg     = 0.0f; 
uint32_t lastPacketTime      = 0;    
float lossPercent;

uint16_t boardCode; 
uint8_t broadcastAddress[6] = {0xA4, 0xCF, 0x12, 0xDC, 0xB5, 0x8E};//TRANSMITTER MAC ADDRESS STANDARD

esp_now_peer_info_t peerInfo;

// callback when data is sent
void OnDataSent(const esp_now_send_info_t *info, esp_now_send_status_t status) {       
    if (status == 1){
        espStatus = true;
    }else{
        espStatus = false;
    }
}

// data is received by ESP-NOW
void onReceiveData(const esp_now_recv_info_t *info, const uint8_t *data, int len){
        receivedData = true;

        // --- misura RF ---
        int8_t rssi = info->rx_ctrl->rssi;
        rssiLast = rssi;
        rssiSum += rssi;
        packetCount++;
        rssiAvg = (float)rssiSum / packetCount;  
        uint32_t total = packetCount + packetLost;
        lossPercent = (total > 0) ? (100.0f * packetLost / total) : 0.0f;
        if (packetLost > total){lossPercent = 0;}
        lastPacketTime = millis();
        // -----------------
        
        int throttle_value;
        memcpy(&throttle_value, data, sizeof(throttle_value));
        EspDelays = millis() - EspLastTimeReceived;

        if (throttle_value == 200 && oneTimeFlag == false){   // command from controller to update receiver from server
            pendingServiceFlag = true;
            throttle_value = 0;
        }

        if((oneTimeFlag) and (throttle_value < 100)){oneTimeFlag = false;}
    /*
        throttle = map(throttle_value, 0, 99, 0, 180); // Write the PWM signal to the ESC (0-255).
        if(throttle_value > 100){throttle = 0;}
        esc.write(int(throttle));// MAIN MOTOR CONTROL POWER
        */
        // Calcolo throttle e scrittura ESC immediata — priorità massima
        throttle = (throttle_value > 100) ? 0 : map(throttle_value, 0, 99, 0, 180);
        esc.write(throttle); // MAIN MOTOR CONTROL POWER — latenza < 1ms


        if (throttle_value > 20){
            recordEnable = true;
            inactivityCounter = 0;
            serviceMode = false;
        }
        EspLastTimeReceived = millis();
        lastTimeReceived = millis();       
        /*
        Serial.print("Throttle: ");
        Serial.print(throttle_value);
        Serial.print(" - period: ");
        Serial.println(EspDelays);  */
}
  

class CaptiveRequestHandler : public AsyncWebHandler {
public:
  CaptiveRequestHandler() {}
  virtual ~CaptiveRequestHandler() {}
  bool canHandle(AsyncWebServerRequest *request){
      return request->url() == "/";
  }
  void handleRequest(AsyncWebServerRequest *request) {
      request->send(LittleFS, "/index.html");
      Serial.println("Client Connected debug 1");
  }
};



void setup() {
        Serial.begin(115200);
        Serial2.begin(115200, SERIAL_8N1, 16, 17);// VESC GPIO 16=RX and 17=TX */
        pinMode(GRAVITY1, INPUT_PULLUP);
        pinMode(GRAVITY2, INPUT_PULLUP);
        UART.setSerialPort(&Serial2);  /** Define which ports to use as UART */

        esc.setPeriodHertz(50);    // standard 50 hz servo
        esc.attach(escpin, minPulseRate, maxPulseRate);
        esc.write(0); // init esc with 0 value

        Wire.begin(27, 26);
        EEPROM.begin(EEPROM_SIZE);
       if (!LittleFS.begin(FORMAT_LITTLEFS_IF_FAILED)) {
            Serial.println("LittleFS Mount Failed");
            return;
        }

        WiFi.mode(WIFI_AP_STA);  // Set device as Access point
        Serial.println("");  
        Serial.println(VERSION);  
        macSetting();
        Serial.println("Setting Access Point... ");
        Serial.println(WiFi.softAPConfig(local_IP, gateway, subnet) ? "Web Server Ready" : "Web Server Failed!");
        WiFi.softAP(ssid, password);
        Serial.println(ssid); 
        delay(100);
        
        adxl345Setup();
        api();

        dnsServer.start(53, "*", local_IP);
        server.addHandler(new CaptiveRequestHandler()).setFilter(ON_AP_FILTER);
        dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
        dnsServer.setTTL(300);

        server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");
        server.begin();      

        ElegantOTA.begin(&server);    // Start ElegantOTA

        // Init ESP-NOW
        if (esp_now_init() != 0) {
            Serial.println("Error initializing ESP-NOW");
            return;
        }else{
            Serial.println("ESP-NOW initializing OK!");
        }
        esp_now_register_send_cb(OnDataSent);
        esp_now_register_recv_cb(onReceiveData);

        // Register peer
        memcpy(peerInfo.peer_addr, broadcastAddress, 6);
        peerInfo.channel = 0;  
        peerInfo.encrypt = false;
        
        // Add peer        
        if (esp_now_add_peer(&peerInfo) != ESP_OK){
            Serial.println("Failed to add peer");
            return;
        }
    
        delay(600);
        batStatus();

        readFileNumber();
        readMessage();

        Serial.print("Flash size: " + String(LittleFS.totalBytes()));
        Serial.println(" -> Used: " + String(LittleFS.usedBytes()));

        uint serviceFlag = EEPROM.read(4);
        if (serviceFlag == 0x0A){setupMode();}  // check the service network for upgrading
        
        thresholdDegrees = EEPROM.read(0);   // to disable the gravity control eeprom location 0 must be zero
        if(thresholdDegrees > 0){checkPositionEnable = true;}
        EEPROM.get(16, axOffset);
        EEPROM.get(20, ayOffset);
        EEPROM.get(24, azOffset);
        Serial.println("✅ System ready");

}


void loop() {
        dnsServer.processNextRequest();
        uint32_t now = millis();
        delays = now - lastTimeReceived;     
        if (delays > timeoutMax){ esc.write(0); lastTimeReceived = now;} // stop motor for safety
        
    //    if(receivedData){
    //            receivedData = false;
    //    }


        if (packetCount > 0) {
                if ((now - lastPacketTime > 300) and (EspDelays < 10000)){   // 3 missing packets
                    packetLost++;
                    lastPacketTime = now;         
                    EspDelays = now - EspLastTimeReceived;
                }
        }
        
        if (pendingServiceFlag) {
                pendingServiceFlag = false;
                uint serviceFlag = EEPROM.read(4);
                if (serviceFlag != 0x0A) {
                        EEPROM.write(4, 0x0A); // save flag on eeprom for next reboot
                        EEPROM.commit();
                        Serial.println("State setup ON saved in eeprom... reboot"); //IT NEEDS A REBOOT
                        delay(1000);
                        ESP.restart();
                }
        }  

        if(!firstConnection and throttle > 20 and vescReady){
                firstConnection = true;
                dataFileCreate();   // it creates the log file only in the firts use of throttle control
//                openImuFile();    // IMU file stores the entire data of the accelerometer's fifo
                lifeCounter();  // increment total number of rides counter of the life of the system    
        }    
        getTelemetry();
        if((Tminutes > lastAnalysis) and (throttle == 0)){  // ride analysis every minutes
                if(accelerometerConnected){
                    rideAnalysis();
                }else{
                    rideAnalysisNoADXL();
                }
                lastAnalysis = Tminutes;
        }
}



void writeSerialNumber(){
      byte newSerialNumber = 0x05;
      Serial.println("Writing serial number for device");
      EEPROM.write(0, newSerialNumber);
      EEPROM.commit();
}

void macSetting(){ // initialization of board code for transmition
            uint8_t lsb = EEPROM.read(1);
            uint8_t msb = EEPROM.read(2);
            if((msb == 255) & (lsb == 255)){generateBoardCode();}   // start fase firts time
            Serial.print("EEPROM serial number: ");
            boardCode = (msb << 8) | lsb;
            Serial.println(boardCode);  
            strcpy(ssid, ssidName);
            char serialNumber[4]; 
            itoa(boardCode, serialNumber, 10);
            strcat(ssid, serialNumber);
            broadcastAddress[4] = msb; 
            broadcastAddress[5] = lsb;  
            Serial.print("BROADCAST MAC ADDRESS: ");
            for (int i = 0; i < 6; i++) {
                if (i > 0) Serial.print(":");
                Serial.printf("%02X", broadcastAddress[i]);
            }
            Serial.println();

            uint8_t customMAC[6] = {0x4C, 0x11, 0xAE, 0x0D, msb, lsb};//RECEIVER MAC ADDRESS

            Serial.print("OLD MAC:  ");  
            Serial.println(WiFi.macAddress());  

            // Imposta il nuovo MAC address
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

void lifeCounter(){ // increment life Counter or total number of rides
        uint32_t lifeCounter;
        EEPROM.get(12, lifeCounter);   // legge 4 byte dall’indirizzo 12
        lifeCounter++;
        EEPROM.put(12, lifeCounter);
        EEPROM.commit();
}


void generateBoardCode(){
            Serial.println("Generate new code...");
            byte value1 = random(0, 256);
            byte value2 = random(0, 256);
            Serial.print("Value 1: ");
            Serial.println(value1, HEX);
            Serial.print("Value 2: ");
            Serial.println(value2, HEX);  
            EEPROM.write(1, value1);
            EEPROM.write(2, value2);
            EEPROM.put(12, 0);
            EEPROM.commit();  
            delay(1000);
//            Serial.println("Rebooting...");
            ESP.restart();
}


void disableAP() {
        Serial.println("Disable Access point");
        WiFi.softAPdisconnect(true);
        delay(100);
        WiFi.mode(WIFI_STA);
        delay(100);
}


