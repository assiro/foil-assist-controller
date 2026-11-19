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
    -3.50 vescUatr modification for stability - Freertos control - battery cutoff control - battery allarm improved 10s 12s
    -3.51 improvement battery percentage scale - ottimization
    -3.52 ADXL345 axis remapping GY291 and GY85 module implementation
    -3.53 Cardio device on BLE
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

// ---- Debug su seriale (fix 31/10/2026): 0 = tutto il debug e' compilato fuori ----
#define ENABLE_DEBUG_LOG 0
#if ENABLE_DEBUG_LOG
  #define DBG(...)      Serial.printf(__VA_ARGS__)
  #define DBGLN(...)    Serial.println(__VA_ARGS__)
  #define DBGPRINT(...) Serial.print(__VA_ARGS__)
#else
  #define DBG(...)      do {} while (0)
  #define DBGLN(...)    do {} while (0)
  #define DBGPRINT(...) do {} while (0)
#endif

#define EEPROM_SIZE 64
#define escpin 4

const char* ssidService     = "tecnofly";
const char* passwordService = "tecnofly";
const char* serverBase = "http://www.eoloonline.it/firmware/webServer";
const char* upgradeServer = "http://www.eoloonline.it/firmware/";
String upgradeFile = "foilAssistReceiver.bin";  
const char ssidName[] = "TecnoFly ";
char ssid[20];

const char* password = "";
String VERSION = "3.57";   // 19/11/2026: monitor heap (3 stati) + log motivo riavvio
// Marcatore di build: la pagina web lo mostra, cosi' si vede subito se il
// firmware caricato e' quello nuovo o se e' rimasto quello di prima.
const char* RX_BUILD = __DATE__ " " __TIME__;
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
#define EEPROM_AXIS_MAGIC 28   // 28: magic mappa assi accelerometro
#define EEPROM_AXIS_MAP   29   // 29..31: mappa X/Y/Z (sorgente<<1 | invertito)
#define EEPROM_TXCFG      61   // 61: 1 = il controller puo' modificare la config VESC (default 0)
#define EEPROM_RADIOTMO   62   // 62: timeout radio in unita' da 100 ms (5..15)
#define EEPROM_CFGFILE   63   // 63: file config motore attivo (0 = mcconf.bin, 1 = focconf.bin)
char    motorCfgPath[20] = "/mcconf.bin";   // file config motore attivo
uint8_t motorCfgActive   = 0;               // 0 = mcconf.bin, 1 = focconf.bin
#define CURVE_CFG_MAGIC 0x5A

void loadThrottleCurveConfig(){
      if (EEPROM.read(EEPROM_CURVE_MAGIC) != CURVE_CFG_MAGIC) return;
      EEPROM.get(EEPROM_CURVE_GAMMA, throttleCurveGamma);
      EEPROM.get(EEPROM_CURVE_MIX, throttleCurveMix);
      if (!(throttleCurveGamma >= 0.40f && throttleCurveGamma <= 3.00f)) throttleCurveGamma = 1.0f;
      if (!(throttleCurveMix >= 0.0f && throttleCurveMix <= 1.0f)) throttleCurveMix = 0.0f;
      DBG("THROTTLE CURVE: gamma %.2f mix %.2f\n", throttleCurveGamma, throttleCurveMix);
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
      DBG("BATT ALARM cfg: 10s %.1f/%.1f  12s %.1f/%.1f\n", battAlarm10s, battReset10s, battAlarm12s, battReset12s);
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
#define EVENT_LOG_MAX 30000

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
        uint8_t motorCfg;     // config motore attiva (MOTORCFG_BLDC / MOTORCFG_FOC): sync menu TX
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
// --- Fix rapido batteria (31/10/2026): percentuale monotona, senza salti ---
float consumedWhAcc = 0.0f;   // Wh consumati accumulati (solo delta positivi)
float whLastSample = -1.0f;   // ultimo wattHours letto dal VESC
int   socRaw    = 99;         // % dal conteggio Wh
int   socLatch  = 99;         // tetto della sessione: puo solo scendere
int   socOut    = 99;         // % filtrata inviata al controller via ESP-NOW
int   socEmptyV = 310;        // tensione pacco vuoto in 0,1 V (dal pacco rilevato)
int   socFullV  = 420;        // tensione pacco pieno in 0,1 V
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
volatile bool pendingPower = false;
bool vescCfgFromTx = false;   // allow controller to change VESC config (EEPROM 61, default OFF)
// Config motore richiesta dal controller (0 = mcconf.bin, 1 = focconf.bin).
// Il cambio arriva nel padding del pacchetto come valore "coded", cosi' un
// controller vecchio (byte a 0) non puo' far scattare un cambio per caso.
volatile uint8_t rxMotorCfgReq = 0;
volatile bool    pendingMotorCfg = false;
volatile uint32_t motorCfgBlockUntil = 0;   // dopo un tentativo fallito, niente retry per 15s
// Ultimi valori ricevuti dal controller. Servono a distinguere un comando
// NUOVO dal valore che il TX ripete in ogni pacchetto (edge detection):
// senza questo, una scelta fatta dalla pagina web veniva annullata dal
// pacchetto successivo del controller (conflitto segnalato 12/11/2026).
uint16_t lastTxTrust    = 0;
uint16_t lastTxPropRpm  = 0;
uint8_t  lastTxPropCw   = 0xFF;
uint8_t  lastTxMotorCfg = 0xFF;
#define MOTORCFG_BLDC 0xA0
#define MOTORCFG_FOC  0xA1
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
    uint8_t motor;     // 0 = mcconf.bin, 1 = focconf.bin
    uint8_t motorOk;   // esito del cambio config (solo per il log)
};

static constexpr uint8_t VESC_CONFIG_POWER = 0x01;
static constexpr uint8_t VESC_CONFIG_PROP  = 0x02;
static constexpr uint8_t VESC_CONFIG_MOTOR = 0x04;
QueueHandle_t vescConfigQueue = nullptr;
QueueHandle_t vescConfigResultQueue = nullptr;

// Pacchetto ESP-NOW dal TX: deve matchare TxPkt del trasmettitore
struct RxPkt { int throttle; uint16_t trustWatts; uint16_t propRpm; uint8_t propCw; uint8_t heartRate; uint8_t hrValid; uint8_t motorCfg; };
float voltageOffset;
uint8_t rxHeartRate = 0;   // bpm dal controller (BLE fascia)
bool    rxHrValid   = false;
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
        // Retrocompatibilita (fix 27/10/2026): il vecchio controller ESP8266 (v4)
        // invia SOLO un int con il throttle (4 byte). Accettiamo entrambi i formati.
        if (info == nullptr || info->rx_ctrl == nullptr || data == nullptr) {
            return;
        }
        const bool legacyTx = (len == (int)sizeof(int));
        if (!legacyTx && len < (int)sizeof(RxPkt)) {
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
        if (legacyTx) {
            // Vecchio ESP8266: solo throttle, nessun trust/elica -> campi a zero
            memset(&pkt, 0, sizeof(pkt));
            memcpy(&pkt.throttle, data, sizeof(int));
        } else {
            memcpy(&pkt, data, sizeof(pkt));
        }
        int throttle_value = pkt.throttle;
        rxHeartRate = pkt.heartRate;
        rxHrValid   = (pkt.hrValid != 0);
        portENTER_CRITICAL(&rxStateMux);
        // I campi di configurazione arrivano in OGNI pacchetto del controller:
        // vanno trattati come comandi a varco (edge), non come stato.
        // Confrontarli con lo stato della ricevente faceva si' che una scelta
        // fatta dalla pagina web venisse riportata indietro dal pacchetto
        // successivo (conflitto pagina web <-> menu controller, 12/11/2026).
        // EEPROM SOVRANA (richiesta 13/11/2026): il primo valore valido che si
        // vede dal controller serve solo ad allinearsi, non a comandare.
        // Cosi' un controller nuovo o appena acceso non puo' cambiare i valori
        // della ricevente: si cambia solo quando il controller CAMBIA davvero.
        if (vescCfgFromTx && !legacyTx && pkt.trustWatts >= 1500 && pkt.trustWatts <= 2200) {
            if (lastTxTrust == 0) {
                lastTxTrust = pkt.trustWatts;          // allineamento, nessuna azione
            } else if (pkt.trustWatts != lastTxTrust) {
                lastTxTrust = pkt.trustWatts;
                if (pkt.trustWatts != rxTrustWatts) {
                    rxTrustWatts = pkt.trustWatts;
                    pendingPower = true;
                }
            }
        }
        // ELICA: stesso principio.
        if (vescCfgFromTx && !legacyTx && pkt.propRpm >= 2500 && pkt.propRpm <= 5500) {
            uint8_t cw = (pkt.propCw != 0) ? 1 : 0;
            if (lastTxPropCw == 0xFF) {
                lastTxPropRpm = pkt.propRpm;           // allineamento, nessuna azione
                lastTxPropCw  = cw;
            } else if (pkt.propRpm != lastTxPropRpm || cw != lastTxPropCw) {
                lastTxPropRpm = pkt.propRpm;
                lastTxPropCw  = cw;
                if (pkt.propRpm != rxPropRpm || (cw != 0) != rxPropCw) {
                    rxPropRpm = pkt.propRpm;
                    rxPropCw  = (cw != 0);
                    pendingProp = true;
                }
            }
        }
        // CONFIG MOTORE dal TX: 0xA0 = BLDC (mcconf.bin), 0xA1 = FOC (focconf.bin).
        // Primo pacchetto = allineamento; poi solo variazioni reali.
        if (vescCfgFromTx && !legacyTx &&
            (pkt.motorCfg == MOTORCFG_BLDC || pkt.motorCfg == MOTORCFG_FOC)) {
            if (lastTxMotorCfg == 0xFF) {
                lastTxMotorCfg = pkt.motorCfg;         // allineamento, nessuna azione
            } else if (pkt.motorCfg != lastTxMotorCfg) {
                lastTxMotorCfg = pkt.motorCfg;
                uint8_t want = (pkt.motorCfg == MOTORCFG_FOC) ? 1 : 0;
                if (want != motorCfgActive && !pendingMotorCfg && millis() > motorCfgBlockUntil) {
                    rxMotorCfgReq = want;
                    pendingMotorCfg = true;
                }
            }
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
        DBGPRINT("Throttle: ");
        DBGPRINT(throttle_value);
        DBGPRINT(" - period: ");
        DBGLN(EspDelays);  */
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
            if (newer.flags & VESC_CONFIG_MOTOR) {
                request.motor = newer.motor;
                request.flags |= VESC_CONFIG_MOTOR;
            }
        }

        // Cambio configurazione motore: seleziona il file e lo scrive nel VESC.
        // Va fatto PRIMA di watt/elica, cosi' gli altri valori si applicano al
        // file appena attivato. Se il file manca non si tocca nulla.
        if (request.flags & VESC_CONFIG_MOTOR) {
            uint8_t idx = request.motor;
            uint8_t prev = motorCfgActive;
            bool ok = false;
            if (motorCfgExists(idx)) {
                motorCfgSet(idx);
                // La scrittura puo' incrociare la telemetria o una lettura di
                // versione andata a vuoto: 3 tentativi prima di dichiarare il
                // fallimento. Senza retry il file restava selezionato ma il
                // VESC non veniva programmato (bug segnalato 10/11/2026).
                vescFwCacheInvalidate();
                for (int attempt = 0; attempt < 3 && !ok; attempt++) {
                    if (attempt) vTaskDelay(pdMS_TO_TICKS(300 * attempt));
                    ok = flashMcconfToVesc();
                    DBG("MOTORCFG: tentativo %d su %s -> %s\n",
                        attempt + 1, motorCfgFileName(idx), ok ? "OK" : "fallito");
                }
                // Se il VESC non ha accettato la config, si torna al file di
                // prima: il menu e la pagina web devono mostrare la verita'.
                if (!ok) {
                    motorCfgSet(prev);
                    motorCfgBlockUntil = millis() + 15000;
                }
            } else {
                DBG("MOTORCFG: %s mancante, cambio annullato\n", motorCfgFileName(idx));
                motorCfgBlockUntil = millis() + 15000;
            }
            VescConfigRequest res = {};
            res.flags = VESC_CONFIG_MOTOR;
            res.motor = idx;
            res.motorOk = ok ? 1 : 0;
            if (vescConfigResultQueue != nullptr) {
                xQueueSend(vescConfigResultQueue, &res, 0);
            }
            request.flags &= ~VESC_CONFIG_MOTOR;
            if (!ok) {
                request.flags = 0;
                haveRequest = false;
                request = {};
                continue;
            }
            // Solo cambio config: niente query vuota (evita una seconda
            // scrittura identica al VESC).
            if (!(request.flags & (VESC_CONFIG_POWER | VESC_CONFIG_PROP))) {
                haveRequest = false;
                request = {};
                continue;
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
                DBGLN("VESC config request failed; will retry");
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
  #if ENABLE_DEBUG_LOG
      Serial.begin(115200);
#endif
        vescUartMutex = xSemaphoreCreateMutex();
        mcconfMutex = xSemaphoreCreateRecursiveMutex();
        if (vescUartMutex == nullptr || mcconfMutex == nullptr) {
            DBGLN("Failed to create firmware mutexes");
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
            DBGLN("LittleFS Mount Failed");
            return;
        }
        vescConfigQueue = xQueueCreate(4, sizeof(VescConfigRequest));
        vescConfigResultQueue = xQueueCreate(4, sizeof(VescConfigRequest));
        if (vescConfigQueue == nullptr || vescConfigResultQueue == nullptr ||
            xTaskCreatePinnedToCore(vescConfigWorker, "vescConfig", 4096, nullptr, 1, nullptr, 0) != pdPASS) {
            DBGLN("VESC config worker unavailable; remote config changes disabled");
            if (vescConfigQueue != nullptr) vQueueDelete(vescConfigQueue);
            if (vescConfigResultQueue != nullptr) vQueueDelete(vescConfigResultQueue);
            vescConfigQueue = nullptr;
            vescConfigResultQueue = nullptr;
        }
        DBGLN("");  
        DBGLN(VERSION); 
//        Serial.println("Setting Access Point... ");
/*
        WiFi.mode(WIFI_AP_STA);  // Set device as Access point
        macSetting();
        DBGLN(WiFi.softAPConfig(local_IP, gateway, subnet) ? "Web Server Ready" : "Web Server Failed!");
        WiFi.softAP(ssid, password);
*/

/////////////////// TEST CHANNEL CHANGE OPTION //////////////////////////////////
        int channel = 1;
        WiFi.mode(WIFI_AP_STA);  // Set device as Access point
        macSetting();
          // 2. Avvio AP con canale
        bool apConfigured = WiFi.softAPConfig(local_IP, gateway, subnet);
        DBGLN(apConfigured ? "Web Server Ready" : "Web Server Failed!");
        WiFi.softAP(ssid, password, channel);
        WiFi.setTxPower(WIFI_POWER_19_5dBm); 
        esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);  // fixed channel (es. 1)
//        esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_LR);  // enable long range
//////////////////////////////////////////////////////////////////////////
        DBGLN(ssid); 
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
            DBGLN("Error initializing ESP-NOW");
            return;
        }else{
            DBGLN("ESP-NOW initializing OK!");
        }
        esp_now_register_send_cb(OnDataSent);
        esp_now_register_recv_cb(onReceiveData);
        // Register peer
        memcpy(peerInfo.peer_addr, broadcastAddress, 6);
        peerInfo.channel = 0;  
        peerInfo.encrypt = false;
        // Add peer        
        if (esp_now_add_peer(&peerInfo) != ESP_OK){
            DBGLN("Failed to add peer");
            return;
        }
        delay(600);
     //   Serial.println("✅ Ready");
        readFileNumber();   
        logEvent("✅ SYSTEM BOOT");             
        // Diagnostica 18/11/2026: registra PERCHE' la scheda si e' riavviata.
        // Se in acqua si blocca, il log dice se e' stato un panic, un watchdog
        // o un brownout. Senza questo dato si tira a indovinare.
        {
            const char* rr = "other";
            switch (esp_reset_reason()) {
                case ESP_RST_POWERON:   rr = "power-on";  break;
                case ESP_RST_SW:        rr = "software";  break;
                case ESP_RST_PANIC:     rr = "PANIC";     break;
                case ESP_RST_INT_WDT:   rr = "INT_WDT";   break;
                case ESP_RST_TASK_WDT:  rr = "TASK_WDT";  break;
                case ESP_RST_WDT:       rr = "WDT";       break;
                case ESP_RST_BROWNOUT:  rr = "BROWNOUT";  break;
                case ESP_RST_DEEPSLEEP: rr = "deepsleep"; break;
                case ESP_RST_EXT:       rr = "external";  break;
            }
            char bootMsg[64];
            snprintf(bootMsg, sizeof(bootMsg), "BOOT reason=%s", rr);
            logEvent(bootMsg);
            heapCheck(true);   // verdetto memoria all'accensione
        }
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
                DBG("TRUST init: %dW (da mcconf)\n", rxTrustWatts);
            }
            if (e0 > 0.0f) {
                rxPropRpm = (uint16_t)((e0 / (float)MOTOR_POLE_PAIRS) + 0.5f);
                rxPropCw  = (invertDir == 0);
                DBG("PROP init: %u rpm %s (mcconf)\n", rxPropRpm, rxPropCw ? "CW" : "CCW");
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
        // Flag: il controller puo' scrivere la config VESC (default OFF)
        vescCfgFromTx = (EEPROM.read(EEPROM_TXCFG) == 1);
        // Timeout radio (500..1500 ms, default 1500)
        {
            uint8_t to = EEPROM.read(EEPROM_RADIOTMO);
            timeoutMax = (to >= 5 && to <= 15) ? (int)to * 100 : 1500;
        }
        
        EEPROM.get(16, axOffset);
        EEPROM.get(20, ayOffset);
        EEPROM.get(24, azOffset);
        axisMapLoad();   // mappa assi accelerometro da EEPROM (default se assente)
        motorCfgLoad();  // file config motore attivo: mcconf.bin (default) o focconf.bin
}


void loop() {
        dnsServer.processNextRequest();
        heapCheck(false);   // verdetto memoria, al massimo 1 riga ogni 10 s
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
            if (configResult.flags & VESC_CONFIG_MOTOR) {
                char buf[72];
                if (configResult.motorOk) {
                    snprintf(buf, sizeof(buf), "Motor cfg -> %s",
                             configResult.motor ? "FOC (focconf.bin)" : "BLDC (mcconf.bin)");
                } else {
                    snprintf(buf, sizeof(buf), "Motor cfg FAILED -> %s (VESC not programmed)",
                             configResult.motor ? "focconf.bin" : "mcconf.bin");
                }
                logEvent(buf);
            }
        }
        uint32_t now = millis();
        delays = now - lastTimeReceived;     
        if (delays > timeoutMax){ esc.write(0); lastTimeReceived = now; /*logEvent("SIGNAL time out!");*/ } // stop motor for safety

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
        bool doMotor = false;
        uint8_t requestedMotor = 0;
        portENTER_CRITICAL(&rxStateMux);
        doMotor = pendingMotorCfg;
        requestedMotor = rxMotorCfgReq;
        pendingMotorCfg = false;
        portEXIT_CRITICAL(&rxStateMux);
        if (doMotor) {
            char mbuf[56];
            snprintf(mbuf, sizeof(mbuf), "Motor cfg request -> %s",
                     requestedMotor ? "FOC" : "BLDC");
            logEvent(mbuf);
        }
        if (doPower || doProp || doMotor) {
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
            if (doMotor) {
                request.flags |= VESC_CONFIG_MOTOR;
                request.motor = requestedMotor;
            }
            if (vescConfigQueue == nullptr || xQueueSend(vescConfigQueue, &request, 0) != pdTRUE) {
                portENTER_CRITICAL(&rxStateMux);
                if (doPower) pendingPower = true;
                if (doProp) pendingProp = true;
                if (doMotor) pendingMotorCfg = true;
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
            DBGPRINT("EEPROM serial number: ");
            boardCode = (msb << 8) | lsb;
            DBGLN(boardCode);  
            strcpy(ssid, ssidName);
            char serialNumber[4]; 
            itoa(boardCode, serialNumber, 10);
            strcat(ssid, serialNumber);
            broadcastAddress[4] = msb; 
            broadcastAddress[5] = lsb;  
            DBGPRINT("BROADCAST MAC ADDRESS: ");
            for (int i = 0; i < 6; i++) {
                if (i > 0) DBGPRINT(":");
                DBG("%02X", broadcastAddress[i]);
            }
            DBGLN();
            uint8_t customMAC[6] = {0x4C, 0x11, 0xAE, 0x0D, msb, lsb};//RECEIVER MAC ADDRESS
//            Serial.print("OLD MAC:  ");  
//            Serial.println(WiFi.macAddress());  
            esp_err_t result = esp_wifi_set_mac(WIFI_IF_STA, customMAC);
            if (result == ESP_OK) {
                DBGPRINT("NEW MAC:  ");
                DBGLN(WiFi.macAddress());                
            } else {
                DBGPRINT("Error: ");
                DBGLN(result);
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

// ======================= MONITOR MEMORIA (19/11/2026) =======================
// Tre stati, due soglie. Al boot una riga con il verdetto; durante l'uso si
// logga SOLO quando lo stato cambia, con isteresi, cosi' il log non si riempie
// e il dato resta leggibile a colpo d'occhio.
//   OK   : heap libero >= 60 KB
//   WARN : heap libero 20..60 KB
//   LOW  : heap libero < 20 KB  -> a rischio di allocazioni fallite
#define HEAP_OK_KB    60
#define HEAP_WARN_KB  20
#define HEAP_HYST_KB   5     // margine per tornare su di stato (evita lampeggi)
#define HEAP_PERIOD_MS 10000UL

static uint8_t       heapState = 0;
static unsigned long heapLastCheck = 0;

void heapCheck(bool boot) {
    unsigned long now = millis();
    if (!boot && (now - heapLastCheck < HEAP_PERIOD_MS)) return;
    heapLastCheck = now;

    uint32_t freeB = ESP.getFreeHeap();
    uint32_t kb    = freeB / 1024UL;

    uint8_t target;
    if (freeB >= (HEAP_OK_KB * 1024UL))        target = 0;
    else if (freeB >= (HEAP_WARN_KB * 1024UL)) target = 1;
    else                                       target = 2;

    if (!boot) {
        if (target > heapState) {
            // peggiora: si segnala subito
        } else if (target < heapState) {
            // migliora: serve il margine di isteresi perche' sia credibile
            uint32_t need = (heapState == 1 ? HEAP_OK_KB : HEAP_WARN_KB) * 1024UL
                          + HEAP_HYST_KB * 1024UL;
            if (freeB < need) return;
        } else {
            return;   // stesso stato: nessuna riga
        }
    }
    heapState = target;

    const char* verdict = (target == 0) ? "ok" : (target == 1) ? "WARN" : "LOW";
    char msg[64];
    snprintf(msg, sizeof(msg), "HEAP %s %s free=%uK min=%uK",
             verdict, boot ? "boot" : "run",
             (unsigned)kb, (unsigned)(ESP.getMinFreeHeap() / 1024UL));
    logEvent(msg);
}

// Stato corrente del monitor memoria, per la card di expert.html.
uint8_t heapStateNow() { return heapState; }

void logEvent(const char* event) {
    // Protezione 18/11/2026 --------------------------------------------------
    // logEvent scrive su LittleFS: se viene chiamata ad alta frequenza (per
    // esempio dal ciclo dell'accelerometro) la flash si satura di scritture.
    // Peggio: superata EVENT_LOG_MAX ogni singola chiamata rilegge e riscrive
    // TUTTO il file, e LittleFS blocca il task durante erase/write: il loop non
    // alimenta piu' ESC, WiFi ed ESP-NOW e la scheda sembra morta, fino al
    // distacco della batteria. Qui si limita a 4 eventi al secondo e si
    // scartano i duplicati ravvicinati: il log resta utile ma non puo' piu'
    // affondare il sistema.
    static unsigned long logWinStart = 0;
    static uint8_t       logWinCount = 0;
    static unsigned long lastLogMs   = 0;
    static char          lastEvt[40] = "";
    unsigned long now = millis();
    if (now - logWinStart > 1000) { logWinStart = now; logWinCount = 0; }
    if (logWinCount >= 4) return;                       // tetto: 4 al secondo
    if (strncmp(lastEvt, event, sizeof(lastEvt) - 1) == 0 &&
        (now - lastLogMs) < 3000) return;               // duplicato ravvicinato
    logWinCount++;
    lastLogMs = now;
    strncpy(lastEvt, event, sizeof(lastEvt) - 1);
    lastEvt[sizeof(lastEvt) - 1] = '\0';
    // -----------------------------------------------------------------------

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
                size_t cutAt = content.length() * 50 / 100;
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

