// ======================================================
// vescPowerLimit.ino — LETTURA, MODIFICA, SCRITTURA MCCONF
// Usa buffer globale (mcconfBuf) per zero allocazioni stack.
// ======================================================

// COMM_GET_MCCONF (14) è definito in vescConfigDump.ino
#define COMM_ACK         1
#define COMM_NACK        2
#define COMM_SET_MCCONF  13   // ← CORRETTO! (nel vecchio era 15 = COMM_GET_MCCONF_DEFAULT)

// ── Offset mc_configuration (VESC 6.06, struct 483 byte, float big-endian) ──
#define MCCONF_OFS_L_CURRENT_MAX      8
#define MCCONF_OFS_L_CURRENT_MIN      12
#define MCCONF_OFS_L_IN_CURRENT_MAX   16
#define MCCONF_OFS_L_IN_CURRENT_MIN   20
#define MCCONF_OFS_L_ABS_CURRENT_MAX  28
#define MCCONF_OFS_L_MIN_ERPM         32
#define MCCONF_OFS_L_MAX_ERPM         36
#define MCCONF_OFS_L_BATT_CUT_START   54    // uint16 BE, scala x10 (raw = Volt x 10) - fix 11/10/2026
#define MCCONF_OFS_L_BATT_CUT_END     56    // uint16 BE, scala x10 (raw = Volt x 10) - fix 11/10/2026
#define MCCONF_OFS_L_TEMP_FET_START    63    // uint8 (NON float32!) dump 0x43-4
#define MCCONF_OFS_L_TEMP_FET_END      64    // uint8
#define MCCONF_OFS_L_TEMP_MOTOR_START  84
#define MCCONF_OFS_L_MAX_DUTY         100
#define MCCONF_OFS_L_WATT_MAX         73    // FIXED: era 104 (dump 0x4D-4=73, vedi hex-diff 28/06/2026)
#define MCCONF_OFS_M_INVERT_DIR       416

// Buffer globale — 600 byte per config (483) + pacchetto (490)
#define MCCONF_DATA_LEN 483   // mc_configuration reale dal VESC
#define MOTOR_POLE_PAIRS 7     // coppie polari motore: ERPM = rpm meccanici x 7 (4300 rpm -> 30100 erpm)
uint8_t mcconfBuf[600];
int     mcconfLen = 0;

// Variabile cache per versione firmware VESC
static String cachedVescFwVersion = "";
static unsigned long vescFwLastCheck = 0;
#define VESC_FW_CACHE_MS 30000   // ricontrolla dopo 30 secondi



// ======================= UTILITY =======================

void floatToBigEndian(float val, uint8_t *dst) {
  union { float f; uint8_t b[4]; } u;
  u.f = val;
  dst[0] = u.b[3];
  dst[1] = u.b[2];
  dst[2] = u.b[1];
  dst[3] = u.b[0];
}

float bigEndianToFloat(const uint8_t *src) {
  union { float f; uint8_t b[4]; } u;
  u.b[3] = src[0];
  u.b[2] = src[1];
  u.b[1] = src[2];
  u.b[0] = src[3];
  return u.f;
}

// ── uint16 big-endian (battery cut start/end: raw = Volt x 10) ──
void uint16ToBigEndian(uint16_t val, uint8_t *dst) {
  dst[0] = (uint8_t)(val >> 8);
  dst[1] = (uint8_t)(val & 0xFF);
}

uint16_t bigEndianToUint16(const uint8_t *src) {
  return (uint16_t)(((uint16_t)src[0] << 8) | (uint16_t)src[1]);
}

// ======================= LETTURA DA FILE =======================

bool loadMcconfFromFile() {
  McconfGuard configGuard;
  if (!configGuard.ownsLock()) return false;
  File f = LittleFS.open("/mcconf.bin", FILE_READ);
  if (!f) {
    Serial.println("MCCONF: mcconf.bin not found — make a Dump");
    return false;
  }
  mcconfLen = f.read(mcconfBuf, sizeof(mcconfBuf));
  f.close();
  if (mcconfLen < 50) {
    Serial.printf("MCCONF: mcconf.bin too small (%d byte)\n", mcconfLen);
    return false;
  }
  // Il VESC accetta ESATTAMENTE sizeof(mc_configuration) = 483 byte
  // mcconf.bin è salvato come 1024 byte (padding), ma il VESC rifiuta
  // payload più lunghi della config reale. Tagliamo a 483.
  if (mcconfLen > MCCONF_DATA_LEN) {
    mcconfLen = MCCONF_DATA_LEN;
  }
  Serial.printf("MCCONF: caricati %d byte\n", mcconfLen);
  return true;
}

// ======================= SALVATAGGIO SU FILE =======================

bool saveMcconfToFile() {
  McconfGuard configGuard;
  if (!configGuard.ownsLock()) return false;
  File f = LittleFS.open("/mcconf.bin", FILE_WRITE);
  if (!f) {
    Serial.println("MCCONF: save mcconf.bin faild");
    return false;
  }
  f.write(mcconfBuf, mcconfLen);
  f.close();
  Serial.printf("MCCONF: mcconf.bin upgrated (%d byte)\n", mcconfLen);
  invalidateWattMaxCache();   // fix 20/08/2026: file riscritto -> la cache l_watt_max non vale più
  return true;
}

// ======================= VERIFICA VERSIONE FIRMWARE VESC =======================

bool isVescFirmware606() {
  // Usa cache se ancora fresco
  if (millis() - vescFwLastCheck < VESC_FW_CACHE_MS && cachedVescFwVersion != "") {
    return cachedVescFwVersion == "6.06";
  }
  // Altrimenti interroga il VESC
  cachedVescFwVersion = getVescFirmwareVersion();
  vescFwLastCheck = millis();
  if (cachedVescFwVersion == "0.00") {
  //  Serial.println("FW CHECK: VESC non risponde, blocco scritture");
    return false;
  }
  Serial.printf("FW CHECK: VESC firmware = %s\n", cachedVescFwVersion.c_str());
  return cachedVescFwVersion == "6.06";
}

String getCachedVescFwVersion() {
  if (cachedVescFwVersion == "" || millis() - vescFwLastCheck > VESC_FW_CACHE_MS) {
    cachedVescFwVersion = getVescFirmwareVersion();
    vescFwLastCheck = millis();
  }
  return cachedVescFwVersion;
}

// ======================= SCRITTURA AL VESC (COMANDO BINARIO) =======================

bool writeMcconfToVesc() {
  McconfGuard configGuard;
  if (!configGuard.ownsLock()) return false;
  if (mcconfLen <= 0) {
    Serial.println("MCCONF: buffer vuoto");
    return false;
  }
  // Verifica versione firmware VESC prima di scrivere
  if (!isVescFirmware606()) {
    String ver = getCachedVescFwVersion();
    Serial.printf("MCCONF: BLOCCATO — versione VESC %s, serve 6.06\n", ver.c_str());
    return false;
  }

  Serial.printf("MCCONF: write %d byte config al VESC\n", mcconfLen);

  // ── Costruisce il pacchetto in mcconfBuf ──
  // Sposta config di 4 byte in avanti per header + comando
  memmove(&mcconfBuf[4], mcconfBuf, mcconfLen);
  mcconfBuf[3] = COMM_SET_MCCONF; // 0x0D = 13

  int payloadLen = mcconfLen + 1;
  uint16_t crc = crc16_vesc(&mcconfBuf[3], payloadLen);

  // Header (3 byte: long packet + length)
  mcconfBuf[0] = 0x03;
  mcconfBuf[1] = (payloadLen >> 8) & 0xFF;
  mcconfBuf[2] = payloadLen & 0xFF;

  // CRC (2 byte)
  mcconfBuf[3 + payloadLen]     = (crc >> 8) & 0xFF;
  mcconfBuf[4 + payloadLen]     = crc & 0xFF;
  mcconfBuf[5 + payloadLen]     = 0x03; // end byte

  int pktLen = payloadLen + 6;

  // ── Invio ──
  VescUartGuard uartGuard;
  if (!uartGuard.ownsLock()) return false;
  while (Serial2.available()) Serial2.read();

  Serial.printf("MCCONF: invio pkt %d byte (cmd=%d)\n", pktLen, COMM_SET_MCCONF);

  Serial2.write(mcconfBuf, pktLen);
  Serial2.flush();

  // ── Legge risposta (ACK/NACK) ──
  // Il VESC risponde con un pacchetto corto: ACK=0x01 o NACK=0x02
  // Se il VESC applica e riavvia, non risponde niente e si disconnette
  uint8_t rx[32];
  int rxLen = 0;
  unsigned long t0 = millis();

  while (millis() - t0 < 3000) {  // aspetta fino a 3s
    while (Serial2.available() && rxLen < (int)sizeof(rx)) {
      rx[rxLen++] = Serial2.read();
    }
    if (rxLen >= 3 && rx[rxLen - 1] == 0x03) break; // fine pacchetto
    delay(1);
  }

  Serial.printf("MCCONF: ricevuti %d byte di risposta\n", rxLen);
  if (rxLen > 0) {
    // Cerca ACK (0x01) o NACK (0x02) nel payload ricevuto
    bool ack = false, nack = false;
    for (int i = 0; i < rxLen; i++) {
      if (rx[i] == COMM_ACK)  ack = true;
      if (rx[i] == COMM_NACK) nack = true;
    }
    if (ack) {
      Serial.println("MCCONF: ✅ VESC ha ACCETTATO la config");
      return true;
    }
    if (nack) {
      Serial.println("MCCONF: ❌ VESC ha RIFIUTATO (NACK)");
      return false;
    }
    // Risposta ricevuta ma non riconosciuta
    Serial.println("MCCONF: risposta non riconosciuta");
    return false;
  }

  // Nessuna risposta: forse il VESC ha riavviato (comportamento normale)
  Serial.println("MCCONF: nessuna risposta — VESC potrebbe essere riavviato");
  Serial.println("MCCONF: verifico se è ancora vivo...");
  delay(500);

  // Prova a pingare il VESC per vedere se è tornato
  for (int tentativo = 0; tentativo < 8; tentativo++) {
    while (Serial2.available()) Serial2.read();
    // Invia una richiesta valori VESC
    uint8_t pingPkt[6] = {0x02, 0x01, 0x04, 0x00, 0x00, 0x03};
    uint16_t pcrc = crc16_vesc(&pingPkt[2], 1);
    pingPkt[3] = (pcrc >> 8) & 0xFF;
    pingPkt[4] = pcrc & 0xFF;
    // COMM_GET_VALUES = 4
    Serial2.write(pingPkt, 6);
    Serial2.flush();

    delay(200);
    if (Serial2.available() > 5) {
      Serial.printf("MCCONF: VESC è tornato vivo (tentativo %d)\n", tentativo + 1);
      while (Serial2.available()) Serial2.read();
      return true;
    }
  }

  Serial.println("MCCONF: VESC non risponde dopo 3 secondi — controllo manuale");
  return false;
}

// ======================= MODIFICA CORRENTI =======================

bool setCurrentLimits(float motorA, float batteryA) {
  McconfGuard configGuard;
  if (!configGuard.ownsLock()) return false;
  if (!loadMcconfFromFile()) return false;

  floatToBigEndian(motorA,   &mcconfBuf[MCCONF_OFS_L_CURRENT_MAX]);
  floatToBigEndian(batteryA, &mcconfBuf[MCCONF_OFS_L_IN_CURRENT_MAX]);

  Serial.printf("setCurrentLimits: motor=%.1fA battery=%.1fA\n", motorA, batteryA);

  saveMcconfToFile();   // mantiene mcconf.bin allineato al VESC
  return writeMcconfToVesc();
}

// ======================= ENDPOINT VESCPARAMS (query string dalla UI) =======================

bool setVescParamsFromQuery(const String &qs, TickType_t lockTimeout) {
  McconfGuard configGuard(lockTimeout);
  if (!configGuard.ownsLock()) return false;
  if (!loadMcconfFromFile()) return false;

  int start = 0;
  while (start < (int)qs.length()) {
    int amp = qs.indexOf('&', start);
    if (amp < 0) amp = qs.length();

    String pair = qs.substring(start, amp);
    int eq = pair.indexOf('=');
    if (eq > 0) {
      String name = pair.substring(0, eq);
      float fval  = pair.substring(eq + 1).toFloat();

      if (name == "l_current_max")      { floatToBigEndian(fval, &mcconfBuf[MCCONF_OFS_L_CURRENT_MAX]); }
      else if (name == "l_current_min")        { floatToBigEndian(fval, &mcconfBuf[MCCONF_OFS_L_CURRENT_MIN]); }
      else if (name == "l_in_current_max")     { floatToBigEndian(fval, &mcconfBuf[MCCONF_OFS_L_IN_CURRENT_MAX]); }
      else if (name == "l_in_current_min")     { floatToBigEndian(fval, &mcconfBuf[MCCONF_OFS_L_IN_CURRENT_MIN]); }
      else if (name == "l_abs_current_max")    { floatToBigEndian(fval, &mcconfBuf[MCCONF_OFS_L_ABS_CURRENT_MAX]); }
      else if (name == "l_min_erpm")           { floatToBigEndian(fval, &mcconfBuf[MCCONF_OFS_L_MIN_ERPM]); }
      else if (name == "l_max_erpm")           { floatToBigEndian(fval, &mcconfBuf[MCCONF_OFS_L_MAX_ERPM]); }
      else if (name == "l_battery_cut_start")  { uint16ToBigEndian((uint16_t)round(constrain(fval, 0.0f, 6553.0f) * 10.0f), &mcconfBuf[MCCONF_OFS_L_BATT_CUT_START]); }
      else if (name == "l_battery_cut_end")    { uint16ToBigEndian((uint16_t)round(constrain(fval, 0.0f, 6553.0f) * 10.0f), &mcconfBuf[MCCONF_OFS_L_BATT_CUT_END]); }
      else if (name == "l_temp_fet_start")     { uint8_t tf = (uint8_t)round(constrain(fval, 0, 255)); mcconfBuf[MCCONF_OFS_L_TEMP_FET_START] = tf; mcconfBuf[MCCONF_OFS_L_TEMP_FET_END] = tf; }
      else if (name == "l_temp_fet_end")       { mcconfBuf[MCCONF_OFS_L_TEMP_FET_END]   = (uint8_t)round(constrain(fval, 0, 255)); }
      else if (name == "l_temp_motor_start")   { floatToBigEndian(fval, &mcconfBuf[MCCONF_OFS_L_TEMP_MOTOR_START]); }
      else if (name == "l_max_duty")           { floatToBigEndian(fval, &mcconfBuf[MCCONF_OFS_L_MAX_DUTY]); }
      else if (name == "l_watt_max")           { floatToBigEndian(fval, &mcconfBuf[MCCONF_OFS_L_WATT_MAX]); }
      else if (name == "m_invert_direction")   { mcconfBuf[MCCONF_OFS_M_INVERT_DIR] = (fval > 0) ? 1 : 0; }
    }
    start = amp + 1;
  }

  saveMcconfToFile();   // mantiene mcconf.bin allineato al VESC
  return writeMcconfToVesc();
}

// ======================= LETTURA WATT_MAX (per slider potenza) =======================
// Fix 20/08/2026: cache per non rileggere mcconf.bin a ogni secondo.
// getVescData() chiama readVescWattMax() ogni 1s: prima stampava
// "MCCONF: caricati 483 byte" in continuo + lettura LittleFS a ogni ciclo.
// La cache viene invalidata quando mcconf.bin viene riscritto (saveMcconfToFile
// o dump): al massimo una lettura per modifica.

static float cachedWattMax = -1.0f;

float readVescWattMax() {
  if (cachedWattMax >= 0.0f) return cachedWattMax;   // cache valida -> zero I/O
  // Questa funzione è chiamata dalla telemetria nel loop: non bloccare il
  // failsafe mentre un endpoint web aggiorna mcconf.bin.
  McconfGuard configGuard(0);
  if (!configGuard.ownsLock()) return 0.0f;
  if (!loadMcconfFromFile()) { cachedWattMax = 0.0f; return 0.0f; }  // fallback: cache anche il "non trovato"
  cachedWattMax = bigEndianToFloat(&mcconfBuf[MCCONF_OFS_L_WATT_MAX]);
  return cachedWattMax;
}

// Carica una volta mcconf.bin all'avvio e ricava tutti i parametri necessari,
// evitando tre aperture/letture LittleFS consecutive.
bool readVescStartupConfig(float &wattMax, float &maxErpm, uint8_t &invertDir) {
  McconfGuard configGuard;
  if (!configGuard.ownsLock()) return false;
  if (!loadMcconfFromFile()) {
    cachedWattMax = 0.0f;
    wattMax = maxErpm = 0.0f;
    invertDir = 0;
    return false;
  }
  wattMax = bigEndianToFloat(&mcconfBuf[MCCONF_OFS_L_WATT_MAX]);
  maxErpm = bigEndianToFloat(&mcconfBuf[MCCONF_OFS_L_MAX_ERPM]);
  invertDir = mcconfBuf[MCCONF_OFS_M_INVERT_DIR];
  cachedWattMax = wattMax;
  return true;
}

void invalidateWattMaxCache() { cachedWattMax = -1.0f; }
// ======================= LETTURA ERPM MAX / SENSO (per la gestione elica) =======================

float readVescMaxErpm() {
  McconfGuard configGuard;
  if (!configGuard.ownsLock()) return 0.0f;
  if (!loadMcconfFromFile()) return 0.0f;
  return bigEndianToFloat(&mcconfBuf[MCCONF_OFS_L_MAX_ERPM]);
}

uint8_t readVescInvertDir() {
  McconfGuard configGuard;
  if (!configGuard.ownsLock()) return 0;
  if (!loadMcconfFromFile()) return 0;
  return mcconfBuf[MCCONF_OFS_M_INVERT_DIR];
}

// ======================= LETTURA PARAMETRI (per /vescParam GET) =======================

String readVescParamsAsText() {
  McconfGuard configGuard;
  if (!configGuard.ownsLock()) return "ERRORE: configurazione occupata\n";
  if (!loadMcconfFromFile()) {
    return "ERRORE: mcconf.bin non trovato -> fai prima un Dump\n";
  }

  String out; char buf[64];

  #define PUSH(n,o,fmt) do { \
    snprintf(buf,sizeof(buf), n ":" fmt "\n", bigEndianToFloat(&mcconfBuf[o])); out += buf; } while(0)
  #define PUSHB(n,o) do { \
    snprintf(buf,sizeof(buf), n ":%d\n", (int)mcconfBuf[o]); out += buf; } while(0)

  PUSH("l_current_max",     MCCONF_OFS_L_CURRENT_MAX,     "%.1f");
  PUSH("l_current_min",     MCCONF_OFS_L_CURRENT_MIN,     "%.1f");
  PUSH("l_in_current_max",  MCCONF_OFS_L_IN_CURRENT_MAX,  "%.1f");
  PUSH("l_in_current_min",  MCCONF_OFS_L_IN_CURRENT_MIN,  "%.1f");
  PUSH("l_abs_current_max", MCCONF_OFS_L_ABS_CURRENT_MAX, "%.1f");
  PUSH("l_min_erpm",        MCCONF_OFS_L_MIN_ERPM,        "%.0f");
  PUSH("l_max_erpm",        MCCONF_OFS_L_MAX_ERPM,        "%.0f");
  snprintf(buf,sizeof(buf), "l_battery_cut_start:%.1f\n", bigEndianToUint16(&mcconfBuf[MCCONF_OFS_L_BATT_CUT_START]) / 10.0f); out += buf;
  snprintf(buf,sizeof(buf), "l_battery_cut_end:%.1f\n", bigEndianToUint16(&mcconfBuf[MCCONF_OFS_L_BATT_CUT_END]) / 10.0f); out += buf;
  PUSHB("l_temp_fet_start",    MCCONF_OFS_L_TEMP_FET_START);
  PUSHB("l_temp_fet_end",      MCCONF_OFS_L_TEMP_FET_END);
  PUSH("l_temp_motor_start",  MCCONF_OFS_L_TEMP_MOTOR_START, "%.1f");
  PUSH("l_max_duty",          MCCONF_OFS_L_MAX_DUTY,       "%.1f");
  PUSH("l_watt_max",          MCCONF_OFS_L_WATT_MAX,       "%.0f");
  PUSHB("m_invert_direction", MCCONF_OFS_M_INVERT_DIR);

  return out;
}

// ======================= FLASH (leggi da file + invia al VESC) =======================

bool flashMcconfToVesc() {
  McconfGuard configGuard;
  if (!configGuard.ownsLock()) return false;
  if (!loadMcconfFromFile()) return false;
  return writeMcconfToVesc();
}
