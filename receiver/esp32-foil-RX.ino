/*
    TecnoFly receiver for Foil Assist system
    2026 writed by Roberto Assiro
    
    This program is free software under the terms of the GNU General Public License as published by the Free Software Foundation.
    VERSION 1.6x May 2025
    ESP32 FOIL ASSIST RECEIVER AND VESC DATA LOGGER
    Connections VESC: GPIO 16= white, 17= black, PPM white = GPIO 4
    Connections to SD CARD CS = GPIO 5
    // EEPROM byte usage: 0=gravity angle; 1-2=board code; 3=direct cut angle; 4=service flag; 8-11=VESC offset; 12-15=life counter; 16,20,24=ax/ay/az offset (float)
    -REBOOT ISSUE .Downgrading to the previous version of esp32 Library board by espressif to version 3.0.7 solved the problem.
    -Library modify: ElegantOTA flag to enable AsyncWebServer mode file ElegantOTA.h
    VERSION history
    -1.65 new pairing system and activated gravity sensor
    -2.00 NO SDCARD EXTERNAL - REPLACED WITH LITTLE FS on - all webserver file in flash - graph by uPlot
    -3.00 ADXL345 implement sensor 3 axis accelerometer and ESP core 3.3.6
    -3.14 ride data format binary
    -3.15 imu file activated and score calc
    -3.40 new web page
    -3.50 vescUart modification stability - Freertos control - battery cutoff control - battery allarm improved 10s 12s
*/
#include <WiFi.h>
#include <esp_now.h>
#include <AsyncTCP.h>
#include <FS.h>
#include <VescUart.h>
#include <ESP32Servo.h>
#include <esp_wifi.h>
#include <ESPAsyncWebServer.h>
#include <HTTPClient.h>
#include <ESP32httpUpdate.h>
#include <EEPROM.h>
#include <DNSServer.h>
#include <ElegantOTA.h> // modificated of elegantOTA.h to enable AsyncWebServer mode
#include <LittleFS.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_ADXL345_U.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#define EEPROM_SIZE 64
#define escpin 4

const char* ssidService     = "tecnofly";
const char* passwordService = "tecnofly";
const char* serverBase = "http://www.eoloonline.it/firmware/webServer";
const char* upgradeServer = "http://www.eoloonline.it/firmware/";
String upgradeFile = "foilAssistReceiver.bin";  //foilAssistReceiver.bin
const char ssidName[] = "TecnoFly ";
char ssid[20];

const char* password = "";
String VERSION = "3.50";
const char* PARAM_INPUT_1 = "number";
const int GRAVITY1 = 33; // gravity sensor 1 for auto lock
const int GRAVITY2 = 32; // gravity sensor 2 for auto lock
Adafruit_ADXL345_Unified accel = Adafruit_ADXL345_Unified(12345);
IPAddress local_IP(192,168,10,1);//Set the IP address of ESP32 itself
IPAddress gateway(192,168,10,1);   //Set the gateway of ESP32 itself
IPAddress subnet(255,255,255,0);  //Set the subnet mask for ESP32 itself
DNSServer dnsServer;
AsyncWebServer server(80);
#define FORMAT_LITTLEFS_IF_FAILED true
VescUart UART;/** Initiate VescUart class */
File file;
int Poles = 14;                  //Poles of motor
// battery 10s
int batEmptyValue = 320;    // value x 10  (32.0V = 3.2V/cella, vuota)
int batFullValue = 412;     // value x 10 - fix 30/08/2026: 10s piena a riposo = 41.2V (4.12V/cella).
                            // Prima 42.0V teorici MAI raggiunti a riposo -> batteria carica non dava mai 99%

// --- Allarme batteria (fix 13/10/2026): soglie in EEPROM, pacco rilevato da batStatus() ---
float battAlarm10s = 31.0;    // soglia allarme 10s (V)
float battAlarm12s = 37.2;    // soglia allarme 12s (V)
float battReset10s = 39.5;    // reset allarme 10s (V) - come da controller
float battReset12s = 47.4;    // reset allarme 12s (V)
bool  battIs12s    = false;   // true se il pacco e 12s (tensione > 43V allo start)
bool  lowBattFlag  = false;   // stato allarme inviato al TX via ESP-NOW
#define EEPROM_BATT_OFF 32   // 32..47: 4 float (soglie/reset)
#define EEPROM_BATT_MAGIC 48 // flag di inizializzazione
#define BATT_CFG_MAGIC 0xA5
// --- Curva throttle (fix 21/10/2026): applicata in RX prima di esc.write() ---
// y = (1-k)*x + k*x^gamma   con x = throttle/99, gamma 0.40..3.00, k 0..1
// Default: gamma 1.0 e k 0 => lineare, comportamento identico a prima.
float throttleCurveGamma = 1.0f;
float throttleCurveMix   = 0.0f;
#define EEPROM_CURVE_GAMMA 52
#define EEPROM_CURVE_MIX   56
#define EEPROM_CURVE_MAGIC 60
#define CURVE_CFG_MAGIC 0x5A

void loadThrottleCurveConfig(){
      if (EEPROM.read(EEPROM_CURVE_MAGIC) != CURVE_CFG_MAGIC) return;
      EEPROM.get(EEPROM_CURVE_GAMMA, throttleCurveGamma);
      EEPROM.get(EEPROM_CURVE_MIX, throttleCurveMix);
      if (!(throttleCurveGamma >= 0.40f && throttleCurveGamma <= 3.00f)) throttleCurveGamma = 1.0f;
      if (!(throttleCurveMix >= 0.0f && throttleCurveMix <= 1.0f)) throttleCurveMix = 0.0f;
      Serial.printf("THROTTLE CURVE: gamma %.2f mix %.2f\n", throttleCurveGamma, throttleCurveMix);
}

void saveThrottleCurveConfig(){
      EEPROM.put(EEPROM_CURVE_GAMMA, throttleCurveGamma);
      EEPROM.put(EEPROM_CURVE_MIX, throttleCurveMix);
      EEPROM.write(EEPROM_CURVE_MAGIC, CURVE_CFG_MAGIC);
      EEPROM.commit();
}

int applyThrottleCurve(int raw){
      if (raw <= 0) return 0;
      if (raw >= 99) return 99;
      float x = raw / 99.0f;
      float e = powf(x, throttleCurveGamma);
      float y = (1.0f - throttleCurveMix) * x + throttleCurveMix * e;
      int out = (int)lroundf(y * 99.0f);
      return constrain(out, 0, 99);
}

void loadBattAlertConfig(){
      if (EEPROM.read(EEPROM_BATT_MAGIC) != BATT_CFG_MAGIC) return;
      EEPROM.get(EEPROM_BATT_OFF,      battAlarm10s);
      EEPROM.get(EEPROM_BATT_OFF + 4,  battAlarm12s);
      EEPROM.get(EEPROM_BATT_OFF + 8,  battReset10s);
      EEPROM.get(EEPROM_BATT_OFF + 12, battReset12s);
      if (!(battAlarm10s > 5 && battAlarm10s < 60)) battAlarm10s = 31.0;
      if (!(battAlarm12s > 5 && battAlarm12s < 60)) battAlarm12s = 37.2;
      if (!(battReset10s > 5 && battReset10s < 60)) battReset10s = 39.5;
      if (!(battReset12s > 5 && battReset12s < 60)) battReset12s = 47.4;
      Serial.printf("BATT ALARM cfg: 10s %.1f/%.1f  12s %.1f/%.1f\n", battAlarm10s, battReset10s, battAlarm12s, battReset12s);
}

void saveBattAlertConfig(){
      EEPROM.put(EEPROM_BATT_OFF,      battAlarm10s);
      EEPROM.put(EEPROM_BATT_OFF + 4,  battAlarm12s);
      EEPROM.put(EEPROM_BATT_OFF + 8,  battReset10s);
      EEPROM.put(EEPROM_BATT_OFF + 12, battReset12s);
      EEPROM.write(EEPROM_BATT_MAGIC, BATT_CFG_MAGIC);
      EEPROM.commit();
}

// battery 12s
int batEmptyValue12s = 384;    // value x 10  (38.4V = 3.2V/cella, vuota)
int batFullValue12s = 495;     // value x 10 - fix 18/08/2026: 12s piena a riposo = 49.5V (4.12V/cella)

// Capacità NOMINALE batteria in Wh (NON è la tensione: batFullValue = V, battFullCapacity = Wh)
uint battFullCapacity = 400;    // Wh - Molicel P45B 21700 - 10s3p
uint battFullCapacity12s = 350; // Wh - 12s (stima 12s2p 21700: 12*3.6V*4.5Ah*2p) 

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

uint directCutDegrees = 0;
bool directCutEnabled = false;

bool directCutActive = false;
unsigned long directCutStart = 0;
const unsigned long DIRECT_CUT_HOLD_MS = 3000;

// ── EVENT LOG ──
#define EVENT_LOG "/events.log"
#define EVENT_LOG_MAX 10000

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
        bool pumping;
        bool boardMoving;   // tavola in movimento (rms > soglia keep-alive) per auto-sleep TX
        uint record;        
        uint16_t trustWatts;   // l_watt_max reale VESC (1500-2200W) — sync trust col TX (fix 20/08/2026)
        int score;             // rider score (fix 20/08/2026) — mostrato sulla VIEW 1 del controller
        bool lowBattFlag;     // allarme batteria calcolato dalla RX (fix 13/10/2026)
//        float watthour;
};
struct vescValues data;

bool espStatus = true;
bool vescReady = false;
bool pumping = false;
bool boardMoving = false;   // tavola in movimento (rms > RIDE_KEEPALIVE_RMS) - inviata al TX
float tempMosfet;
float rpm;
float voltage;
float current;
float motorCurrent;
int power;
float watthour;
float whBaseline = 0;   // Wh VESC all'accensione (fix 10/08/2026: il contatore wattHours non parte da 0)
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
uint16_t rxTrustWatts = 0;        // ultimi watt ricevuti dal TX (1500-2200)
#define MOTOR_POLE_PAIRS 7     // coppie polari motore: ERPM = rpm meccanici x 7 (4300 rpm -> 30100 erpm)
uint16_t rxPropRpm = 0;               // rpm MECCANICI elica ricevuti dal TX
bool     rxPropCw  = true;            // senso richiesto (true = CW/orario)
volatile bool pendingProp = false;  // true = da applicare al VESC nel loop()
volatile bool pendingPower = false; // true = da applicare al VESC nel loop()
portMUX_TYPE rxStateMux = portMUX_INITIALIZER_UNLOCKED;
SemaphoreHandle_t vescUartMutex = nullptr;
SemaphoreHandle_t mcconfMutex = nullptr;

class VescUartGuard {
public:
    explicit VescUartGuard(TickType_t timeout = portMAX_DELAY)
        : locked(vescUartMutex != nullptr && xSemaphoreTake(vescUartMutex, timeout) == pdTRUE) {}
    ~VescUartGuard() { if (locked) xSemaphoreGive(vescUartMutex); }
    bool ownsLock() const { return locked; }
private:
    bool locked;
};

class McconfGuard {
public:
    explicit McconfGuard(TickType_t timeout = portMAX_DELAY)
        : locked(mcconfMutex != nullptr && xSemaphoreTakeRecursive(mcconfMutex, timeout) == pdTRUE) {}
    ~McconfGuard() { if (locked) xSemaphoreGiveRecursive(mcconfMutex); }
    bool ownsLock() const { return locked; }
private:
    bool locked;
};

struct VescConfigRequest {
    uint8_t flags;
    uint16_t watts;
    uint16_t propRpm;
    bool propCw;
};

static constexpr uint8_t VESC_CONFIG_POWER = 0x01;
static constexpr uint8_t VESC_CONFIG_PROP  = 0x02;
QueueHandle_t vescConfigQueue = nullptr;
QueueHandle_t vescConfigResultQueue = nullptr;

// Pacchetto ESP-NOW dal TX: deve matchare TxPkt del trasmettitore
struct RxPkt { int throttle; uint16_t trustWatts; uint16_t propRpm; uint8_t propCw; };
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
bool fileReady = false;
int score;
uint timeRecord = 20;
int record;

//debugging parameters
float C1;
float C2;
float C3;
float C4;

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
char fileNameChar[12]; // spazio per un int decimale e terminatore NUL
unsigned long lastDataVesc;
char dataFileName[20];
char jsonFileName[20];
bool recordEnable = false;
uint inactivityCounter;
bool serviceMode = false;
bool firstConnection = false;
bool oneTimeFlag = false;
uint timeNoRecord = 300; //max time in seconds to record data without activity (throttle o movimento tavola) - il keep-alive accelerometrico in vesc_data.ino resetta il contatore
uint lastAnalysis = 5; //minutes of total time after to do the data analysis every minute

volatile bool pendingServiceFlag = false;
uint vescError;

// ---- RF signal quality ----
volatile int8_t  rssiLast    = 0;    
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
        // Throttle e impostazioni arrivano nello stesso pacchetto del TX.
        // Ignora pacchetti malformati: non leggere mai campi da un RxPkt parziale.
        if (info == nullptr || info->rx_ctrl == nullptr || data == nullptr || len < (int)sizeof(RxPkt)) {
            return;
        }
        receivedData = true;
        int8_t rssi = info->rx_ctrl->rssi;
        portENTER_CRITICAL(&rxStateMux);
        rssiLast = rssi;
        if (packetCount < UINT32_MAX) packetCount++;
        // Media incrementale: mantiene la media di sessione senza accumulare
        // una somma RSSI che può andare in overflow dopo molte ore/giorni.
        if (packetCount > 0) {
            rssiAvg += ((float)rssi - rssiAvg) / (float)packetCount;
        }
        uint64_t total = (uint64_t)packetCount + packetLost;
        lossPercent = (total > 0) ? (100.0f * (float)packetLost / (float)total) : 0.0f;
        lastPacketTime = millis();
        portEXIT_CRITICAL(&rxStateMux);
        // -----------------
        
        RxPkt pkt;
        memcpy(&pkt, data, sizeof(pkt));
        int throttle_value = pkt.throttle;
        portENTER_CRITICAL(&rxStateMux);
        if (pkt.trustWatts >= 1500 && pkt.trustWatts <= 2200 && pkt.trustWatts != rxTrustWatts) {
            rxTrustWatts = pkt.trustWatts;
            pendingPower = true;
        }
        // ELICA: nuova impostazione ricevuta dal TX (rpm meccanici 2500-5500 + senso)
        if (pkt.propRpm >= 2500 && pkt.propRpm <= 5500 &&
            (pkt.propRpm != rxPropRpm || (pkt.propCw != 0) != rxPropCw)) {
            rxPropRpm = pkt.propRpm;
            rxPropCw  = (pkt.propCw != 0);
            pendingProp = true;
        }
        portEXIT_CRITICAL(&rxStateMux);
        EspDelays = millis() - EspLastTimeReceived;

        if (throttle_value == 200 && oneTimeFlag == false){   // command from controller to update receiver from server
            portENTER_CRITICAL(&rxStateMux);
            pendingServiceFlag = true;
            portEXIT_CRITICAL(&rxStateMux);
            throttle_value = 0;
        }

        if((oneTimeFlag) and (throttle_value < 100)){oneTimeFlag = false;}

        // Calcolo throttle e scrittura ESC immediata — priorità massima
        throttle = (throttle_value > 100) ? 0 : map(throttle_value, 0, 99, 0, 180);   // grezzo: logica, display, dati
            int throttleEsc = (throttle_value > 100) ? 0 : map(applyThrottleCurve(throttle_value), 0, 99, 0, 180);   // curvo: SOLO verso il VESC
        if (!directCutActive) {
            esc.write(throttleEsc); // MAIN MOTOR CONTROL POWER — latenza < 1ms
        }
        
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
  }
};

bool setVescParamsFromQuery(const String &qs, TickType_t lockTimeout);
void logEvent(const char* event);

// Le modifiche richieste dal TX vengono applicate fuori dal loop. La coda viene
// svuotata prima di ogni scrittura, così più variazioni ravvicinate diventano
// un'unica configurazione con gli ultimi valori ricevuti.
void vescConfigWorker(void* parameter) {
    VescConfigRequest request = {};
    bool haveRequest = false;
    bool failureReported = false;

    for (;;) {
        if (!haveRequest) {
            if (xQueueReceive(vescConfigQueue, &request, portMAX_DELAY) != pdTRUE) continue;
            haveRequest = true;
        }

        VescConfigRequest newer;
        while (xQueueReceive(vescConfigQueue, &newer, 0) == pdTRUE) {
            if (newer.flags & VESC_CONFIG_POWER) {
                request.watts = newer.watts;
                request.flags |= VESC_CONFIG_POWER;
            }
            if (newer.flags & VESC_CONFIG_PROP) {
                request.propRpm = newer.propRpm;
                request.propCw = newer.propCw;
                request.flags |= VESC_CONFIG_PROP;
            }
        }

        char query[80];
        size_t used = 0;
        query[0] = '\0';
        if (request.flags & VESC_CONFIG_POWER) {
            used = snprintf(query, sizeof(query), "l_watt_max=%u", request.watts);
        }
        if (request.flags & VESC_CONFIG_PROP) {
            uint32_t erpm = (uint32_t)request.propRpm * MOTOR_POLE_PAIRS;
            snprintf(query + used, sizeof(query) - used,
                     "%sl_max_erpm=%lu&m_invert_direction=%u",
                     used ? "&" : "", (unsigned long)erpm, request.propCw ? 0 : 1);
        }

        bool ok = setVescParamsFromQuery(String(query), portMAX_DELAY);
        if (!ok) {
            if (!failureReported) {
                Serial.println("VESC config request failed; will retry");
                failureReported = true;
            }
            vTaskDelay(pdMS_TO_TICKS(5000));
            continue;
        }

        failureReported = false;
        if (vescConfigResultQueue != nullptr) {
            xQueueSend(vescConfigResultQueue, &request, 0);
        }
        haveRequest = false;
        request = {};
    }
}


void setup() {
        Serial.begin(115200);
        vescUartMutex = xSemaphoreCreateMutex();
        mcconfMutex = xSemaphoreCreateRecursiveMutex();
        if (vescUartMutex == nullptr || mcconfMutex == nullptr) {
            Serial.println("Failed to create firmware mutexes");
            return;
        }
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
        vescConfigQueue = xQueueCreate(4, sizeof(VescConfigRequest));
        vescConfigResultQueue = xQueueCreate(4, sizeof(VescConfigRequest));
        if (vescConfigQueue == nullptr || vescConfigResultQueue == nullptr ||
            xTaskCreatePinnedToCore(vescConfigWorker, "vescConfig", 4096, nullptr, 1, nullptr, 0) != pdPASS) {
            Serial.println("VESC config worker unavailable; remote config changes disabled");
            if (vescConfigQueue != nullptr) vQueueDelete(vescConfigQueue);
            if (vescConfigResultQueue != nullptr) vQueueDelete(vescConfigResultQueue);
            vescConfigQueue = nullptr;
            vescConfigResultQueue = nullptr;
        }
        Serial.println("");  
        Serial.println(VERSION); 
//        Serial.println("Setting Access Point... ");
/*
        WiFi.mode(WIFI_AP_STA);  // Set device as Access point
        macSetting();
        Serial.println(WiFi.softAPConfig(local_IP, gateway, subnet) ? "Web Server Ready" : "Web Server Failed!");
        WiFi.softAP(ssid, password);
*/

/////////////////// TEST CHANNEL CHANGE OPTION //////////////////////////////////
        int channel = 1;
        WiFi.mode(WIFI_AP_STA);  // Set device as Access point
        macSetting();
          // 2. Avvio AP con canale
        Serial.println(WiFi.softAPConfig(local_IP, gateway, subnet) ? "Web Server Ready" : "Web Server Failed!");         
        WiFi.softAP(ssid, password, channel);
        WiFi.setTxPower(WIFI_POWER_19_5dBm); 
        esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);  // fixed channel (es. 1)
//        esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_LR);  // enable long range
//////////////////////////////////////////////////////////////////////////
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
     //   Serial.println("✅ Ready");
        readFileNumber();   
        logEvent("✅ SYSTEM BOOT");             
        batStatus();
        readFileMessage();
        readFileRecord();
 
        // Fix 20/08/2026: rxTrustWatts parte dal valore REALE del VESC (mcconf.bin).
        // Prima partiva da 0 -> al primo aggancio il trust dal TX (stesso valore dopo
        // il sync) triggerava pendingPower -> scrittura mcconf al VESC a ogni accensione
        // (loop RX bloccato 3-4s + VESC riavviato -> NO SIGNAL + throttle assente).
        {
            float w0 = 0.0f, e0 = 0.0f;
            uint8_t invertDir = 0;
            readVescStartupConfig(w0, e0, invertDir);
            if (w0 >= 1500.0f && w0 <= 2200.0f) {
                portENTER_CRITICAL(&rxStateMux);
                rxTrustWatts = (uint16_t)w0;
                portEXIT_CRITICAL(&rxStateMux);
                Serial.printf("TRUST init: %dW (da mcconf)\n", rxTrustWatts);
            }
            if (e0 > 0.0f) {
                rxPropRpm = (uint16_t)((e0 / (float)MOTOR_POLE_PAIRS) + 0.5f);
                rxPropCw  = (invertDir == 0);
                Serial.printf("PROP init: %u rpm %s (mcconf)\n", rxPropRpm, rxPropCw ? "CW" : "CCW");
            }
        }

//        Serial.print("Flash size: " + String(LittleFS.totalBytes()));
//        Serial.println(" -> Used: " + String(LittleFS.usedBytes()));
        uint serviceFlag = EEPROM.read(4);
        if (serviceFlag == 0x0A){setupMode();}  // check the service network for upgrading

        thresholdDegrees = EEPROM.read(0);   // to disable the gravity control eeprom location 0 must be zero
        if(thresholdDegrees > 0){checkPositionEnable = true;}
        
        directCutDegrees = EEPROM.read(3);
        if(directCutDegrees > 0){ directCutEnabled = true; }
        
        EEPROM.get(16, axOffset);
        EEPROM.get(20, ayOffset);
        EEPROM.get(24, azOffset);
}


void loop() {
        dnsServer.processNextRequest();
        VescConfigRequest configResult;
        while (vescConfigResultQueue != nullptr &&
               xQueueReceive(vescConfigResultQueue, &configResult, 0) == pdTRUE) {
            if (configResult.flags & VESC_CONFIG_POWER) {
                char buf[48];
                snprintf(buf, sizeof(buf), "Power limit -> %dW", configResult.watts);
                logEvent(buf);
            }
            if (configResult.flags & VESC_CONFIG_PROP) {
                char buf[64];
                int erpm = (int)((uint32_t)configResult.propRpm * MOTOR_POLE_PAIRS);
                snprintf(buf, sizeof(buf), "Propeller -> %urpm %s (%derpm)",
                         configResult.propRpm, configResult.propCw ? "CW" : "CCW", erpm);
                logEvent(buf);
            }
        }
        uint32_t now = millis();
        delays = now - lastTimeReceived;     
        if (delays > timeoutMax){ esc.write(0); lastTimeReceived = now;} // stop motor for safety

        if (directCutActive) {
            esc.write(0);
            if (millis() - directCutStart >= DIRECT_CUT_HOLD_MS) {
                directCutActive = false;
//                logEvent("DIRECT CUT OFF");
            }
        }


        portENTER_CRITICAL(&rxStateMux);
        if (packetCount > 0 && (now - lastPacketTime > 300) && (EspDelays < 10000)) {
            if (packetLost < UINT32_MAX) packetLost++;
            lastPacketTime = now;
            EspDelays = now - EspLastTimeReceived;
            uint64_t total = (uint64_t)packetCount + packetLost;
            lossPercent = (total > 0) ? (100.0f * (float)packetLost / (float)total) : 0.0f;
        }
        portEXIT_CRITICAL(&rxStateMux);
        
        bool doService = false;
        portENTER_CRITICAL(&rxStateMux);
        doService = pendingServiceFlag;
        pendingServiceFlag = false;
        portEXIT_CRITICAL(&rxStateMux);
        if (doService) {
                uint serviceFlag = EEPROM.read(4);
                if (serviceFlag != 0x0A) {
                        EEPROM.write(4, 0x0A); // save flag on eeprom for next reboot
                        EEPROM.commit();
                        logEvent("Setup ON");
                        delay(1000);
                        ESP.restart();
                }
        }
        bool doPower = false;
        uint16_t requestedWatts = 0;
        portENTER_CRITICAL(&rxStateMux);
        doPower = pendingPower;
        requestedWatts = rxTrustWatts;
        pendingPower = false;
        portEXIT_CRITICAL(&rxStateMux);
        // ELICA: applica al VESC (rpm meccanici -> ERPM x coppie polari + senso)
        bool doProp = false;
        uint16_t requestedPropRpm = 0;
        bool requestedPropCw = true;
        portENTER_CRITICAL(&rxStateMux);
        doProp = pendingProp;
        requestedPropRpm = rxPropRpm;
        requestedPropCw = rxPropCw;
        pendingProp = false;
        portEXIT_CRITICAL(&rxStateMux);
        if (doPower || doProp) {
            VescConfigRequest request = {};
            if (doPower) {
                request.flags |= VESC_CONFIG_POWER;
                request.watts = requestedWatts;
            }
            if (doProp) {
                request.flags |= VESC_CONFIG_PROP;
                request.propRpm = requestedPropRpm;
                request.propCw = requestedPropCw;
            }
            if (vescConfigQueue == nullptr || xQueueSend(vescConfigQueue, &request, 0) != pdTRUE) {
                portENTER_CRITICAL(&rxStateMux);
                if (doPower) pendingPower = true;
                if (doProp) pendingProp = true;
                portEXIT_CRITICAL(&rxStateMux);
            }
        }
        if(!firstConnection and throttle > 20 and vescReady){
                firstConnection = true;
                fileReady = true;
                dataFileCreate();   // it creates the data ride file only in the firts use of throttle control
                rideNumber = fileName;  // sync ride number for event log
//                openImuFile();    // IMU file stores the entire data of the accelerometer's fifo
                lifeCounter();  // increment total number of rides counter of the life of the system
                logEvent("RIDE_START ✅");    
        }    
        getTelemetry();
        if((Tminutes > lastAnalysis) and (throttle == 0)){  // ride analysis every minutes
                if(accelerometerConnected){
                    rideAnalysis();
                }else{
                    rideAnalysisNoADXL();
                }
                rideToJson();   // fix 22/10/2026: tiene il riepilogo JSON allineato durante il ride
                lastAnalysis = Tminutes;
        }
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
//            Serial.print("OLD MAC:  ");  
//            Serial.println(WiFi.macAddress());  
            esp_err_t result = esp_wifi_set_mac(WIFI_IF_STA, customMAC);
            if (result == ESP_OK) {
                Serial.print("NEW MAC:  ");
                Serial.println(WiFi.macAddress());                
            } else {
                Serial.print("Error: ");
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
            byte value1 = random(0, 256);
            byte value2 = random(0, 256); 
            EEPROM.write(1, value1);
            EEPROM.write(2, value2);
            EEPROM.put(12, 0);
            EEPROM.commit();  
            delay(1000);
            ESP.restart();
}

void logEvent(const char* event) {
    uint8_t mm = Tminutes;
    uint8_t ss = Tseconds;
    // Se VESC non ha ancora dato dati, calcola da millis()
    if (mm == 0 && ss == 0 && millis() > 5000) {
        unsigned long t = millis() / 1000;
        mm = t / 60;
        ss = t % 60;
    }
    File f = LittleFS.open(EVENT_LOG, FILE_APPEND);
    if (f) {
        f.printf("%u,%02u:%02u,%s\n", rideNumber, mm, ss, event);
        size_t logSize = f.size();
        f.close();
        if (logSize > EVENT_LOG_MAX) {
            // Legge il file, taglia le righe piu' vecchie (~30%), riscrive
            File rf = LittleFS.open(EVENT_LOG, FILE_READ);
            if (rf) {
                String content = rf.readString();
                rf.close();
                // Taglia al primo newline dopo il 30% del file
                size_t cutAt = content.length() * 30 / 100;
                int cutPos = content.indexOf('\n', cutAt);
                if (cutPos > 0) {
                    content = content.substring(cutPos + 1);
                }
                File wf = LittleFS.open(EVENT_LOG, FILE_WRITE);
                if (wf) {
                    wf.print(content);
                    wf.close();
                }
            }
        }
    }
}

