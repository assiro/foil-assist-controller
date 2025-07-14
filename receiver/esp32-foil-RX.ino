/*
    TecnoFly receiver for Foil Assist system
    2024 writed by Roberto Assiro
    
    This program is free software under the terms of the GNU General Public License 
    as published by the Free Software Foundation.
    VERSION 1.6x May 2025
    ESP32 FOIL ASSIST RECEIVER AND VESC DATA LOGGER
    Connections VESC: GPIO 16= white, 17= black, PPM white = GPIO 4
    Connections to SD CARD CS = GPIO 5
    // EEPROM byte usage: 0-3 = RXmode; 4-7 = vesc offset; 8-11 = VESC offset;
    -REBOOT ISSUE .Downgrading to the previous version of esp32 Library board by espressif to version 3.0.7 solved the problem.
    -Library modify: ElegantOTA flag to enable AsyncWebServer mode file ElegantOTA.h
    VERSION 1.60 last firmware with LoRa module management
    -1.65 new pairing system activated gravity sensor
*/
#include <WiFi.h>
#include <esp_now.h>
#include <AsyncTCP.h>
#include <SPIFFS.h>
#include <FS.h>
#include <SPI.h>
#include <SD.h>
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

#define EEPROM_SIZE 32
#define escpin 4
const int chipSelect = SS;
const char* ssidService     = "tecnofly";
const char* passwordService = "tecnofly";
String upgradeServer = "http://www.eoloonline.it/firmware/foilAssist/";
String upgradeFile = "foilAssistReceiver.bin";  
const char ssidName[] = "TecnoFly ";
char ssid[20];

const char* password = "";
String VERSION = "1.65";
const char* PARAM_INPUT_1 = "number";
const int GRAVITY = 33; // gravity sensor for auto lock
const int GRAVITY2 = 32; // gravity sensor 2 for auto lock

IPAddress local_IP(192,168,10,1);//Set the IP address of ESP32 itself
IPAddress gateway(192,168,10,1);   //Set the gateway of ESP32 itself
IPAddress subnet(255,255,255,0);  //Set the subnet mask for ESP32 itself
DNSServer dnsServer;
AsyncWebServer server(80);
unsigned long ota_progress_millis = 0;
#define FORMAT_SPIFFS_IF_FAILED true
VescUart UART;/** Initiate VescUart class */
File file;
int Poles = 14;                  //Poles of motor
int batEmptyValue = 320;    // value x 10
int batFullValue = 410;  // max voltage x 10
int minvoltage = 32;
//int maxvoltage = 42;
uint battFullCapacity = 390;//*(42-minvoltage)/(42-32); //Wh - for Molicel P42A 21700 - 10s3p

struct VESCData {
    char time[6];   // hh:mm formato
    int rpm;
    float voltage;
    float current;
    float Mcurrent;
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
    int Rminutes;
    int Rseconds;
    uint powerCounter;
    uint powerAverage;
    int powerIndex;
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
//float amphour;
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
bool sdcard;
int Tminutes;
int Tseconds;
int Rminutes;
int Rseconds;
float receiverVersion;
float minVoltage = 50;
float maxTemp;
float maxCurrent;
int maxPower;
float voltageOffset;
char message1[8];
char message2[8];
char message3[8];
uint takeOff;
uint wave;
uint waveBestTime;
uint rideNumber;
bool gravityAlarm;
bool gravityEnable;

Servo esc; 

//variables
int timeoutMax = 1300;
int minPulseRate = 900; 
int maxPulseRate = 2150;
int throttle = 0;
bool recievedData = false;
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
bool firtsConnection = false;
bool oneTimeFlag = false;

unsigned long gravityThreshold = 5000; // ms time to lock due gravity sensor
bool switchPressed = false;
unsigned long switchPressStartTime = 0;
unsigned long gravityReleaseThreshold = 2000; 
unsigned long switchReleaseStartTime = 0;

uint16_t boardCode; 
uint8_t broadcastAddress[6] = {0xA4, 0xCF, 0x12, 0xDC, 0xB5, 0x8E};//TRANSMITTER MAC ADDRESS STANDARD

esp_now_peer_info_t peerInfo;

// callback when data is sent
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
//  Serial.print("\r\nLast Packet Send Status:\t");
//  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
    if (status == 1){
        espStatus = true;
    }else{
        espStatus = false;
    }
}

// data is received by ESP-NOW
void onReceiveData(const uint8_t * mac, const uint8_t *data, int len) {
      int throttle_value;
      memcpy(&throttle_value, data, sizeof(throttle_value));
      EspDelays = millis() - EspLastTimeReceived;

      if (throttle_value == 250 && oneTimeFlag == false){
          esc.write(0);
          rideAnalysis();
          throttle_value = 0;
      }
      
      if (throttle_value == 200 && oneTimeFlag == false){
          esc.write(0);
          uint serviceFlag = EEPROM.read(4);
          if (serviceFlag != 0x0A){
                EEPROM.write(4, 0x0A); // save flag on eeprom for next reboot
                EEPROM.commit();
                Serial.println("State setup ON saved in flash memory"); //IT NEEDS A REBOOT
          }         
          throttle_value = 0;
    //    delay(3000); ESP.restart();         
      }

      if((oneTimeFlag) and (throttle_value < 100)){oneTimeFlag = false;}
      throttle = map(throttle_value, 0, 99, 0, 180); // Write the PWM signal to the ESC (0-255).
      if(throttle_value > 100){throttle = 0;}
      esc.write(int(throttle));
      if (throttle_value > 20){
          recordEnable = true;
          inactivityCounter = 0;
          serviceMode = false;
      }
      EspLastTimeReceived = millis();
      lastTimeReceived = millis();       
      /**/
      Serial.print("Throttle: ");
      Serial.print(throttle_value);
      Serial.print(" - period: ");
      Serial.println(EspDelays);  
    //  if(gravityAlarm){Serial.println("ALARM");}
}
  
  
class CaptiveRequestHandler : public AsyncWebHandler {
public:
    CaptiveRequestHandler() {}
    virtual ~CaptiveRequestHandler() {}
    bool canHandle(AsyncWebServerRequest *request){
        return request->url() == "/";
    }
    void handleRequest(AsyncWebServerRequest *request) {
        request->send(SD, "/index.html");
        Serial.println("Client Connected debug 1");
    }
};



void setup() {
        Serial.begin(115200);
        Serial2.begin(115200, SERIAL_8N1, 16, 17);// VESC GPIO 16=RX and 17=TX */
        pinMode(GRAVITY, INPUT_PULLUP);
        pinMode(GRAVITY2, INPUT_PULLUP);
        UART.setSerialPort(&Serial2);  /** Define which ports to use as UART */
        if(!SPIFFS.begin(FORMAT_SPIFFS_IF_FAILED)){    // Initialize SPIFFS
                Serial.println("An Error has occurred while mounting SPIFFS");
                return;
        }     
        esc.setPeriodHertz(50);    // standard 50 hz servo
        esc.attach(escpin, minPulseRate, maxPulseRate);
        esc.write(0); // init esc with 0 value

        EEPROM.begin(EEPROM_SIZE);
    //      int32_t channel = 0;
    //      esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
        WiFi.mode(WIFI_AP_STA);  // Set device as Access point
        Serial.println("");  
        Serial.println(VERSION);  

        macSetting();

        Serial.println("Setting Access Point... ");
        Serial.println(WiFi.softAPConfig(local_IP, gateway, subnet) ? "Web Server Ready" : "Web Server Failed!");
        WiFi.softAP(ssid, password);
        Serial.println(ssid); 
        delay(100);
        initSDCard();
        api();

        dnsServer.start(53, "*", local_IP);
        server.addHandler(new CaptiveRequestHandler()).setFilter(ON_AP_FILTER);
        dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
        dnsServer.setTTL(300);

        server.serveStatic("/", SD, "/");
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
        esp_now_register_recv_cb(esp_now_recv_cb_t(onReceiveData));//      esp_now_register_recv_cb(onReceiveData);

        // Register peer
        memcpy(peerInfo.peer_addr, broadcastAddress, 6);
        peerInfo.channel = 0;  
        peerInfo.encrypt = false;
        
        // Add peer        
        if (esp_now_add_peer(&peerInfo) != ESP_OK){
            Serial.println("Failed to add peer");
            return;
        }
    
        delay(500);
        batStatus();
    //      dataFileCreate();
        delay(500);
        readFileNumber();
        readMessage();
        uint serviceFlag = EEPROM.read(4);
        if (serviceFlag == 0x0A){
                setupMode();  // check the service network for upgrading
        }
        serviceFlag = EEPROM.read(0);
        if(serviceFlag > 0){gravityEnable = true;}
}



void loop() {
        dnsServer.processNextRequest();

        delays = millis() - lastTimeReceived;     
        if (delays > timeoutMax){ esc.write(0); lastTimeReceived = millis();} // stop motor for safety

        if(firtsConnection == false and throttle > 20 and vescReady == true){
                firtsConnection = true;
                dataFileCreate();// it creates the log file only in the firts use of throttle control
        }    

        getStateOfSensor();
        getVescData();
}



void setupMode(){ // UPGRADE FIRMWARE AND WEBSERVER 
    serviceMode = true;
    Serial.println("SETUP MODE");
    WiFi.mode(WIFI_STA); 
    delay(500);
    WiFi.begin(ssidService, passwordService);
  // Wait for connection
  int count = 0;
  Serial.println("Waiting for Wi-Fi connection");
  while ( count < 40 ) {
      if (WiFi.status() == WL_CONNECTED) {
          Serial.print("Connected to ");
          Serial.println(ssidService);
          Serial.print("IP address: ");
          Serial.println(WiFi.localIP());
          Serial.println("Check for upgrade on Web Server...");
          String payload;
          WiFiClient client;
          HTTPClient http;
          String serverPath = upgradeServer + "foilAssistUpgrade.php?T=R&V=" + VERSION;
          Serial.println(serverPath);
          http.begin(client, serverPath.c_str());
          int httpResponseCode = http.GET();
          if (httpResponseCode>0) {
            Serial.print("HTTP Response code: ");
            Serial.println(httpResponseCode);
            payload = http.getString();
//            Serial.println(payload);
          }
          else {
            Serial.print("Error code: ");
            Serial.println(httpResponseCode);
          }
          http.end();
          Serial.println("State setup OFF saved in flash memory");  
          EEPROM.write(4, 0);
          EEPROM.commit();
          if(payload == "true"){
                  downloadFile(); // upgrade files in sd card               
                  Serial.println("Downloading firmware...");     
                  t_httpUpdate_return ret = ESPhttpUpdate.update(upgradeServer + upgradeFile);
                  switch(ret) {
                      case HTTP_UPDATE_FAILED: Serial.printf("HTTP_UPDATE_FAILD Error (%d): %s", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());break;
                      case HTTP_UPDATE_NO_UPDATES: Serial.println("HTTP_UPDATE_NO_UPDATES");break;
                      case HTTP_UPDATE_OK: Serial.println("HTTP_UPDATE_OK");break;
                  }
          }else{        
          Serial.println("No Upgrade available");
          } 
          Serial.println("Reboot...");
          ESP.restart();
      }
  delay(500);
  Serial.print(".");
  count++;
  }
  Serial.println("State setup OFF saved in flash memory");  
  EEPROM.write(4, 0);
  EEPROM.commit();  
  Serial.println("End of setup. Reboot...");
  ESP.restart();  
}

void update_progress(int cur, int total) {      
            Serial.print(cur);
            Serial.print(" - ");
            Serial.print(total);
}

void writeSerialNumber(){
      byte newSerialNumber = 0x05;
      Serial.println("Writing serial number for new device");
      EEPROM.write(0, newSerialNumber);
      EEPROM.commit();
}

void readFileNumber(){
    const char* path = "/fileNumb.txt";
    size_t len = 0;
//    Serial.printf("Reading file: %s\n", path);
    File file = SD.open(path);
    fileName;
    if(file.available()){
        if(file.size() == 0){
              Serial.println("File number empty!");
        }else{
              char inChar = file.read();
              Serial.print("Last Log file number is: ");
              Serial.println(inChar);     
              fileName = inChar - '0';
        } 
    }else{
        Serial.println("file number not present...");
    }
    file.close();
    itoa(fileName, fileNameChar,10);
}

void readMessage(){
  File file = SD.open("/message.txt");
  if (!file) {
      Serial.println("Errore nell'apertura del file.");
      return;
  }
  String str1 = file.readStringUntil('\n');
  str1.toCharArray(data.message1, 8);
  String str2 = file.readStringUntil('\n');
  str2.toCharArray(data.message2, 8);
  String str3 = file.readStringUntil('\n');
  str3.toCharArray(data.message3, 8);
  file.close();
  Serial.print(data.message1);
  Serial.print(" ");
  Serial.print(data.message2);
  Serial.print(" ");
  Serial.println(data.message3);  
}


void macSetting(){ // initialization of board code for transmition
            uint8_t lsb = EEPROM.read(1);
            uint8_t msb = EEPROM.read(2);
            if((msb == 255) & (lsb == 255)){generateBoardCode();}

            Serial.print("EEPROM serial number: ");
            boardCode = (msb << 8) | lsb;
            Serial.println(boardCode);  
            Serial.println(boardCode, HEX);  
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


void getStateOfSensor() {
    if (gravityEnable) {
        int switchState = digitalRead(GRAVITY);
        int switchState2 = digitalRead(GRAVITY2);
        if ((switchState == LOW) or (switchState2 == LOW)){ 
            if (!switchPressed) {
                switchPressed = true;
                switchPressStartTime = millis();
            } else {
                if (!gravityAlarm && millis() - switchPressStartTime >= gravityThreshold) {
                    gravityAlarm = true;
                }
            }
            switchReleaseStartTime = 0; 
        } else { 
            if (gravityAlarm) {
                if (switchReleaseStartTime == 0) {
                    switchReleaseStartTime = millis();
                } else if (millis() - switchReleaseStartTime >= gravityReleaseThreshold) {
                    gravityAlarm = false;
                }
            }
            switchPressed = false;
        }
    }
}