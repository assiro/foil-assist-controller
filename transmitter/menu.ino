// EEPROM byte usage:
// 0-3 = valMax; 4-7 = valMin; 8 = TRUST (byte, legacy);
// 10-11 = (LIBERI dal 20/08/2026 — trust NON salvato: fonte di verità = VESC/RX); 12-15 = RXmode;
// 20-23 = display mode
// 9, 16-19, 24-27 = (free) — ex Tx Power / THmode (fix 18/08/2026)

void firstInit(){
        EEPROM.get(20, displayMode); // display view mode
        EEPROM.get(0, valMax);
        EEPROM.get(4, valMin);
        // Fix 20/08/2026: trust NON letto da EEPROM — parte dal default 2200 in RAM
        // e viene adottato dalla board al primo sync (fonte di verità = VESC/RX)
 //       maxPower = map(trustWatts, 1500, 2400, 65, 99);
        
        Serial.print("Max Power: "); Serial.println(trustWatts);
        valMiddle = ((valMax - valMin) / 2) + valMin;
        if(valMax<=valMin or valMax>2800 or valMin>2600 or valMax<1800 or (valMax - valMin) < 100){hallCalibration();}
        hallValue = analogRead(hallPin);
        if (hallValue > valMiddle){ 
                valMax = hallValue - 10;    // define final value of valMax
                EEPROM.put(0, valMax);  // int - so 4 bytes (next address is '4')
                while (hallValue > valMiddle){  // rilascia throttle all'avvio per sicurezza
                        delay(500);
                        hallValue = analogRead(hallPin);
                        display.setPartialWindow(0, 0, 200, 200); // Imposta la finestra da aggiornare
                        display.firstPage();
                        display.setTextSize(3);
                        display.setCursor(10,0);
                        display.println("SETUP MODE");
                        display.setCursor(42,110);
                        display.print("RELEASE");
                        display.setCursor(42,145);
                        display.print("TRIGGER");
                        display.setTextSize(2);
                        display.setCursor(5,35);
                        display.print("Hall Calibration");                  
                        display.setCursor(0,65);
                        display.print(hallValue);
                        display.nextPage();
                        counter++;
                        if(counter > 200){hallCalibration();}
                }
                delay(1000);
                valMin = analogRead(hallPin) + 20;
                display.setCursor(65,65);
                display.print(valMin);
                display.print(" > ");
                display.print(valMax);
                display.nextPage();
                EEPROM.put(4, valMin);
                EEPROM.commit();
                valMiddle = ((valMax - valMin) / 2) + valMin;
                delay(1000);
                menuDesign();
        }
        delay(600);       
        do{
                //display.setFullWindow();
                display.setPartialWindow(0, 0, 200, 200); // Imposta la finestra da aggiornare
                display.firstPage();
                display.setTextSize(4);
                display.setCursor(43,0);
                display.print("BOARD");
                display.setCursor(55,50);
                display.print("CODE");  
                display.setTextSize(6);
                display.setCursor(10,90);
                display.print(boardCode);  
                display.setTextSize(2);
                display.setCursor(0,170);
                if (trustSynced) {   // fix 20/08/2026: mostra il trust SOLO se connesso alla board
                    display.print("Max Trust: ");  
                    display.print(trustWatts);  
                    display.print("W");
                }else{
                    display.print("NO BOARD");
                }
        }while (display.nextPage());
        delay(2500);
}


void menuDesign(){
        int menuItemsNumber = 6;
        const char* menuItems[menuItemsNumber] = {
        "- Pair",
        "- Display",
        "- Trust",          
        "- Propeller",
        "- Firmware",                
        "- Exit"
        };
        int selected = 0; 
        bool exitMenu = false;
        vibration_confirm(200);
        while (!exitMenu) {
                // --- Disegno menu ---
                display.setPartialWindow(0, 0, 200, 200);
                display.firstPage();
                do {
                        display.setTextSize(3);
                        display.setCursor(10, 0);
                        display.println("SETUP MODE");
                        for (int i = 0; i < menuItemsNumber; i++) {
                            int y = 30 + i * 30; // 30,60,90,120,150,180
                            if (i == selected) {
                                    display.fillRect(0, y - 3, 200, 30, GxEPD_BLACK);
                                    display.setTextColor(GxEPD_WHITE);
                            } else {
                                    display.setTextColor(GxEPD_BLACK);
                            }
                            display.setCursor(0, y);
                            display.print(menuItems[i]);
                        }                       
                } while (display.nextPage());
                int triggerState = check_trigger();
                if (triggerState == 1) { // short trigger pressing
                        selected = (selected + 1) % menuItemsNumber;   
                        vibration_confirm(30);
                } 
                else if (triggerState == 2) {// Long press → selection confirm                
                        vibration_confirm(200);
                        switch (selected) {
                                case 0: scanWiFi(); break;
                                case 1: DisplayView(); break;
                                case 2: setTrustPower(); break;                                
                                case 3: setPropeller(); break;
                                case 4: firmwareMenu(); break;
                                case 5: exitMenu = true; break;
                        }
                }
        }
//        fullRefresh();
              delay(1000);
              ESP.restart();
}


void check_trigger_off() {
        while(throttle > 20){
                read_throttle();
                delay(200);
        }
}


int check_trigger() {
        int result = 0;
        while (result == 0) {          
                uint32_t startTime = millis();
                read_throttle();
                if (throttle > 50) {
                while (throttle > 50) { // Mantieni il loop finché è premuto
                        read_throttle();
                        if (millis() - startTime >= 1200) { // Pressione lunga
                        result = 2;  // Long press
                        break;
                        }
                }
                if (result == 0) {result = 1;}
                }
                delay(100);
                timerOff++;
                if(timerOff > 3000){systemOff();}
        }
        return result;
}


void DisplayView(){
        check_trigger_off();
        bool exit = false;
        int timeCounter = 0;
        while(exit == false){
                display.setPartialWindow(0, 0, 200, 200); // Imposta la finestra da aggiornare
                display.firstPage();
                display.setTextSize(3);
                display.setCursor(40,0);
                display.println("DISPLAY");
                display.setCursor(25,35);
                display.println("DATA VIEW");
                display.setCursor(45,90);
                display.println("Select");      
                display.setTextSize(8);      
                if ((displayMode == 1) or (displayMode < 1)) {
                    display.setCursor(80,130);
                    display.println("1");
                }else if(displayMode == 2 ){
                    display.setCursor(80,130);
                    display.print("2"); 
                }else if(displayMode > 2 ){
                    display.setCursor(80,130);
                    display.print("3");     
                }
                display.nextPage();

                int triggerState = check_trigger();
                if (triggerState == 1) {
                        displayMode++;    
                        vibration_confirm(30);
                } 
                else if (triggerState == 2) {// Long press → conferma selezione                   
                        vibration_confirm(200);
                        exit = true;
                }
                if (displayMode > 3){displayMode = 1;}
                delay(100);
        }
        EEPROM.put(20, displayMode);
        EEPROM.commit();
        delay(1500);
}

void setTrustPower(){
        check_trigger_off(); 
        // Fix 18/08/2026: modificabile SOLO con board collegata. Il valore trust
        // viene inviato alla RX via ESP-NOW che lo applica al VESC: senza
        // collegamento controller e VESC si desincronizzerebbero.
        if (millis() - lastRxTime > 3000) {   // nessun pacchetto RX da 3s
                display.setPartialWindow(0, 0, 200, 200); 
                display.firstPage();
                do {
                        display.setTextSize(2);
                        display.setCursor(45,40);    
                        display.print("NO BOARD");  
                        display.setCursor(30,70);
                        display.print("CONNECTION");  
                        display.setCursor(20,120);
                        display.print("TRUST LOCKED!");
                } while (display.nextPage());
                vibration_confirm(400);
                delay(2000);
                return;
        }
        trustMenuActive = true;   // fix 06/09/2026: blocca la sync RX durante la regolazione
        bool exit = false;
        while(exit == false){
                display.setPartialWindow(0, 0, 200, 200); 
                display.firstPage();
                do {
                        display.setTextSize(2);
                        display.setCursor(25,40);    
                        display.print("Set max power");  
                        display.setCursor(35,60);
                        display.print("from 1500W");  
                        display.setCursor(50,80);
                        display.print("to 2200W");  
                        display.setTextSize(3);
                        display.setCursor(50,3);
                        display.print("Trust");           
                        display.setCursor(45,110);
                        display.println("Select");   
                        display.setTextSize(6);
                        display.setCursor(15,150);
                        display.print(trustWatts); 
                        display.print("W"); 
                } while (display.nextPage());
                    int triggerState = check_trigger();
                    if (triggerState == 1) {
                            trustWatts = trustWatts + 100;    
                            vibration_confirm(30);
                    } 
                    else if (triggerState == 2) {                   
                            vibration_confirm(200);
                            exit = true;
                    }
                if (trustWatts > 2200){trustWatts = 1500;}
                delay(100);
        }
//        maxPower = map(trustWatts, 1500, 2400, 65, 99);
        trustMenuActive = false;   // fix 06/09/2026: uscita menu trust
        trustMenuExitTime = millis();   // fix 06/09/2026: 5s di settle, poi riprende la sync RX
        trustSynced = true;   // fix 20/08/2026: il valore scelto dall'utente va trasmesso alla board

        // fix 06/09/2026: il menu gira in setup() PRIMA dei task FreeRTOS, quindi espnowTask
        // NON sta inviando. Trasmette qui il nuovo trust direttamente (piu' tentativi),
        // cosi' la RX lo applica prima dell'ESP.restart() a fine menu.
        for (int _i = 0; _i < 10; _i++) {
            TxPkt _pkt = { 0, (uint16_t)trustWatts };
            esp_now_send(broadcastAddress, (uint8_t *)&_pkt, sizeof(_pkt));
            delay(150);
        }
        // Fix 20/08/2026: NIENTE EEPROM — il trust lo salva la RX in mcconf.bin quando lo applica al VESC
//        Serial.printf("Trust: %dW (%d%%)\n", trustWatts, maxPower);
        delay(1500);
}

// --------------------- PROPELLER (gestione elica) ---------------------
// Valori in RPM MECCANICI: la RX li converte in ERPM (x coppie polari) e li scrive nel VESC.
// Preset: TecnoFly = 4300 rpm CW ; Foil Drive = 4300 rpm CCW ; Custom = 2500..5500 step 100 + senso.
#define PROP_RPM_MIN  2500
#define PROP_RPM_MAX  5500
#define PROP_RPM_STEP 100
#define PROP_RPM_TECNOFLY  4300
#define PROP_RPM_FOILDRIVE 4300

void sendPropellerToBoard() {
        // Il menu gira in setup() PRIMA dei task FreeRTOS: invio diretto (come per il trust).
        for (int i = 0; i < 10; i++) {
                TxPkt p = { 0, trustSynced ? trustWatts : 0, propRpm, propCw ? 1 : 0 };
                esp_now_send(broadcastAddress, (uint8_t *)&p, sizeof(p));
                delay(150);
        }
}

void applyPropeller(uint16_t rpm, bool cw) {
        propRpm = rpm;
        propCw  = cw;
        sendPropellerToBoard();
        vibration_confirm(300);
}

void propellerCustom() {
        uint16_t rpm = propRpm ? propRpm : PROP_RPM_TECNOFLY;
        bool cw = propCw;
        bool exit = false;
        // Passo 1: RPM (short = +100, long = conferma)
        while (!exit) {
                display.setPartialWindow(0, 0, 200, 200);
                display.firstPage();
                do {
                        display.setTextSize(3);
                        display.setCursor(30, 0);
                        display.print("CUSTOM");
                        display.setTextSize(2);
                        display.setCursor(20, 40);
                        display.print("Set RPM");
                        display.setTextSize(6);
                        display.setCursor(35, 90);
                        display.print(rpm);
                        display.setTextSize(2);
                        display.setCursor(0, 170);
                        display.print("+100 / hold=next");
                } while (display.nextPage());
                int t = check_trigger();
                if (t == 1) { rpm = (rpm >= PROP_RPM_MAX) ? PROP_RPM_MIN : rpm + PROP_RPM_STEP; vibration_confirm(30); }
                else if (t == 2) { vibration_confirm(200); exit = true; }
                delay(100);
        }
        // Passo 2: senso di rotazione (short = cambia, long = conferma)
        exit = false;
        while (!exit) {
                display.setPartialWindow(0, 0, 200, 200);
                display.firstPage();
                do {
                        display.setTextSize(3);
                        display.setCursor(30, 0);
                        display.print("CUSTOM");
                        display.setTextSize(2);
                        display.setCursor(20, 40);
                        display.print("Rotation");
                        display.setTextSize(8);
                        display.setCursor(35, 90);
                        display.print(cw ? "CW" : "CCW");
                        display.setTextSize(2);
                        display.setCursor(0, 170);
                        display.print("toggle / hold=ok");
                } while (display.nextPage());
                int t = check_trigger();
                if (t == 1) { cw = !cw; vibration_confirm(30); }
                else if (t == 2) { vibration_confirm(200); exit = true; }
                delay(100);
        }
        applyPropeller(rpm, cw);
        delay(1500);
}

void setPropeller() {
        check_trigger_off();
        // Fix 14/09/2026: come per il Trust, l'elica si regola SOLO con la board collegata.
        // Senza link ESP-NOW i preset non arrivano al VESC e controller/board si desincronizzano.
        if (millis() - lastRxTime > 3000) {   // nessun pacchetto RX da 3s
                display.setPartialWindow(0, 0, 200, 200);
                display.firstPage();
                do {
                        display.setTextSize(2);
                        display.setCursor(45,40);
                        display.print("NO BOARD");
                        display.setCursor(30,70);
                        display.print("CONNECTION");
                        display.setCursor(0,120);
                        display.print("PROPELLER LOCKED!");
                } while (display.nextPage());
                vibration_confirm(400);
                delay(2000);
                return;
        }
        int sel = 0;   // 0 = TecnoFly, 1 = Foil Drive, 2 = Custom
        bool exit = false;
        const char* names[3] = { "TecnoFly", "Foil Drive", "Custom" };
        while (!exit) {
                display.setPartialWindow(0, 0, 200, 200);
                display.firstPage();
                do {
                        display.setTextSize(3);
                        display.setCursor(25, 0);
                        display.print("PROPELLER");
                        for (int i = 0; i < 3; i++) {
                                int y = 45 + i * 40;
                                if (i == sel) { display.fillRect(0, y - 4, 200, 34, GxEPD_BLACK); display.setTextColor(GxEPD_WHITE); }
                                else { display.setTextColor(GxEPD_BLACK); }
                                display.setCursor(10, y);
                                display.print(names[i]);
                        }
                        display.setTextColor(GxEPD_BLACK);
                        display.setTextSize(2);
                        display.setCursor(0, 175);
                        if (propRpm) { display.print(propRpm); display.print("rpm "); display.print(propCw ? "CW" : "CCW"); }
                        else { display.print("not set"); }
                } while (display.nextPage());
                int t = check_trigger();
                if (t == 1) { sel = (sel + 1) % 3; vibration_confirm(30); }
                else if (t == 2) {
                        vibration_confirm(200);
                        if (sel == 2) { propellerCustom(); }
                        else { applyPropeller(sel == 0 ? PROP_RPM_TECNOFLY : PROP_RPM_FOILDRIVE, sel == 0); }
                        exit = true;
                }
                delay(100);
        }
        delay(1000);
}