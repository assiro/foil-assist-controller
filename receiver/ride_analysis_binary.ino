// ============================================================
//  ride_analysis_binary.ino  –  legge file .bin (VescRecord)
//
// ============================================================

// ---- Soglie takeoff ----
// Soglia DINAMICA (fix 18/08/2026): potenza max VESC (l_watt_max) - margine,
// così anche i rider leggeri con max power bassa vengono rilevati al takeoff.
const int   TAKEOFF_WATTS_FALLBACK = 1800;  // se mcconf non disponibile
const int   TAKEOFF_WATTS_MARGIN   = 200;   // W sotto la max VESC
const int   TAKEOFF_WATTS_MIN      = 300;   // floor anti-falsi positivi
const int   TAKEOFF_MIN_SAMPLES    = 3;

// Legge la soglia corrente: max power VESC - margine (min TAKEOFF_WATTS_MIN)
int takeoffWattsThr() {
    float wmax = readVescWattMax();
    if (wmax <= 0) return TAKEOFF_WATTS_FALLBACK;
    int thr = (int)wmax - TAKEOFF_WATTS_MARGIN;
    if (thr < TAKEOFF_WATTS_MIN) thr = TAKEOFF_WATTS_MIN;
    return thr;
}

// ---- Soglie foil / surf ----
const float FOIL_RMS_MIN        = 0.18f;
const float FOIL_PEAK_MIN       = 0.30f;
const float FOIL_TILT_MAX       = 25.0f;
const float FOIL_ROLL_MAX       = 20.0f;
const float FOIL_PITCH_MAX      = 20.0f;

// ---- Rilevamento caduta ----
const float FALL_TILT_THR       = 28.0f;
const float FALL_ROLL_THR       = 25.0f;
const float FALL_PITCH_THR      = 25.0f;
const float FALL_RMS_LOW        = 0.12f;
const int   FALL_CONFIRM_SAMP   = 2;

// ---- Durata onda ----
const int   SURF_MIN_SECONDS    = 8;
// 3600 s = 1 h (era 180 s): i ride pump&glide oltre 3 minuti venivano
// scartati del tutto — niente wave, niente tempo totale
const int   SURF_MAX_SECONDS    = 3600;

// ---- Gap tolerance ----
// Campioni consecutivi "non-foil" (throttle=0, senza caduta) tollerati senza
// chiudere la sessione. Il waveTime continua ad incrementarsi: il rider è
// ancora in volo (glide liscio / pump poco energico) e quei secondi fanno
// parte della sessione a tutti gli effetti.
// 30 campioni (~30 s a 1 Hz): un glide liscio tra pompate dura 10-20 s,
// con 5 campioni la sessione si chiudeva in pieno volo (ride tagliati)
const int   FOIL_GAP_MAX        = 30;

// ---- Rider Score parameters ----
const float K1 = 0.001;
const float K2 = 0.5;
const float K3 = 0.5;
const float K4 = 1;
const float K5 = 20;
int totalWaveTime = 0;

// ============================================================
//  Helper inline  (identici alla versione testo)
// ============================================================
static inline bool isFoilRiding(float rms, float peak,
                                  float tilt, float pitch, float roll)
{
    if (!accelerometerConnected) return true;
    bool motionOk  = (rms  >= FOIL_RMS_MIN  || peak  >= FOIL_PEAK_MIN);
    bool postureOk = (fabsf(tilt)  <= FOIL_TILT_MAX  &&
                      fabsf(pitch) <= FOIL_PITCH_MAX  &&
                      fabsf(roll)  <= FOIL_ROLL_MAX);
    return motionOk && postureOk;
}

static inline bool isFallSample(float rms, float tilt,
                                  float pitch, float roll)
{
    if (!accelerometerConnected) return false;
    // Caduta vera: angoli fuori soglia.
    // rms basso da solo NON è caduta: durante pump quieto o glide il rider
    // può avere rms < FALL_RMS_LOW con tilt/roll/pitch normali (tavola dritta).
    // rms basso conta solo se un angolo è già "sospetto" (> metà soglia).
    bool anglesAnomaly = fabsf(tilt)  > FALL_TILT_THR  ||
                         fabsf(roll)  > FALL_ROLL_THR  ||
                         fabsf(pitch) > FALL_PITCH_THR;
    bool anglesWarning = fabsf(tilt)  > FALL_TILT_THR  / 2.0f ||
                         fabsf(roll)  > FALL_ROLL_THR  / 2.0f ||
                         fabsf(pitch) > FALL_PITCH_THR / 2.0f;
    return anglesAnomaly || (anglesWarning && rms < FALL_RMS_LOW);
}


// ============================================================
//  rideAnalysis  –  file binary version
// ============================================================
void rideAnalysis(){
    takeOff = wave = waveBestTime = totalWaveTime = 0;
    uint  count               = 0;
    uint  consecutiveHighPow  = 0;
    uint  waveTime            = 0;
    uint  fallSamples         = 0;
    uint  gapCount            = 0;   // campioni di gap correnti

    bool  inTakeOff    = false;
    bool  inWave       = false;
    bool  afterTakeOff = false;
    bool  inFall       = false;

    // Apre il file .bin
    char rideFileName[32];
    snprintf(rideFileName, sizeof(rideFileName), "/vescDat%u.bin", fileName);
    File file = LittleFS.open(rideFileName, "r");
    if (!file) {return;}
    // Verifica che la dimensione sia un multiplo di sizeof(VescRecord)
    size_t fileSize = file.size();
    if (fileSize % sizeof(VescRecord) != 0) {
        Serial.printf("Attenzione: dimensione file non multipla di %u!\n",
                      sizeof(VescRecord));
    }

    VescRecord rec;

    // Soglia takeoff dinamica (max power VESC - margine)
    const int takeoffThr = takeoffWattsThr();

    while (file.read((uint8_t*)&rec, sizeof(rec)) == sizeof(rec))
    {
        // ---- Ricostruzione valori float ----
        float voltage     = rec.voltage10 / 10.0f;
        float rms         = rec.rms100    / 100.0f;
        float peak        = rec.peak100   / 100.0f;
        float tilt        = (float)rec.tilt;
        float pitch       = (float)rec.pitch;
        float roll        = (float)rec.roll;
        int   watts       = (int)rec.power;
        int   thr         = (int)rec.throttle;

        // Formato tempo per log (opzionale)
        // char timeStr[6]; snprintf(timeStr, 6, "%02u:%02u", rec.tMin, rec.tSec);

        // ================================================================
        //  1. CADUTA
        // ================================================================
        bool fallNow = isFallSample(rms, tilt, pitch, roll);
        if (fallNow) {
            fallSamples++;
            if (fallSamples >= FALL_CONFIRM_SAMP) inFall = true;
        } else {
            fallSamples = 0;
            inFall      = false;
        }

        // ================================================================
        //  2. TAKEOFF
        // ================================================================
        bool highPower = (watts > takeoffThr && thr > 10);

        if (highPower) {
            consecutiveHighPow++;
            inTakeOff    = true;
            afterTakeOff = true;
            // motore acceso: chiudi la sessione foil se aperta.
            // Non controlliamo inFall qui: se il rider stava surfonando e
            // riaccende il motore, la sessione è valida a prescindere da
            // eventuali spike di caduta rilevati in quegli istanti.
            // Comportamento identico a rideAnalysis.js.
            if (inWave) {
                if (waveTime >= (uint)SURF_MIN_SECONDS) {
                    totalWaveTime += waveTime;   // il tempo conta sempre
                    if (waveTime <= (uint)SURF_MAX_SECONDS) {
                        wave++;
                        if (waveTime > waveBestTime) {
                            waveBestTime = waveTime;
                            riderAlert   = true;
                        }
                    }
                }
                inWave   = false;
                waveTime = 0;
                gapCount = 0;
            }
        } else {
            if (inTakeOff && consecutiveHighPow >= (uint)TAKEOFF_MIN_SAMPLES) {
                takeOff++;
            }
            consecutiveHighPow = 0;
            inTakeOff          = false;
        }

        // ================================================================
        //  3. FASE FOIL / SURF con gap tolerance
        //
        //  Un "gap" è un campione a throttle=0, senza caduta, in cui
        //  rms/peak scende momentaneamente sotto soglia (glide liscio,
        //  pump poco energico). Non chiudiamo la sessione: aspettiamo
        //  fino a FOIL_GAP_MAX campioni consecutivi.
        //  In entrambi i casi (foil valido o gap) waveTime cresce:
        //  il rider è ancora in volo e quei secondi appartengono alla
        //  sessione a tutti gli effetti.
        // ================================================================
        bool motorOff    = (thr <= 5 && watts < 50);
        bool foilPosture = isFoilRiding(rms, peak, tilt, pitch, roll);
        bool surfing     = afterTakeOff && motorOff && foilPosture && !inFall;

        if (surfing) {
            // campione foil valido
            inWave   = true;
            gapCount = 0;   // gap precedente assorbito
            waveTime++;
        } else if (inWave && motorOff && !inFall && gapCount < (uint)FOIL_GAP_MAX) {
            // glide / gap: sessione tenuta aperta, secondo contato
            gapCount++;
            waveTime++;
        } else {
            // chiusura definitiva: motore acceso, caduta, o gap > FOIL_GAP_MAX
            if (inWave) {
                if (waveTime >= (uint)SURF_MIN_SECONDS) {
                    totalWaveTime += waveTime;   // il tempo conta sempre
                    if (waveTime <= (uint)SURF_MAX_SECONDS) {
                        wave++;
                        if (waveTime > waveBestTime) {
                            waveBestTime = waveTime;
                            riderAlert   = true;
                        }
                    }
                }
                inWave   = false;
                waveTime = 0;
                gapCount = 0;
            }
        }

        count++;
        if (count % 100 == 0) yield();
    }

    // Sessione foil rimasta aperta a fine file
    if (inWave && waveTime >= (uint)SURF_MIN_SECONDS)
    {
        totalWaveTime += waveTime;   // il tempo conta sempre
        if (waveTime <= (uint)SURF_MAX_SECONDS) {
            wave++;
            if (waveTime > waveBestTime) {
                waveBestTime = waveTime;
                riderAlert   = true;
            }
        }
    }

    file.close();
    scoreCalc();

    // ---- record pump & glide ----
    if (waveBestTime > (uint)record) {
        record = (int)waveBestTime;
        saveRecord(record);
        riderAlert = true;
    }

    //UCCIO: if (count > 0) {
        //UCCIO: Serial.printf(
            //UCCIO: "File %u | Campioni: %u | Takeoffs: %u | Sessioni foil: %u | Best: %u s\n",
            //UCCIO: fileName, count, takeOff, wave, waveBestTime);
    //UCCIO: } else {
        //UCCIO: Serial.printf("File %u – vuoto o non valido\n", fileName);
    //UCCIO: }
}


// ============================================================
//  rideAnalysisNoADXL  –  invariato, usa .bin
// ============================================================
void rideAnalysisNoADXL()
{
    const int takeOffThreshold  = takeoffWattsThr();   // soglia dinamica
    const int minTakeOffSamples = 3;

    takeOff = wave = waveBestTime = totalWaveTime = 0;
    uint count = 0, consecutiveHighPower = 0, waveTime = 0;
    bool inTakeOff = false, inWave = false, afterTakeOff = false;

    char rideFileName[32];
    snprintf(rideFileName, sizeof(rideFileName), "/vescDat%u.bin", fileName);

    File file = LittleFS.open(rideFileName, "r");
    if (!file) return;

    VescRecord rec;
    while (file.read((uint8_t*)&rec, sizeof(rec)) == sizeof(rec))
    {
        int watts = (int)rec.power;
        int thr   = (int)rec.throttle;

        if (watts > takeOffThreshold && thr > 10) {
            if (!inTakeOff) afterTakeOff = true;
            consecutiveHighPower++;
            inTakeOff = true;
        } else {
            if (inTakeOff && consecutiveHighPower >= (uint)minTakeOffSamples) takeOff++;
            consecutiveHighPower = 0;
            inTakeOff = false;
        }

        if (thr == 0 && afterTakeOff) {
            inWave = true;
            waveTime++;
        } else {
            if (inWave && waveTime >= (uint)SURF_MIN_SECONDS) {
                totalWaveTime += waveTime;   // il tempo conta sempre
                if (waveTime <= (uint)SURF_MAX_SECONDS) {
                    wave++;
                    if (waveTime > waveBestTime) waveBestTime = waveTime;
                }
            }
            waveTime = 0;
            inWave   = false;
        }
        count++;
        if (count % 100 == 0) yield();
    }
    file.close();
    scoreCalc();

/*    
    if (count > 0)
        Serial.printf("File %u | Takeoffs: %u | Sessioni: %u | Best: %u s\n",
                      fileName, takeOff, wave, waveBestTime);
    else
        Serial.printf("File %u – vuoto\n", fileName);
*/
}


void scoreCalc(){
        // Protezione divisioni per zero e overflow
        unsigned long totalTime = (totalWaveTime > 0) ? (unsigned long)totalWaveTime : 1;
        unsigned long waves     = (wave > 0)           ? (unsigned long)wave       : 1;
        int battDenominator     = (100 - batpercentage);
        if (battDenominator < 1) battDenominator = 1;  

        C1 = K1 * powerAverage * totalTime;
        C2 = K2 * waveBestTime + K3 * totalTime / waves;
        C3 = K4 * totalTime / battDenominator;
        C4 = K5 * takeOff;

        float fScore = C1 + C2 + C3 - C4;

        // Clamp a 32-bit signed per sicurezza
        if (fScore > 2000000000.0f) fScore = 2000000000.0f;
        if (fScore < 0.0f) fScore = 0.0f;
        score = (int)fScore;
}