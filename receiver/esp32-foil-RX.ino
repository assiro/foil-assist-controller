//ESP32 FOIL ASSIST RECEIVER AND VESC DATA LOGGER
//WRITTEN BY ROBERTO ASSIRO 11/2023
//Connessioni VESC: GPIO 16=filo bianco, 17=filo nero, PPM filo bianco = GPIO 4
//Connessioni SD CARD CS = GPIO 5
// version Dic 2023
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
#include <AsyncElegantOTA.h>
#include <Arduino_JSON.h>
#include <LoRa.h>

#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

#include <HTTPClient.h>
#include <ESP32httpUpdate.h>
#include <EEPROM.h>

//define the pins used by the transceiver module
#define loraEN 15
#define rst 14
#define dio0 2
#define EEPROM_SIZE 10
#define escpin 4
const int chipSelect = SS;
const char* ssidService     = "tecnofly";
const char* passwordService = "tecnofly";
String upgradeServer = "http://www.eoloonline.it/firmware/foilAssist/";
String upgradeFile = "foilAssistReceiver.bin";
const char ssidName[] = "TecnoFly eBooster ";
char ssid[20];

const char* password = "";
String VERSION = "1.09";
const char* PARAM_INPUT_1 = "number";

IPAddress local_IP(192,168,10,1);//Set the IP address of ESP32 itself
IPAddress gateway(192,168,10,1);   //Set the gateway of ESP32 itself
IPAddress subnet(255,255,255,0);  //Set the subnet mask for ESP32 itself
AsyncWebServer server(80);
#define FORMAT_SPIFFS_IF_FAILED true
JSONVar board;
VescUart UART;/** Initiate VescUart class */
File file;
int Poles = 14;                  //Poles of motor
float BatteryCells = 8;         //Number of cells in Battery

struct vescValues {
  float voltage;  
  float tempMosfet;
  float current;
  int power;
  int batt;
  int file;
  int minutes;
  int seconds;
  uint powerCounter;
  uint powerAverage;
  uint powerIndex;
  float receiverVersion;
};
struct vescValues data;

bool espStatus = true;
bool loraStatus = true;
bool vescReady = false;
float tempMosfet;
float rpm;
float voltage;
float current;
int power;
float amphour;
float watthour;
int batpercentage;
uint batCapacity;
uint percentageOfCapacity;
unsigned long totalWatt;
uint powerCounter;
uint powerAverage;
uint powerIndex;
int fileName;
bool sdcard;
int minutes;
int seconds;
float receiverVersion;

Servo esc; 

//Min and max pulse
int timeoutMax = 1300;
int minPulseRate = 900; 
int maxPulseRate = 2150;
int throttle = 0;
bool recievedData = false;
uint32_t lastTimeReceived = 0;
uint32_t EspLastTimeReceived = 0;
uint32_t LoRaLastTimeReceived = 0;
uint32_t LoRaLastTimeCheck = 0;
uint32_t delays = 0;
uint32_t LoRaDelays = 0;
uint32_t EspDelays = 0;
int rowCounter = 0;
char fileNameChar[1];
unsigned long lastDataVesc;
char dataFileName[20];
bool recordEnable = false;
uint inactivityCounter;
bool serviceMode = false;
bool firtsConnection = false;
//RECEIVER MAC ADDRESS
uint8_t newMACAddress[] = {0x4C, 0x11, 0xAE, 0x0D, 0xE5, 0x35}; //RECEIVER 1
//TRANSMITTER MAC ADDRESS
uint8_t broadcastAddress[] = {0xA4, 0xCF, 0x12, 0xDC, 0xB5, 0x8E};// trasmettitore 1

byte localAddress;// LoRa address of this device
byte destination;// LoRa destination to send to
typedef struct struct_message {   // Structure to send data
  float a;
  float inpVoltage;
  float tempMosfet;
  float avgInputCurrent;
  float watt;
} struct_message;

struct_message myData;// Create a struct_message called myData
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
//      delays = millis() - lastTimeReceived; 
//      EspDelays = millis() - EspLastTimeReceived;

      if (throttle_value == 200){
          esc.write(0);
          EEPROM.write(4, 0x0A);
          EEPROM.commit();
          Serial.println("State setup ON saved in flash memory"); //IT NEEDS A REBOOT
          delay(3000);
//          ESP.restart();
      }
      throttle = map(throttle_value, 0, 99, 0, 180); // Write the PWM signal to the ESC (0-255).
      if(throttle_value == 200){throttle = 0;}
      esc.write(int(throttle));
      if (throttle_value > 20){
          recordEnable = true;
          inactivityCounter = 0;
          serviceMode = false;
      }
      EspLastTimeReceived = millis();
      lastTimeReceived = millis();       
      /*
      Serial.print("Data by ESP ");
      Serial.print(throttle_value);
      Serial.print(" - delay: ");
      Serial.println(EspDelays);*/
}

void setup() {
      Serial.begin(115200);
      Serial2.begin(115200, SERIAL_8N1, 16, 17);// VESC GPIO 16=RX and 17=TX */
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

      uint EEPROMserialNumber = EEPROM.read(0);
      Serial.print("EEPROM serial number: ");
    
      if (EEPROMserialNumber > 5){EEPROMserialNumber = 1;}
      if(EEPROMserialNumber == 1){
            newMACAddress[5] = 0x35;
            broadcastAddress[5] = 0x8E; 
      }else if(EEPROMserialNumber == 2){
            newMACAddress[5] = 0x36;
            broadcastAddress[5] = 0x8F;
      }else if(EEPROMserialNumber == 3){
            newMACAddress[5] = 0x37; 
            broadcastAddress[5] = 0x03;
      }else if(EEPROMserialNumber == 4){  
            newMACAddress[5] = 0x38;
            broadcastAddress[5] = 0x04;
      }else if(EEPROMserialNumber == 5){
            broadcastAddress[5] = 0x05;
            newMACAddress[5] = 0x39; 
      }
      localAddress = newMACAddress[5];// LoRa address of this device
      destination = broadcastAddress[5];// LoRa destination to send to      
      Serial.println(EEPROMserialNumber);  

      strcpy(ssid, ssidName);
      char serialNumber[2]; 
      itoa(EEPROMserialNumber, serialNumber, 10);
      strcat(ssid, serialNumber);

      Serial.print("--- MAC:  ");  
      Serial.println(WiFi.macAddress());  
      esp_wifi_set_mac(WIFI_IF_STA, &newMACAddress[0]);
      Serial.print("NEW MAC:  ");
      Serial.println(WiFi.macAddress());
      Serial.println("Setting Access Point... ");
      Serial.println(WiFi.softAPConfig(local_IP, gateway, subnet) ? "Web Server Ready" : "Web Server Failed!");
      WiFi.softAP(ssid, password);
      Serial.println(ssid); 
      initSDCard();

      // Route for root / web page
      server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(SD, "/index.html", "text/html");
      });
  
      server.on("/ver", HTTP_GET, [](AsyncWebServerRequest *request){
      request->send_P(200, "text/plain", VERSION.c_str());
      });

      server.on("/rx_number", HTTP_GET, [](AsyncWebServerRequest *request){
        uint rxNumber = EEPROM.read(0);
        char serialNumber[2]; 
        itoa(rxNumber, serialNumber, 10);
        request->send_P(200, "text/plain", serialNumber);
      });

      server.on("/fileNumber", HTTP_GET, [](AsyncWebServerRequest *request){
      request->send_P(200, "text/plain", fileNameChar);
      });

/////////////////////////////////

// Send a GET request to <ESP_IP>/serial?number=5>
  server.on("/serial", HTTP_GET, [] (AsyncWebServerRequest *request) {
    String inputMessage1;
    // GET input1 value on <ESP_IP>/update?output=<inputMessage1>&state=<inputMessage2>
    if (request->hasParam(PARAM_INPUT_1)) {
      inputMessage1 = request->getParam(PARAM_INPUT_1)->value();
//      digitalWrite(inputMessage1.toInt(), inputMessage2.toInt());
    }
    else {
      inputMessage1 = "No message sent";
    }
    Serial.print("Serial Number setting to: ");
    Serial.println(inputMessage1);
    request->send(200, "text/plain", "Serial Number setting to: " + inputMessage1);
    byte newSerialNumber = inputMessage1.toInt();
    Serial.print("Writing serial number for new device: ");
    Serial.println(newSerialNumber);
    EEPROM.write(0, newSerialNumber);
    EEPROM.commit();
    delay(2000);
    Serial.print("Rebooting...");
    ESP.restart(); 
  });

      server.serveStatic("/", SD, "/");
      server.begin();      
      AsyncElegantOTA.begin(&server);

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

    //setup LoRa transceiver module
      LoRa.setPins(loraEN, rst, dio0);
      if (!LoRa.begin(433E6)) {
            Serial.println("Starting LoRa failed!");
      }else{
            LoRa.setPreambleLength(6);
            LoRa.setTxPower(20, PA_OUTPUT_PA_BOOST_PIN);
            LoRa.setSpreadingFactor(8); // 8 max
//            LoRa.setGain(6); //between 0 and 6. If gain is 0, AGC will be enabled 
//            LoRa.setSignalBandwidth(62.5E3); // default 125E3 - 62.5E3 - 41.7E3 - 31.25E3
            // Change sync word (0xF3) to match the receiver
      //      LoRa.setSyncWord(0xF3);
            Serial.println("LoRa Initializing OK!");
      }
 
      delay(500);
      batStatus();
//      dataFileCreate();
      delay(500);
      readFileNumber();
      uint serviceFlag = EEPROM.read(4);
      if (serviceFlag == 0x0A){
            setupMode();  // check the service network for upgrading
      }

}

void loop() {
    EspDelays = millis() - EspLastTimeReceived;
    LoRaDelays = millis() - LoRaLastTimeReceived;
    delays = millis() - lastTimeReceived;     
    if (delays > timeoutMax){           
            esc.write(0);
            lastTimeReceived = millis();
    } 
    if(millis() - LoRaLastTimeCheck > 80){  // every 80ms read LoRa register
          if(firtsConnection == false and throttle > 20 and vescReady == true){
                  firtsConnection = true;
                  dataFileCreate();// it creates the log file only in the firts use of throttle control
          }
          onReceive(LoRa.parsePacket()); 
          LoRaLastTimeCheck = millis();
    }
    getVescData();
}

void onReceive(int packetSize) {
//      LoRaDelays = millis() - LoRaLastTimeReceived;
      if (LoRaDelays > 1500){loraStatus = true;}      
      if (packetSize == 0) return;          // if there's no packet, return
      int recipient = LoRa.read();          // recipient address
      byte sender = LoRa.read();            // sender address
      String incoming = "";
      while (LoRa.available()) {
            incoming += (char)LoRa.read();
      }
/* 
    Serial.print("destination byte: ");
    Serial.print(recipient, HEX);
    Serial.print(" - sender byte: ");
    Serial.println(sender, HEX);
*/  
  // if the recipient isn't this device or broadcast,
   if (recipient != localAddress && sender != destination) {
//        Serial.println("This message is not for me.");
    return;                             // skip rest of function
  }
  LoRaLastTimeReceived = millis();
  lastTimeReceived = millis();
  int loraData = (incoming.toInt());
  throttle = map(loraData, 0, 99, 0, 180); // Write the PWM signal to the ESC (0-255).
  if(EspDelays > 1300){
//      Serial.print("ESP FAILS! Lora delay: ");
      esc.write(int(throttle));
//      Serial.println(LoRaDelays);
  }
/*  Serial.print("Data by LoRa: ");
  Serial.print(loraData);
//  Serial.print(" - ");
//  Serial.print(throttle);
  //Serial.print(" - RSSI: " + String(LoRa.packetRssi()));
  //Serial.print(" - Snr: " + String(LoRa.packetSnr()));
  Serial.print(" - delay: ");
  Serial.println(LoRaDelays);*/
  loraStatus = false;
}

void setupMode(){
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
