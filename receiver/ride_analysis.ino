// ============================================================
//  ride_analysis.ino  –  Algoritmo migliorato v2
//  Segnali usati: throttle, watts, rpm, rms, peak, tilt, pitch, roll
// ============================================================

// ---- Soglie takeoff ----
const int   TAKEOFF_WATTS_THR   = 1800;   // W minimi per contare come spinta
const int   TAKEOFF_MIN_SAMPLES = 3;       // campioni consecutivi sopra soglia

// ---- Soglie foil / surf ----
const float FOIL_RMS_MIN        = 0.18f;  // g  – vibrazione minima su foil
const float FOIL_PEAK_MIN       = 0.30f;  // g
const float FOIL_TILT_MAX       = 25.0f;  // °  – tavola ragionevolmente dritta
const float FOIL_ROLL_MAX       = 20.0f;  // °  – roll contenuto
const float FOIL_PITCH_MAX      = 20.0f;  // °  – pitch contenuto

// ---- Rilevamento caduta (fall) ----
// Criteri: tilt alto OPPURE roll/pitch grandi e rms bassa (boardflat in acqua)
const float FALL_TILT_THR       = 28.0f;  // ° – inclinazione da caduta
const float FALL_ROLL_THR       = 25.0f;  // °
const float FALL_PITCH_THR      = 25.0f;  // °
const float FALL_RMS_LOW        = 0.12f;  // g – quasi fermo = immerso
const int   FALL_CONFIRM_SAMP   = 2;      // campioni consecutivi per confermare

// ---- Durata onda ----
const int   SURF_MIN_SECONDS    = 6;      // scarta fasi brevissime
const int   SURF_MAX_SECONDS    = 180;    // limite superiore (anomalia dati)

// ============================================================
//  Helper inline
// ============================================================

// Ritorna true se la postura è da foil (tavola dritta, movimento presente)
static inline bool isFoilRiding(float rms, float peak,
                                 float tilt, float pitch, float roll)
{
    if (!accelerometerConnected) return true;   // fallback senza ADXL

    bool motionOk  = (rms  >= FOIL_RMS_MIN  || peak  >= FOIL_PEAK_MIN);
    bool postureOk = (fabsf(tilt)  <= FOIL_TILT_MAX  &&
                      fabsf(pitch) <= FOIL_PITCH_MAX  &&
                      fabsf(roll)  <= FOIL_ROLL_MAX);
    return motionOk && postureOk;
}

// Ritorna true se questo campione indica una caduta
static inline bool isFallSample(float rms, float tilt,
                                 float pitch, float roll)
{
    if (!accelerometerConnected) return false;

    bool tiltCrash  = fabsf(tilt)  > FALL_TILT_THR;
    bool rollCrash  = fabsf(roll)  > FALL_ROLL_THR;
    bool pitchCrash = fabsf(pitch) > FALL_PITCH_THR;
    bool stillInWater = (rms < FALL_RMS_LOW);

    // Caduta = grande inclinazione  OPPURE  sbilanciato + fermo
    return tiltCrash || rollCrash || pitchCrash || stillInWater;
}

// ============================================================
//  rideAnalysis  –  versione con accelerometro completo
// ============================================================
void rideAnalysis()
{
    takeOff = wave = waveBestTime = 0;

    uint  count               = 0;
    uint  consecutiveHighPow  = 0;
    uint  waveTime            = 0;
    uint  fallSamples         = 0;

    bool  inTakeOff    = false;
    bool  inWave       = false;
    bool  afterTakeOff = false;
    bool  inFall       = false;

    String startTakeOffTime, startWaveTime;

    char rideFileName[32];
    snprintf(rideFileName, sizeof(rideFileName), "/vescDat%u.txt", fileName);

    File file = LittleFS.open(rideFileName, "r");
    if (!file) {
        Serial.printf("Errore apertura %s\n", rideFileName);
        return;
    }

    while (file.available())
    {
        String line = file.readStringUntil('\n');
        if (line.length() < 10) continue;

        // ---- Parsing riga ----
        struct {
            char  time[6];
            int   rpm;
            float voltage;
            int   current;
            int   watts;
            int   throttle;
            int   battery;
            int   wh;
            float temperature;
            int   powerAvg;
            float rms;
            float peak;
            float tilt;
            float pitch;
            float roll;
        } row;

        int parsed = sscanf(line.c_str(),
            "%5[^,],%d,%f,%d,%d,%d,%d,%d,%f,%d,%f,%f,%f,%f,%f",
            row.time, &row.rpm, &row.voltage, &row.current, &row.watts,
            &row.throttle, &row.battery, &row.wh, &row.temperature,
            &row.powerAvg, &row.rms, &row.peak,
            &row.tilt, &row.pitch, &row.roll);

        if (parsed < 12) {
            // Se mancano tilt/pitch/roll usa valori neutri
            if (parsed < 12) continue;
        }
        // Se parsed==12 (vecchio formato senza IMU) usa zero per angoli
        if (parsed == 12) {
            row.tilt = row.pitch = row.roll = 0.0f;
        }

        // ================================================================
        //  1. RILEVAMENTO CADUTA
        //     Deve essere valutato PRIMA della logica onda per poter
        //     interrompere correttamente la fase di surf.
        // ================================================================
        bool fallNow = isFallSample(row.rms, row.tilt, row.pitch, row.roll);

        if (fallNow) {
            fallSamples++;
            if (fallSamples >= FALL_CONFIRM_SAMP) inFall = true;
        } else {
            fallSamples = 0;
            inFall      = false;
        }

        // ================================================================
        //  2. RILEVAMENTO TAKEOFF
        //     Alta potenza + throttle > minimo per ≥ N campioni consecutivi
        // ================================================================
        bool highPower = (row.watts > TAKEOFF_WATTS_THR &&
                          row.throttle > 10);

        if (highPower) {
            if (!inTakeOff) {
                startTakeOffTime = row.time;
            }
            consecutiveHighPow++;
            inTakeOff    = true;
            afterTakeOff = true;   // abilita il conteggio onde
            // Se eravamo in un'onda, la interrompiamo (nuovo takeoff)
            if (inWave) {
                if (!inFall && waveTime >= (uint)SURF_MIN_SECONDS &&
                               waveTime <= (uint)SURF_MAX_SECONDS) {
                    wave++;
                    if (waveTime > waveBestTime) {
                        waveBestTime = waveTime;
                        riderAlert   = true;
                    }
                }
                inWave   = false;
                waveTime = 0;
            }
        } else {
            if (inTakeOff && consecutiveHighPow >= (uint)TAKEOFF_MIN_SAMPLES) {
                takeOff++;
            }
            consecutiveHighPow = 0;
            inTakeOff          = false;
        }

        // ================================================================
        //  3. RILEVAMENTO FASE FOIL / SURF (motore spento dopo un takeoff)
        //
        //  Condizioni per stare "in onda":
        //    a) motore spento (throttle basso e watts~0)
        //    b) siamo dopo almeno un takeoff
        //    c) postura da foil (tilt/roll/pitch contenuti)
        //    d) movimento rilevato da rms/peak
        //    e) NON in caduta confermata
        // ================================================================
        bool motorOff    = (row.throttle <= 5 && row.watts < 50);
        bool foilPosture = isFoilRiding(row.rms, row.peak,
                                         row.tilt, row.pitch, row.roll);
        bool surfing     = afterTakeOff && motorOff && foilPosture && !inFall;

        if (surfing) {
            if (!inWave) {
                startWaveTime = row.time;
            }
            inWave = true;
            waveTime++;
        } else {
            // Fine fase surf: valida solo se non caduta e durata OK
            if (inWave) {
                if (!inFall &&
                    waveTime >= (uint)SURF_MIN_SECONDS &&
                    waveTime <= (uint)SURF_MAX_SECONDS)
                {
                    wave++;
                    if (waveTime > waveBestTime) {
                        waveBestTime = waveTime;
                        riderAlert   = true;
                    }
                }
                inWave   = false;
                waveTime = 0;
            }
        }

        count++;
    }

    // Chiudi eventuale onda rimasta aperta alla fine del file
    if (inWave && !inFall &&
        waveTime >= (uint)SURF_MIN_SECONDS &&
        waveTime <= (uint)SURF_MAX_SECONDS)
    {
        wave++;
        if (waveTime > waveBestTime) {
            waveBestTime = waveTime;
            riderAlert   = true;
        }
    }

    file.close();

    if (count > 0) {
        Serial.printf(
            "File %u | Takeoffs: %u | Sessioni foil: %u | Best: %u s | Righe: %u\n",
            fileName, takeOff, wave, waveBestTime, count);
    } else {
        Serial.printf("File %u – vuoto o non valido\n", fileName);
    }
}


// ============================================================
//  rideAnalysisNoADXL  –  fallback senza accelerometro (invariato)
// ============================================================
void rideAnalysisNoADXL()
{
    const int takeOffThreshold  = 1800;
    const int minTakeOffSamples = 3;

    takeOff = wave = waveBestTime = 0;
    uint count = 0, consecutiveHighPower = 0, waveTime = 0;
    bool inTakeOff = false, inWave = false, afterTakeOff = false;

    char rideFileName[32];
    snprintf(rideFileName, sizeof(rideFileName), "/vescDat%u.txt", fileName);

    File file = LittleFS.open(rideFileName, "r");
    if (!file) return;

    while (file.available()) {
        String line = file.readStringUntil('\n');
        if (line.length() < 5) continue;

        struct {
            char  time[6];
            int   rpm;
            float voltage;
            int   current;
            int   watts;
            int   throttle;
            int   battery;
            int   wh;
            float temperature;
            int   powerAvg;
        } row;

        int parsed = sscanf(line.c_str(),
            "%5[^,],%d,%f,%d,%d,%d,%d,%d,%f,%d",
            row.time, &row.rpm, &row.voltage, &row.current, &row.watts,
            &row.throttle, &row.battery, &row.wh, &row.temperature, &row.powerAvg);
        if (parsed != 10) continue;

        if (row.watts > takeOffThreshold && row.throttle > 10) {
            if (!inTakeOff) afterTakeOff = true;
            consecutiveHighPower++;
            inTakeOff = true;
        } else {
            if (inTakeOff && consecutiveHighPower >= (uint)minTakeOffSamples)
                takeOff++;
            consecutiveHighPower = 0;
            inTakeOff = false;
        }

        if (row.throttle == 0 && afterTakeOff) {
            if (!inWave) {}
            inWave = true;
            waveTime++;
        } else {
            if (inWave && waveTime >= (uint)SURF_MIN_SECONDS &&
                          waveTime <= (uint)SURF_MAX_SECONDS) {
                wave++;
                if (waveTime > waveBestTime) waveBestTime = waveTime;
            }
            waveTime = 0;
            inWave   = false;
        }
        count++;
    }
    file.close();

    if (count > 0)
        Serial.printf("File %u | Takeoffs: %u | Sessioni: %u | Best: %u s\n",
                      fileName, takeOff, wave, waveBestTime);
    else
        Serial.printf("File %u – vuoto\n", fileName);
}
