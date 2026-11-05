// ============================================================
//  vesc_data_binary.ino
//  Sostituzione di appendToFile() e dataFileCreate()
//  con salvataggio binario packed (22 byte/campione).
//
//  RISPARMIO: ~55% vs CSV testo (22 byte vs ~48 byte/riga)
//  Per 1 ora di ride (3600 campioni):
//    CSV testo:   ~173 KB
//    Binario:     ~79 KB
// ============================================================

// ============================================================
//  Struttura record binario  –  22 byte packed
// ============================================================
struct __attribute__((packed)) VescRecord {
    uint8_t  tMin;        // Tminutes  (0–99)
    uint8_t  tSec;        // Tseconds  (0–59)
    int16_t  rpm;         // RPM reale (diviso per poli)
    int16_t  voltage10;   // Tensione × 10  (es. 372 = 37.2 V)
    int16_t  current10;   // Corrente media × 10 (es. 124 = 12.4 A) — fix 10/08/2026: prima era intero
    uint16_t power;       // Potenza [W]
    uint8_t  throttle;    // Throttle mappato 0–99
    uint8_t  batt;        // Percentuale batteria 0–99
    uint16_t watthour;    // Wh consumati
    int8_t   tempMosfet;  // Temperatura MOSFET [°C]  (max 127°C ok)
    uint16_t powerAvg;    // Potenza media [W]
    uint8_t  rms100;      // RMS accelerometro × 100  (0.00–2.55 g)
    uint8_t  peak100;     // Peak accelerometro × 100
    int8_t   tilt;        // Tilt  [°]  (±127°)
    int8_t   pitch;       // Pitch [°]
    int8_t   roll;        // Roll  [°]
};
// sizeof(VescRecord) == 22 (verificato con static_assert sotto)
static_assert(sizeof(VescRecord) == 22, "VescRecord size mismatch!");


// ============================================================
//  appendToFileBin  –  scrive UN record binario in append
//  Chiamare al posto di appendToFile(dataFileName)
// ============================================================
void appendToFileBin(const char* path) {
    if (!fileReady) return;   // ← guard
    File f = LittleFS.open(path, FILE_APPEND);
    if (!f) return;

    VescRecord rec;
    rec.tMin       = (uint8_t)Tminutes;
    rec.tSec       = (uint8_t)Tseconds;
    rec.rpm        = (int16_t)rpm;
    rec.voltage10  = (int16_t)roundf(voltage * 10.0f);
    rec.current10  = (int16_t)roundf(current * 10.0f);   // fix: salva 1 decimale (×10)
    rec.power      = (uint16_t)constrain((int)power, 0, 65535);
    rec.throttle   = (uint8_t)map(throttle, 0, 180, 0, 99);
    rec.batt       = (uint8_t)batpercentage;
    rec.watthour   = (uint16_t)constrain((int)watthour, 0, 65535);
    rec.tempMosfet = (int8_t)constrain((int)tempMosfet, -128, 127);
    rec.powerAvg   = (uint16_t)constrain((int)powerAverage, 0, 65535);

    // ✅ FIX: se accelerometro non connesso scrivi zero, non garbage
    if (accelerometerConnected) {
        rec.rms100  = (uint8_t)constrain((int)(rms  * 100.0f), 0, 254); // 254 max, 255=flag "no sensor"
        rec.peak100 = (uint8_t)constrain((int)(peak * 100.0f), 0, 254);
        rec.tilt    = (int8_t)constrain((int)tilt,  -127, 127);
        rec.pitch   = (int8_t)constrain((int)pitch, -127, 127);
        rec.roll    = (int8_t)constrain((int)roll,  -127, 127);
    } else {
        rec.rms100  = 0;
        rec.peak100 = 0;
        rec.tilt    = 0;
        rec.pitch   = 0;
        rec.roll    = 0;
    }

    f.write((const uint8_t*)&rec, sizeof(rec));
    f.close();
}


// ============================================================
//  dataFileCreate  –  invariato nella logica, cambia estensione
//  Il file ora si chiama vescDatX.bin
// ============================================================
void dataFileCreate() {
    memset(dataFileName, 0, sizeof(dataFileName));
    const char* path = "/fileNumb.txt";
    File file = LittleFS.open(path);
    if (file.available()) {
        if (file.size() == 0) {
//            Serial.println("File number empty!");
            fileNumber(path, 0);
        } else {
            char inChar = file.read();
            fileName = inChar - '0';
            fileName++;
            if (fileName > 9) {
                fileNumber(path, 0);
                fileName = 0;
            } else {
                fileNumber(path, fileName);
            }
        }
    } else {
//        Serial.println("file number not present, created");
        fileNumber(path, 0);
    }
    file.close();
    itoa(fileName, fileNameChar, 10);   // fix 22/10/2026: /fileNumber deve dare il ride CORRENTE
    snprintf(dataFileName, sizeof(dataFileName), "/vescDat%d.bin", fileName);
    Serial.print(dataFileName);
    writeToFile(dataFileName);             // crea file vuoto
    // fix 22/10/2026: il riepilogo JSON del ciclo precedente con lo stesso indice
    // non deve comparire come riepilogo del ride appena iniziato.
    resetRideJson();   // riscrive il riepilogo a zero (la card deve poter caricare)
}


