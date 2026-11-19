// Get vesc data if connected and send call back by espnow

// Soglia di movimento [g] per il keep-alive del ride: se la tavola si muove
// (pump & glide a trigger fermo) la sessione non deve chiudersi
const float RIDE_KEEPALIVE_RMS = 0.08f;

// -- Fault code VESC: tabella sintetica in codice, SOLO i principali --
// (indice = codice fault; NULL = codice raro/non gestito: logga solo il numero)
static const char* const VESC_FAULTS[] = {
    "None",        // 0
    "OverVolt",    // 1
    "UnderVolt",   // 2
    "DRV",         // 3
    "OverCurr",    // 4
    "TempFET",     // 5
    "TempMotor",   // 6
    NULL,           // 7  Gate Driver Over Voltage (raro)
    "MCU_UnderV",  // 8
    "WatchdogRst", // 9
    NULL,           // 10 Gate Driver Under Voltage (raro)
    NULL,           // 11 Encoder SPI (raro)
    NULL,           // 12 Encoder Sincos (raro)
    NULL,           // 13 Encoder Sincos (raro)
    NULL,           // 14 Flash Corruption (raro)
    NULL,           // 15 Offset Curr Sens 1 (raro)
    NULL,           // 16 Offset Curr Sens 2 (raro)
    NULL,           // 17 Offset Curr Sens 3 (raro)
    "UnbalCurr",   // 18
    "BRK",         // 19
    NULL,           // 20 Resolver (raro)
    NULL,           // 21 Resolver (raro)
    NULL,           // 22 Resolver (raro)
    NULL,           // 23 Flash Corrupt App (raro)
    NULL,           // 24 Flash Corrupt MC (raro)
    NULL,           // 25 Encoder No Magnet (raro)
    NULL,           // 26 Encoder Magnet Forte (raro)
    NULL,           // 27 Phase Filter (raro)
    NULL,           // 28 Encoder Fault (raro)
    NULL            // 29 LV Output Fault (raro)
};

static uint lastLoggedVescFault = 0;

const char* vescFaultName(uint code){
    if(code < (sizeof(VESC_FAULTS)/sizeof(VESC_FAULTS[0]))){
        return VESC_FAULTS[code];
    }
    return NULL;
}

// Percentuale stimata dalla tensione a riposo, sulla stessa scala dell'avvio
// (fix 31/10/2026: usata solo come correzione unidirezionale, mai come valore di uscita)
static int socFromVoltage(float v){
      int V = (int)(v * 10.0f);
      int s = map(constrain(V, socEmptyV, socFullV), socEmptyV, socFullV, 0, 99);
      return constrain(s, 0, 99);
}

void getVescData(){  // get vesc data every second and store to flash
            // Keep-alive accelerometrico: movimento sulla tavola = ride vivo
            // anche senza throttle (pump & glide oltre 2-3 minuti)
            if (accelerometerConnected && rms > RIDE_KEEPALIVE_RMS) {
                inactivityCounter = 0;
            }
            if ((inactivityCounter > timeNoRecord) or (gravityAlarm)){
                if (recordEnable && firstConnection) {
                    rideToJson();
                    // Fix 14/11/2026 (indicazione di Roberto): le statistiche di
                    // sessione NON si azzerano a fine ride. Un ride corrisponde a una
                    // batteria nuova, e la sostituzione e' un riavvio: finche' il main
                    // gira, maxPower/maxCurrent/minVoltage/maxTemp devono restare
                    // quelli accumulati dall'accensione. Azzerarli qui faceva
                    // scrivere nel JSON massimi piu' bassi dei picchi gia' nel .bin.
                }
                recordEnable = false;
            } 
            if ((inactivityCounter > 100) and (gravityAlarm) and (accelerometerConnected) and (imuFile)){closeImuFile();}
            if(firstConnection and recordEnable){ //increment total time
                  totalTime ++;
                  Tminutes = totalTime / 60;
                  Tseconds = totalTime - 60 * Tminutes;
            }
            if(firstConnection and recordEnable and throttle > 20){ //increment time of motor riding
                  rowCounter ++;
                  Mminutes = rowCounter / 60;
                  Mseconds = rowCounter - 60 * Mminutes;
            }
            //data
            receiverVersion = VERSION.toFloat();
            data.receiverVersion = receiverVersion;
            data.Tminutes = Tminutes;
            data.Tseconds = Tseconds;
            data.takeOff = takeOff;
            data.wave = wave;
            data.waveBestTime = waveBestTime;
            // Pump/glide detection: throttle zero + accelerometer motion
            pumping = (accelerometerConnected && throttle < 10 && rms > 0.12f);
            data.pumping = pumping;
            // Board moving: stessa soglia del keep-alive ride (per auto-sleep TX)
            boardMoving = (accelerometerConnected && rms > RIDE_KEEPALIVE_RMS);
            data.boardMoving = boardMoving;
            data.gravityAlarm = gravityAlarm;
            data.riderAlert = riderAlert;
            // La UART e' condivisa con le richieste di configurazione provenienti
            // dal web server asincrono. Se occupata, salta questo campione senza
            // consumare o corrompere la risposta dell'altra transazione.
            float vescRpm = 0, vescVoltage = 0, vescCurrent = 0, vescTemp = 0, vescWh = 0;
            uint faultCode = 0;
            bool gotVescValues = false;
            bool vescSampleAvailable = false;
            {
                VescUartGuard uartGuard(pdMS_TO_TICKS(100));
                if (uartGuard.ownsLock()) {
                    vescSampleAvailable = true;
                    while (Serial2.available()) Serial2.read();
                    if (vescReady && UART.getVescValues()) {
                        vescRpm = UART.data.rpm;
                        vescVoltage = UART.data.inpVoltage;
                        vescCurrent = UART.data.avgInputCurrent;
                        vescTemp = UART.data.tempMosfet;
                        vescWh = UART.data.wattHours;
                        faultCode = UART.data.error;
                        gotVescValues = true;
                    }
                }
            }
            
            if (gotVescValues){
                        rpm = vescRpm / (Poles / 2);                                // UART.data.rpm returns cRPM. Divide by pole pairs.
                        voltage = roundf((vescVoltage + voltageOffset) * 10) / 10;      
                        current = roundf(vescCurrent * 10) / 10;
                        tempMosfet = roundf(vescTemp * 10) / 10;   
                        watthour = vescWh;
                        if(faultCode > 0){
                              vescError = faultCode;
                              if (faultCode != lastLoggedVescFault) {
                                    char faultMsg[64];
                                    const char* fname = vescFaultName(faultCode);
                                    if(fname){
                                          snprintf(faultMsg, sizeof(faultMsg), "VESC FAULT CODE: %u - %s", faultCode, fname);
                                    }else{
                                          snprintf(faultMsg, sizeof(faultMsg), "VESC FAULT CODE: %u", faultCode);
                                    }
                                    logEvent(faultMsg);
                              }
                              lastLoggedVescFault = faultCode;
                        } else {
                              lastLoggedVescFault = 0;
                        }
                        if(current < 0){
                              power = 0;
                        }else{
                              power = (int)roundf(voltage * current);   // arrotonda, non tronca
                        }         
                        if(power > 30){
                              totalWatt = totalWatt + power;
                              powerCounter ++;
                              powerAverage = totalWatt / powerCounter;
      // POWER INDEX calc, HIGH power index is good performance MORE OF 50
                              if (Mminutes < 5 or powerAverage < 400){
                                    powerIndex = 0;
                              }else{
                                    int lowConsuption = 500; int highConsuption = 1500; int highRidingTime = 30; int lowRidingTime = 5;
                                    int powerIndex1 = map(powerAverage, lowConsuption, highConsuption, 100, 0); // funcion of average power
                                    int powerIndex2 = map(Tminutes, lowRidingTime, highRidingTime, 0, 100); // funcion of riding time
                                    powerIndex1 = constrain(powerIndex1, 0, 100);
                                    powerIndex2 = constrain(powerIndex2, 0, 100);
                                    powerIndex = (powerIndex1 + powerIndex2) / 2; //VOTE in decimal calculation of votation for the ride
                              }         
                        } 
      ////////////////////////////////////////////////////////////////////////////////////////////
                        // ---- Percentuale batteria (fix 31/10/2026) ----
                        // 1) energia: si accumulano solo i delta positivi del contatore Wh del VESC,
                        //    cosi un suo reset non fa piu risalire la percentuale
                        {
                              float dWh = watthour - whLastSample;
                              if (whLastSample >= 0.0f && dWh > 0.0f && dWh < 100.0f) consumedWhAcc += dWh;
                              whLastSample = watthour;

                              float remainingWh = (float)batCapacity - consumedWhAcc;
                              if (remainingWh < 0.0f) remainingWh = 0.0f;
                              int den = (batCapacity > 0) ? (int)batCapacity : 1;
                              socRaw = (int)lroundf(remainingWh * (float)percentageOfCapacity / (float)den);
                              socRaw = constrain(socRaw, 0, (int)percentageOfCapacity);

                              // 2) controllo tensione: solo a basso carico sostenuto, e puo SOLO
                              //    abbassare il tetto della sessione (latch): niente ritorni su
                              static float vLow = 0;
                              static unsigned long lowSince = 0;
                              if (power < 150 && current < 5.0f && voltage > 20.0f) {
                                    if (lowSince == 0) lowSince = millis();
                                    vLow = (vLow < 1.0f) ? voltage : (vLow * 0.8f + voltage * 0.2f);
                                    if (millis() - lowSince > 8000UL) {
                                          int socV = socFromVoltage(vLow);
                                          if (socV < socLatch) {
                                                if (socLatch - socV >= 2) {
                                                      char socMsg[64];
                                                      snprintf(socMsg, sizeof(socMsg), "SOC tensione %d%% (era %d%%)", socV, socLatch);
                                                      logEvent(socMsg);
                                                }
                                                socLatch = socV;
                                          }
                                    }
                              } else {
                                    lowSince = 0;
                                    vLow = 0;
                              }

                              // 3) uscita: max 1% al secondo in discesa, mai in salita
                              int socTarget = (socRaw < socLatch) ? socRaw : socLatch;
                              if (socTarget < socOut) socOut -= 1;
                              batpercentage = socOut;
                        }
                        // values peak calc
                        if(voltage < minVoltage){minVoltage = voltage;}
                        if(tempMosfet > maxTemp){maxTemp = tempMosfet;}
                        if(current > maxCurrent){maxCurrent = current;}
                        if(power > maxPower){maxPower = power;}
                        data.voltage = voltage;
                        data.tempMosfet = tempMosfet;
                        data.current = current;
                        data.power = power;
                        data.batt = batpercentage;
                        data.file = fileName;
                        data.Mminutes = Mminutes;
                        data.Mseconds = Mseconds;
                        data.powerCounter = powerCounter; 
                        data.powerAverage = powerAverage;
                        data.powerIndex = powerIndex;
                        data.record = record; 
                        if(recordEnable){
                              appendToFileBin(dataFileName);
                        }
            }else if (vescSampleAvailable){
//                        Serial.println("No VESC data");          
                        // Azzera TUTTE le variabili globali (non solo data.xxx)
                        voltage = 0.0;
                        tempMosfet = 0;
                        current = 0.0;
                        power = 0;
                        // fix 31/10/2026: la percentuale NON si azzera, resta l'ultima valida
                        watthour = 0;
                        rpm = 0;
                        // NOTA fix 10/08/2026: minVoltage/maxTemp/maxCurrent/maxPower NON azzerati qui.
                        // Sono statistiche del ride: azzerarle quando il VESC non rispondeva
                        // faceva salvare maxCurrent:0.0 nel JSON di fine ride.
                        data.voltage = 0.0;
                        data.tempMosfet = 0;
                        data.current = 0.0;
                        data.power = 0;
                        data.batt = socOut;   // fix 31/10/2026: non mandare 0%% al controller se il VESC non risponde
                        data.file = fileName;
                        data.powerAverage = 0;
                        data.powerIndex = 0;
                        data.Mminutes = 0;
                        data.Mseconds = 0;    
            }
            // Trust reale dal VESC (mcconf.bin allineato): rimandato al TX così il controller
            // mostra e salva SOLO il valore esatto della board (fix 20/08/2026)
            float realTrust = readVescWattMax();
            uint16_t requestedTrust;
            portENTER_CRITICAL(&rxStateMux);
            requestedTrust = rxTrustWatts;
            portEXIT_CRITICAL(&rxStateMux);
            data.trustWatts = (realTrust >= 1500 && realTrust <= 2200) ? (uint16_t)realTrust : requestedTrust;
            data.score = score;   // rider score aggiornato a ogni analisi (fix 20/08/2026)
            // --- Allarme batteria (fix 13/10/2026): soglia decisa dalla RX sul pacco rilevato ---
            {
                  float _thOn  = battIs12s ? battAlarm12s : battAlarm10s;
                  float _thOff = battIs12s ? battReset12s : battReset10s;
                  if (voltage > 0.0f && voltage < _thOn)  lowBattFlag = true;
                  else if (voltage > _thOff)              lowBattFlag = false;
                  data.lowBattFlag = lowBattFlag;
            // Config motore attiva, per allineare il menu del controller
            data.motorCfg = motorCfgActive ? MOTORCFG_FOC : MOTORCFG_BLDC;
            }
            esp_now_send(broadcastAddress, (uint8_t *) &data, sizeof(data)); 
            data.rideNumber = 0;
            inactivityCounter++;      
            riderAlert = false; 
}


void offsetCalc(){    // VESC correction voltage - offset calc eeprom content 0-2000(0 to 10V) 2000-4000(-0 to -10V)
          int offset; 
          EEPROM.get(8, offset); 
          if(offset > 4000){offset = 0;}
          if(offset > 1999){
              voltageOffset = (float)-(offset - 2000) / 100; // negative number
          }else{
              voltageOffset = (float)offset / 100; // positive number
          }
 //         Serial.print("VOLTAGE OFFSET: "); Serial.print(voltageOffset); Serial.println("V");
}

void batStatus(){   // staus of battery at start up
      loadBattAlertConfig();      loadThrottleCurveConfig();
      offsetCalc();
      for (int a=0; a<5; a++) {
            bool gotValues = false;
            {
                VescUartGuard uartGuard(pdMS_TO_TICKS(500));
                if (uartGuard.ownsLock()) {
                    while (Serial2.available()) Serial2.read();   // fix 26/10/2026: scarta rumore/byte parziali (boot senza VESC)
                    gotValues = UART.getVescValues();
                    if (gotValues) {
                        voltage = UART.data.inpVoltage + voltageOffset;
                        tempMosfet = UART.data.tempMosfet;
                        whBaseline = UART.data.wattHours;
                    }
                }
            }
            if (gotValues) {
                        DBGPRINT("Battery Voltage: "); DBGPRINT(voltage,1);DBGLN("V");
                        int emptyV, fullV;
                        uint fullCap;
                        const char* label;
                        battIs12s = (voltage > 43);   // fix 13/10/2026: pacco 12s se > 43V allo start
                        if(voltage > 43){
                              emptyV  = batEmptyValue12s;
                              fullV   = batFullValue12s;
                              fullCap = battFullCapacity12s;
                              label   = "12s";
                        }else{
                              emptyV  = batEmptyValue;
                              fullV   = batFullValue;
                              fullCap = battFullCapacity;
                              label   = "10s";
                        }
                        int V = (int)(voltage * 10);
                        int batConst = constrain(V, emptyV, fullV);
                        batpercentage = map(batConst, emptyV, fullV, 0, 100);
                        batpercentage = constrain(batpercentage, 0, 99);
                        percentageOfCapacity = batpercentage;
                        // fix 31/10/2026: stato della percentuale per la sessione
                        socEmptyV = emptyV;
                        socFullV  = fullV;
                        socLatch  = batpercentage;   // puo solo scendere
                        socOut    = batpercentage;
                        socRaw    = batpercentage;
                        consumedWhAcc = 0.0f;
                        whLastSample  = whBaseline;
                        // Wh residui stimati dalla % (fullCap = Wh nominali della config)
                        batCapacity = map(batpercentage, 0, 99, 0, (int)fullCap);
                        batCapacity = constrain(batCapacity, 0, (int)fullCap);
                        DBGPRINT("Battery Full Capacity: "); DBGLN(fullCap);
                        char buf[64];
                        snprintf(buf, sizeof(buf), "Bat %s - %d Wh - %d%%", label, batCapacity, batpercentage);
                        logEvent(buf);
                        if(batpercentage < 60){
                              char bootMsg[96];
                              snprintf(bootMsg, sizeof(bootMsg), "START BATT. LOW %d%% - Restart or bad battery", batpercentage);
                              logEvent(bootMsg);
                        }
                        vescReady = true;
                        delay(600);
                        if(batpercentage > 0){return;}
            }else{
                        DBGLN("NO VESC DATA!");
                        batCapacity = 0;
            }
            delay(500);
      }
}

