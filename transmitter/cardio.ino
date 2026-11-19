#if ENABLE_CARDIO
// cardio.ino — fascia cardiaca BLE (Magene) sul controller ESP32-C3
// Servizio HRS 0x180D, caratteristica 0x2A37 (notify).
// Non bloccante: tutto gira in un task dedicato a priorita' bassa (1),

#include <NimBLEDevice.h>

extern volatile uint8_t heartRate;     // definiti nel main sketch
extern volatile bool    hrValid;
static volatile uint32_t hrLastBeatMs = 0;

static NimBLEClient* g_client   = nullptr;
static volatile bool  g_connected = false;
static NimBLEAddress  g_target;
static volatile bool  g_haveTarget = false;
static uint32_t       g_lastTry    = 0;

// dati: [flags][bpm 8 o 16 bit][...]
static void hrNotifyCB(NimBLERemoteCharacteristic*, uint8_t* data, size_t len, bool) {
      if (len < 2 || data == nullptr) return;
      uint16_t bpm;
      if (data[0] & 0x01) {                       // bit0 = valore a 16 bit
            if (len < 3) return;
            bpm = (uint16_t)(data[1] | (data[2] << 8));
      } else {
            bpm = data[1];
      }
      if (bpm > 20 && bpm < 250) {
            heartRate   = (uint8_t)bpm;
            hrLastBeatMs = millis();
            hrValid     = true;
      }
}

class HrScanCB : public NimBLEScanCallbacks {
      void onResult(const NimBLEAdvertisedDevice* dev) override {
            bool hrs = dev->isAdvertisingService(NimBLEUUID((uint16_t)0x180D));
            std::string name = dev->getName();
            if (!hrs && name.find("Magene") == std::string::npos &&
                name.find("HRM") == std::string::npos) return;
            g_target     = dev->getAddress();
            g_haveTarget = true;
            NimBLEDevice::getScan()->stop();
      }
};

static bool tryConnect() {
      if (!g_haveTarget) return false;
      if (!g_client) {
            g_client = NimBLEDevice::createClient();
            if (!g_client) return false;
            // 200..500 ms, latenza 0, timeout 4 s: convivenza tranquilla con WiFi/ESP-NOW
            g_client->setConnectionParams(160, 400, 0, 400);
      }
      if (!g_client->connect(g_target)) return false;
      auto* svc = g_client->getService(NimBLEUUID((uint16_t)0x180D));
      if (!svc) { g_client->disconnect(); return false; }
      auto* ch = svc->getCharacteristic(NimBLEUUID((uint16_t)0x2A37));
      if (!ch || !ch->canNotify() || !ch->subscribe(true, hrNotifyCB)) {
            g_client->disconnect();
            return false;
      }
      g_connected = true;
      return true;
}

static void cardioTask(void*) {
      for (;;) {
            // segnale perso: fascia tolta o battiti fermi da 5 s
            if (hrValid && (millis() - hrLastBeatMs > 5000)) hrValid = false;

            if (g_connected && (!g_client || !g_client->isConnected())) {
                  g_connected = false;
                  hrValid     = false;
            }

            if (!g_connected && (millis() - g_lastTry > 3000)) {
                  g_lastTry = millis();
                  if (g_haveTarget) {
                        tryConnect();                    // bloccante ma in questo task
                  } else {
                        auto* scan = NimBLEDevice::getScan();
                        if (scan && !scan->isScanning()) scan->start(6000, false);
                  }
            }
            vTaskDelay(pdMS_TO_TICKS(250));
      }
}

void cardioBegin() {
      NimBLEDevice::init("TX-CARDIO");
      auto* scan = NimBLEDevice::getScan();
      scan->setScanCallbacks(new HrScanCB(), false);
      scan->setActiveScan(true);
      scan->setInterval(100);
      scan->setWindow(50);            // duty 50%: non ruba tempo al loop del gas
      scan->start(6000, false);       // non bloccante
      xTaskCreate(cardioTask, "Cardio", 4096, nullptr, 1, nullptr);
}
#endif
