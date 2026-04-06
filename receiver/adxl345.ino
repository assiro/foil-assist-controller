///// ACCELEROMETER ADXL345 /////

#define SAMPLE_RATE_HZ 12
#define WINDOW_SIZE 10
#define G 9.80665
// buffer
float dynMag[WINDOW_SIZE];
int sampleIndex = 0;
float peak;
float rms;
float roll;
float pitch;
float tilt;

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
            Serial.println("❌ADXL345 not found");
        }else{
              Serial.println("✅ADXL345 ready");
              accelerometerConnected = true;
        }
        accel.setRange(ADXL345_RANGE_4_G);     // molto più sensibile
        accel.setDataRate(ADXL345_DATARATE_12_5_HZ);
        accel.writeRegister(ADXL345_REG_FIFO_CTL, 0x9F);
}


void getTelemetry() {
        unsigned long now = millis();
        if(accelerometerConnected){
                if (now - lastSample >= sampleInterval) {
                        lastSample = now;
                        acquireAdxlSample();
                }
                if (sampleIndex >= WINDOW_SIZE) {                        
                        analyzeAdxlFifo();
//Serial.print("pitch:"); Serial.print(pitch); Serial.print(" - roll:"); Serial.print(roll); Serial.print(" - tilt:"); Serial.println(tilt);
                        getVescData();
                        sampleIndex = 0;

                                ///// TEST FILE IMU
                        if ((inactivityCounter > 100) and (gravityAlarm) and (accelerometerConnected) and (imuFile)){closeImuFile();}
                        if(recordEnable){
                                if(imuFile){
                                        fileCounter++;
                                        if(fileCounter == 5){
                                                imuFile.flush();
                                                fileCounter = 0;
                                        }
                                }
                        }
                }
        }else{
                checkBoardPositionBySwitch(); 
                if (millis() - lastDataVesc >= 1000) {
                        getVescData();
                        lastDataVesc = now;
                }
                
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
/*
        tilt = atan2(sqrt(x * x + y * y), z) * 180.0 / PI;
        roll  = atan2(y, z) * 180.0 / PI;
        pitch = atan2(-x, sqrt(y*y + z*z)) * 180.0 / PI;
*/
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

        // low-pass → gravity
        gx = alpha * gx + (1.0 - alpha) * x;
        gy = alpha * gy + (1.0 - alpha) * y;
        gz = alpha * gz + (1.0 - alpha) * z;
        float dx = x - gx;
        float dy = y - gy;
        float dz = z - gz;

//        float dynamicMag = sqrt(dx*dx + dy*dy + dz*dz);
//        float dynamicMag = max(max(abs(dx), abs(dy)), abs(dz));

//        dynMag[sampleIndex++] = dynamicMag;           // cambiato valori piccoli
//        dynMag[sampleIndex++] = abs(dx) + abs(dy) + abs(dz);
        dynMag[sampleIndex++] = max(max(abs(dx),abs(dy)),abs(dz));
//Serial.print("x:");Serial.print(dx); Serial.print("   y:");Serial.print(dy);Serial.print("   z:");Serial.println(dz);
//Serial.print("pitch:"); Serial.print(pitch); Serial.print(" - roll:"); Serial.print(roll); Serial.print(" - tilt:"); Serial.println(tilt);
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
 //       tilt  = atan2(sqrt(axMean*axMean + ayMean*ayMean), azMean) * 180.0 / PI;
        float h = sqrt(axMean*axMean + ayMean*ayMean);
        tilt = atan2(h, azMean) * 180.0 / PI;
        roll  = atan2(ayMean, azMean) * 180.0 / PI;
        pitch = atan2(-axMean, sqrt(ayMean*ayMean + azMean*azMean)) * 180.0 / PI;
        
//Serial.print("RMS: "); Serial.print(rms,3); Serial.print("  Peak: "); Serial.print(peak,3); Serial.print("  RollAvg: "); Serial.print(roll,1); Serial.print("  PitchAvg: "); Serial.print(pitch,1); Serial.print("  TiltAvg: "); Serial.println(tilt,1);

        AdxlBoardPosition();

}



void AdxlBoardPosition(){
        if ((checkPositionEnable) and (tilt > thresholdDegrees)) {
                if (!gravityAlarm) {        
                        if (gravityPositionStartTime == 0) {
                                gravityPositionStartTime = millis();
                        } else if (millis() - gravityPositionStartTime >= gravityStartThreshold) {
                                Serial.println("⛔ LOCK");
                                gravityAlarm = true;
                        }
                }
                gravityPositionReleseTime = 0; 
        } else { 
                if (gravityAlarm) {
                        if (gravityPositionReleseTime == 0) {
                                gravityPositionReleseTime = millis();
                        } else if (millis() - gravityPositionReleseTime >= gravityReleaseThreshold) {
                                Serial.println("✅ UNLOCK");
                                gravityAlarm = false;
                        }
                }
                gravityPositionStartTime = 0;
        }
}


bool openImuFile() {
        if(accelerometerConnected){
                        char imuFileName[10] = "/imu";

        //                itoa(fileName, fileNameChar,10);
        //                strcat(imuFileName, fileNameChar);

                        strcat(imuFileName, ".bin");
                        Serial.print(imuFileName);  
                        imuFile = LittleFS.open(imuFileName, FILE_WRITE);
                        if (!imuFile) {
                                Serial.println("❌ No imu.bin");
                                return false;
                        }
                        FileHeader header;
                        header.magic    = 0x464F494C; // 'FOIL'
                        header.version  = 1;
                        header.rate     = 50;
                        header.start_ms = millis();
                        imuFile.write((uint8_t*)&header, sizeof(header));
                        imuFile.flush();
                        Serial.println(" New IMU file created!");
                        return true;
        }
        return false;
}


void closeImuFile() {
        if (imuFile) {
                imuFile.flush();
                imuFile.close();
                Serial.println("✅ IMU file chiuso");
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
        Serial.println("Calibration complete:");
        Serial.print("axOffset: "); Serial.println(axOffset,4);
        Serial.print("ayOffset: "); Serial.println(ayOffset,4);
        Serial.print("azOffset: "); Serial.println(azOffset,4);
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