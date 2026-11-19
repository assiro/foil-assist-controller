///// ACCELEROMETER ADXL345 /////

#define SAMPLE_RATE_HZ 12
#define WINDOW_SIZE 10
#define G 9.80665

float dynMag[WINDOW_SIZE];      // buffer
int sampleIndex = 0;

float rms   = 0.0f;
float peak  = 0.0f;
float tilt  = 0.0f;
float pitch = 0.0f;
float roll  = 0.0f;

float axBuf[WINDOW_SIZE];
float ayBuf[WINDOW_SIZE];
float azBuf[WINDOW_SIZE];
int zeroCrossRate;
float variance;
int bufIndex = 0;

// gravità stimata (low-pass)
float gx = 0, gy = 0, gz = G;

//const float alpha = 0.98;
const float alpha = 0.90;

// timing
unsigned long lastSample = 0;
const unsigned long sampleInterval = 1000 / SAMPLE_RATE_HZ;

volatile bool shockDetected = false;
void IRAM_ATTR adxlISR() {shockDetected = true;}

//#define IMU_FILE "/imu.bin"

struct FileHeader {
        uint32_t magic;      // 'FOIL'
        uint16_t version;    // 1
        uint16_t rate;       // 10 Hz
        uint32_t start_ms;
};

struct AccSample {
        int16_t ax;  // mg
        int16_t ay;
        int16_t az;
};

File imuFile;
uint fileCounter;

// --- Rimappatura assi accelerometro (03/11/2026) ---------------------------
// Ogni byte: (sorgente << 1) | invertito ; sorgente 0=X 1=Y 2=Z del chip.
// Default = montaggio standard nella scatola: Xout=+Y, Yout=+Z, Zout=+X.
// EEPROM: 28 magic 0xA5, 29/30/31 mappa X/Y/Z.
const uint8_t AXIS_MAP_DEFAULT[3] = {2, 4, 0};
uint8_t axisMap[3] = {2, 4, 0};

// pose grezze della procedura guidata (assi del chip, in g)
float axisPose[3][3] = {{0,0,0},{0,0,0},{0,0,0}};
bool  axisPoseDone[3] = {false, false, false};

bool axisMapValid(const uint8_t *m){
      uint8_t used = 0;
      for (int i = 0; i < 3; i++) {
            uint8_t src = (m[i] >> 1) & 0x03;
            if (src > 2) return false;
            used |= (uint8_t)(1 << src);
      }
      return used == 0x07;      // ogni asse del chip usato esattamente una volta
}

void axisMapLoad(){
      if (EEPROM.read(EEPROM_AXIS_MAGIC) == 0xA5) {
            uint8_t m[3];
            m[0] = EEPROM.read(EEPROM_AXIS_MAP);
            m[1] = EEPROM.read(EEPROM_AXIS_MAP + 1);
            m[2] = EEPROM.read(EEPROM_AXIS_MAP + 2);
            if (axisMapValid(m)) { memcpy(axisMap, m, 3); return; }
      }
      memcpy(axisMap, AXIS_MAP_DEFAULT, 3);
}

void axisMapSave(const uint8_t *m){
      if (!axisMapValid(m)) return;
      for (int i = 0; i < 3; i++) EEPROM.write(EEPROM_AXIS_MAP + i, m[i]);
      EEPROM.write(EEPROM_AXIS_MAGIC, 0xA5);
      EEPROM.commit();
      memcpy(axisMap, m, 3);
}

void axisMapReset(){
      for (int i = 0; i < 3; i++) EEPROM.write(EEPROM_AXIS_MAP + i, AXIS_MAP_DEFAULT[i]);
      EEPROM.write(EEPROM_AXIS_MAGIC, 0x00);
      EEPROM.commit();
      memcpy(axisMap, AXIS_MAP_DEFAULT, 3);
}

void remapAccel(float ix, float iy, float iz, float &x, float &y, float &z){
      const float in[3] = {ix, iy, iz};
      float out[3];
      for (int i = 0; i < 3; i++) {
            float v = in[(axisMap[i] >> 1) & 0x03];
            out[i] = (axisMap[i] & 1) ? -v : v;
      }
      x = out[0]; y = out[1]; z = out[2];
}

const char* axisMapName(uint8_t code){
      static const char* names[6] = {"+X","-X","+Y","-Y","+Z","-Z"};
      uint8_t src = (code >> 1) & 0x03;
      if (src > 2) return "?";
      return names[src * 2 + (code & 1)];
}

// n: 1 = livellata, 2 = prua in su, 3 = fianco destro in su.
// La cattura la esegue il LOOP (lo stesso task che legge il sensore): il web
// handler prenota e aspetta. Evita due task sullo stesso bus I2C.
volatile bool axisCapReq = false;
volatile bool axisCapDone = true;
volatile uint8_t axisCapN = 0;
static float axisCapSx = 0, axisCapSy = 0, axisCapSz = 0;
static uint8_t axisCapCount = 0;
static unsigned long axisCapLastMs = 0;

bool axisCapturePose(uint8_t n){
      if (n < 1 || n > 3 || !accelerometerConnected) return false;
      axisCapN = n;
      axisCapCount = 0;
      axisCapSx = axisCapSy = axisCapSz = 0;
      axisCapDone = false;
      axisCapReq = true;
      return true;
}

void axisCaptureService(){
      if (!axisCapReq) return;
      unsigned long now = millis();
      if (now - axisCapLastMs < 5) return;      // ~200 Hz, 100 campioni = 0,5 s
      axisCapLastMs = now;
      sensors_event_t event;
      accel.getEvent(&event);
      axisCapSx += event.acceleration.x / G;
      axisCapSy += event.acceleration.y / G;
      axisCapSz += event.acceleration.z / G;
      if (++axisCapCount >= 100) {
            uint8_t n = axisCapN;
            axisPose[n-1][0] = axisCapSx / 100.0f;
            axisPose[n-1][1] = axisCapSy / 100.0f;
            axisPose[n-1][2] = axisCapSz / 100.0f;
            axisPoseDone[n-1] = true;
            axisCapSx = axisCapSy = axisCapSz = 0;
            axisCapCount = 0;
            axisCapReq = false;
            axisCapDone = true;
      }
}

static int axisArgMax(const float *v){
      int idx = 0;
      for (int i = 1; i < 3; i++) if (fabsf(v[i]) > fabsf(v[idx])) idx = i;
      return idx;
}

// +1 se la matrice e' una rotazione, -1 se e' specchiata
static int8_t axisDetSign(const uint8_t *m){
      int8_t M[3][3] = {{0,0,0},{0,0,0},{0,0,0}};
      for (int i = 0; i < 3; i++) {
            int src = (m[i] >> 1) & 0x03;
            M[i][src] = (m[i] & 1) ? -1 : 1;
      }
      int d = M[0][0]*(M[1][1]*M[2][2]-M[1][2]*M[2][1])
            - M[0][1]*(M[1][0]*M[2][2]-M[1][2]*M[2][0])
            + M[0][2]*(M[1][0]*M[2][1]-M[1][1]*M[2][0]);
      return (d > 0) ? 1 : -1;
}

// Calcola la mappa dalle tre pose; errore = messaggio per la pagina (NULL se ok)
bool axisComputeMap(uint8_t *out, const char* &errore){
      errore = NULL;
      if (!axisPoseDone[0] || !axisPoseDone[1] || !axisPoseDone[2]) {
            errore = "missing pose: capture level, nose up and right side up";
            return false;
      }
      // 1) Z: asse dominante a tavola livellata, component positiva
      const float *pa = axisPose[0];
      if (fabsf(pa[0]) + fabsf(pa[1]) + fabsf(pa[2]) < 0.5f) { errore = "pose 1 empty: accelerometer not reading"; return false; }
      int zi = axisArgMax(pa);
      if (fabsf(pa[zi]) < 0.70f) { errore = "pose 1 not level enough (dominant axis < 0.70 g)"; return false; }
      out[2] = (uint8_t)((zi << 1) | (pa[zi] >= 0 ? 0 : 1));

      // 2) X: tra i due assi liberi quello che si inclina a prua in su.
      //    Segno negativo perche' pitch = atan2(-x, ...) deve crescere con la prua in su
      const float *pb = axisPose[1];
      int xi = -1;
      for (int i = 0; i < 3; i++) {
            if (i == zi) continue;
            if (xi < 0 || fabsf(pb[i]) > fabsf(pb[xi])) xi = i;
      }
      if (fabsf(pb[xi]) < 0.25f) { errore = "pose 2: raise the bow more (~30 deg)"; return false; }
      out[0] = (uint8_t)((xi << 1) | (pb[xi] <= 0 ? 0 : 1));

      // 3) Y: l'asse rimasto, positivo a fianco destro in su (roll = atan2(y, ...))
      const float *pc = axisPose[2];
      int yi = 3 - zi - xi;
      if (fabsf(pc[yi]) < 0.25f) { errore = "pose 3: raise the right side more (~30 deg)"; return false; }
      out[1] = (uint8_t)((yi << 1) | (pc[yi] >= 0 ? 0 : 1));

      // 4) una permutazione specchiata non e' un montaggio fisico valido
      if (axisDetSign(out) < 0) {
            errore = "mirrored axes: check the three poses (module upside down?)";
            return false;
      }
      return true;
}

// offset di zero dalla posa livellata, nel frame della nuova mappa
void axisApplyOffsets(){
      float x, y, z;
      remapAccel(axisPose[0][0], axisPose[0][1], axisPose[0][2], x, y, z);
      axOffset = x;
      ayOffset = y;
      azOffset = z - 1.0f;
      EEPROM.put(16, axOffset);
      EEPROM.put(20, ayOffset);
      EEPROM.put(24, azOffset);
      EEPROM.commit();
}

uint8_t accelAddress = 0;   // 0x53 o 0x1D, 0 = non trovato

void adxl345Setup(){
        accelAddress = 0;
        if (accel.begin(0x53))       accelAddress = 0x53;
        else if (accel.begin(0x1D))  accelAddress = 0x1D;
        if (accelAddress == 0) {
            DBGLN("ADXL345 not found at 0x53 or 0x1D");
        } else {
            accelerometerConnected = true;
            DBGLN("ADXL345 found");
            accel.setRange(ADXL345_RANGE_4_G);
            accel.setDataRate(ADXL345_DATARATE_12_5_HZ);
            accel.writeRegister(ADXL345_REG_FIFO_CTL, 0x9F);
        }
}


// ── in adxl345.ino → getTelemetry() ──
// Timer progressivo per IMU e VESC: mantiene frequenza costante
// anche quando getVescData() impiega più del previsto.
// Catch-up protection: se il ritardo accumulato supera 2 intervalli,
// resetta il timer per evitare burst di recupero.

void getTelemetry() {
    axisCaptureService();
    unsigned long now = millis();
    if (accelerometerConnected) {
        // IMU a ~12Hz con timer progressivo — fase costante
        if (now - lastSample >= sampleInterval) {
            lastSample += sampleInterval;
            if (now - lastSample >= sampleInterval) {  // catch-up
                lastSample = now;
            }
            acquireAdxlSample();
        }
        // Analisi finestra ogni 10 campioni
        if (sampleIndex >= WINDOW_SIZE) {
            analyzeAdxlFifo();
            sampleIndex = 0;
        }
    } else {
        checkBoardPositionBySwitch();
    }
    
    // VESC ogni 1000ms con timer progressivo — chiamata sempre necessaria
    // per inviare dati (anche azzerati) al TX via ESP-NOW
    if (now - lastDataVesc >= 1000) {
        lastDataVesc += 1000;
        if (now - lastDataVesc >= 1000) {   // catch-up
            lastDataVesc = now;
        }
        getVescData();
    }
}


void acquireAdxlSample() {
        sensors_event_t event;
        accel.getEvent(&event);
        // rimappatura assi (default: X=+Y, Y=+Z, Z=+X) + offset di zero
        float x, y, z;
        remapAccel(event.acceleration.x / G, event.acceleration.y / G, event.acceleration.z / G, x, y, z);
        x -= axOffset; y -= ayOffset; z -= azOffset;
        axBuf[bufIndex] = x;
        ayBuf[bufIndex] = y;
        azBuf[bufIndex] = z;
        bufIndex = (bufIndex + 1) % WINDOW_SIZE;

        float sampleTilt = atan2(sqrt(x*x + y*y), z) * 180.0 / PI;

        // DIRECT CUT con DEBOUNCE:
        // scatta solo se raw tilt supera soglia per N campioni consecutivi
        // cosí le vibrazioni e i movimenti brevi non fanno falsi positivi,
        // ma un'inclinazione reale viene rilevata in ~400ms
        if (directCutEnabled && !directCutActive && throttle > 10) {
            static int dcCount = 0;
            if (sampleTilt > directCutDegrees) {
                dcCount++;
                if (dcCount >= 8) {     //cambiato da 5 per falsi positivi 20 09 2026
                    directCutActive = true;
                    directCutStart = millis();
                    esc.write(0);
                    logEvent("ALLARM - STOP PROPELLER");
                }
            } else {
                dcCount = 0;
            }
        }

        // write to imu.bin file
        if(recordEnable){
                if (imuFile) {
                        AccSample s;
                        s.ax = (int16_t)(x * 1000.0f);
                        s.ay = (int16_t)(y * 1000.0f);
                        s.az = (int16_t)(z * 1000.0f);
                        imuFile.write((uint8_t*)&s, sizeof(s));
                }
        }

        // low-pass -> gravity estimate
        gx = alpha * gx + (1.0 - alpha) * x;
        gy = alpha * gy + (1.0 - alpha) * y;
        gz = alpha * gz + (1.0 - alpha) * z;
        float dx = x - gx;
        float dy = y - gy;
        float dz = z - gz;

        dynMag[sampleIndex++] = max(max(abs(dx),abs(dy)),abs(dz));
}


void analyzeAdxlFifo() {
        float sumSq = 0;
        peak = 0;
        float sumX = 0;
        float sumY = 0;
        float sumZ = 0;

        for (int i = 0; i < WINDOW_SIZE; i++) {
                sumSq += dynMag[i] * dynMag[i];
                if (abs(dynMag[i]) > peak)
                peak = abs(dynMag[i]);
                sumX += axBuf[i];
                sumY += ayBuf[i];
                sumZ += azBuf[i];
        }

        rms = sqrt(sumSq / WINDOW_SIZE);
        float axMean = sumX / WINDOW_SIZE;
        float ayMean = sumY / WINDOW_SIZE;
        float azMean = sumZ / WINDOW_SIZE;
        float norm = sqrt(axMean*axMean + ayMean*ayMean + azMean*azMean);

        if (norm > 0.01) {
                axMean /= norm;
                ayMean /= norm;
                azMean /= norm;
        }

        // === ANGOLI MEDIATI ===
        float h = sqrt(axMean*axMean + ayMean*ayMean);
        tilt = atan2(h, azMean) * 180.0 / PI;
        roll  = atan2(ayMean, azMean) * 180.0 / PI;
        pitch = atan2(-axMean, sqrt(ayMean*ayMean + azMean*azMean)) * 180.0 / PI;
//        if (peak > 2.2){logEvent("High peak = CRASH");}        
        AdxlBoardPosition();
}

void AdxlBoardPosition(){
        if ((checkPositionEnable) and (tilt > thresholdDegrees)) {
                if (!gravityAlarm) {        
                        if (gravityPositionStartTime == 0) {
                                gravityPositionStartTime = millis();
                        } else if (millis() - gravityPositionStartTime >= gravityStartThreshold) {
                        //        Serial.println("⛔ LOCK");
                                gravityAlarm = true;
                        //        logEvent("GRAVITY_ALARM_ON");
                        }
                }
                gravityPositionReleseTime = 0; 
        } else { 
                if (gravityAlarm) {
                        if (gravityPositionReleseTime == 0) {
                                gravityPositionReleseTime = millis();
                        } else if (millis() - gravityPositionReleseTime >= gravityReleaseThreshold) {
                        //        Serial.println("✅ UNLOCK");
                                gravityAlarm = false;
                        //        logEvent("GRAVITY_ALARM_OFF");
                        }
                }
                gravityPositionStartTime = 0;
        }
}


bool openImuFile() {
        if(accelerometerConnected){
                        char imuFileName[10] = "/imu";
                        strcat(imuFileName, ".bin");
                        DBGPRINT(imuFileName);  
                        imuFile = LittleFS.open(imuFileName, FILE_WRITE);
                        if (!imuFile) {return false;}
                        FileHeader header;
                        header.magic    = 0x464F494C; // 'FOIL'
                        header.version  = 1;
                        header.rate     = 50;
                        header.start_ms = millis();
                        imuFile.write((uint8_t*)&header, sizeof(header));
                        imuFile.flush();
                        return true;
        }
        return false;
}


void closeImuFile() {
        if (imuFile) {
                imuFile.flush();
                imuFile.close();
        }
}


void calibrateAccelerometer() {
        float sumX = 0;
        float sumY = 0;
        float sumZ = 0;
        const int samples = 200;
        for (int i = 0; i < samples; i++) {
                sensors_event_t event;
                accel.getEvent(&event);
                float ax, ay, az;
                remapAccel(event.acceleration.x / G, event.acceleration.y / G, event.acceleration.z / G, ax, ay, az);
                sumX += ax;
                sumY += ay;
                sumZ += az;
                delay(5);
        }
        axOffset = sumX / samples;
        ayOffset = sumY / samples;
        azOffset = (sumZ / samples) - 1.0;   
        EEPROM.put(16, axOffset);
        EEPROM.put(20, ayOffset);
        EEPROM.put(24, azOffset);
        EEPROM.commit();
}

// ball contact switch to define the position of the board. Old solution replaced from the accelerometer
void checkBoardPositionBySwitch() {
    if ((checkPositionEnable) and (!accelerometerConnected)) {    // to disable the gravity controll eeprom location 0 must be zero
        int switchState1 = digitalRead(GRAVITY1);
        int switchState2 = digitalRead(GRAVITY2);
        if ((switchState1 == LOW) or (switchState2 == LOW)){ 
            roll = 45; pitch = 45; tilt = 45;
            if (!switchPressed) {
                switchPressed = true;
                switchPressStartTime = millis();
            } else {
                if (!gravityAlarm && millis() - switchPressStartTime >= gravityStartThreshold) {
                    gravityAlarm = true;
                    logEvent("GRAVITY_ALARM_ON");
                }
            }
            switchReleaseStartTime = 0; 
        } else { 
            roll = 0; pitch = 0; tilt = 0;
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
