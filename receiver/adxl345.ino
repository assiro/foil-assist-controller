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

void adxl345Setup(){
        if (!accel.begin()) {
            Serial.println("❌ADXL345");
        }else{
              Serial.println("✅ADXL345");
              accelerometerConnected = true;
        }
        accel.setRange(ADXL345_RANGE_4_G);     // molto più sensibile
        accel.setDataRate(ADXL345_DATARATE_12_5_HZ);
        accel.writeRegister(ADXL345_REG_FIFO_CTL, 0x9F);
}


// ── in adxl345.ino → getTelemetry() ──
// Timer progressivo per IMU e VESC: mantiene frequenza costante
// anche quando getVescData() impiega più del previsto.
// Catch-up protection: se il ritardo accumulato supera 2 intervalli,
// resetta il timer per evitare burst di recupero.

void getTelemetry() {
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

        axBuf[bufIndex] = event.acceleration.y / G - axOffset; // in g
        ayBuf[bufIndex] = event.acceleration.z / G - ayOffset;
        azBuf[bufIndex] = event.acceleration.x / G - azOffset;
        bufIndex = (bufIndex + 1) % WINDOW_SIZE; //bufIndex++;
        float x = event.acceleration.y / G - axOffset;
        float y = event.acceleration.z / G - ayOffset;
        float z = event.acceleration.x / G - azOffset;

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
                        s.ax = (int16_t)(event.acceleration.y * 1000.0 / G - axOffset);
                        s.ay = (int16_t)(event.acceleration.z * 1000.0 / G - ayOffset);
                        s.az = (int16_t)(event.acceleration.x * 1000.0 / G - azOffset);
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
                        Serial.print(imuFileName);  
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
                float ax = event.acceleration.y / 9.80665;
                float ay = event.acceleration.z / 9.80665;
                float az = event.acceleration.x / 9.80665;
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
