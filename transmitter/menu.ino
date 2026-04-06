// EEPROM byte usage:
// 0-3 = valMax; 4-7 = valMin; 8 = TRUST; 9 = Tx Power; 12-15 = RXmode; 16-19 = THmode; 20-23 = display mode

void firstInit(){
        EEPROM.get(20, displayMode); // display view mode
        EEPROM.get(16, THmode); //read throttle curve
        if(THmode > 10){THmode = 1;}
        EEPROM.get(0, valMax);
        EEPROM.get(4, valMin);
        maxPower = EEPROM.read(8);
        if(maxPower < 60 or maxPower > 99){maxPower = 99;}
        Serial.print("Max Power: "); Serial.println(maxPower);
        
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
                display.print("Trust set to ");  
                display.print(maxPower);  
                display.print("%");
        }while (display.nextPage());
        delay(2500);
}


void menuDesign(){
        int menuItemsNumber = 6;
        const char* menuItems[menuItemsNumber] = {
        "- Pair",
        "- Display",
        "- Throttle",
        "- Trust",          
        "- Firmware",                
//        "- Manual",
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
                                case 2: ThrottleMode();break;
                                case 3: setTrustPower(); break;                                
                                case 4: firmwareUpgrade(); break;

        //                        case 4: showQR(); break;
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

void ThrottleMode(){
        check_trigger_off();
        bool exit = false;
        int timeCounter = 0;
        while(exit == false){
                display.setPartialWindow(0, 0, 200, 200); 
                display.firstPage();
                do {
                        display.setTextSize(3);
                        display.setCursor(30,0);
                        display.println("THROTTLE");
                        display.setCursor(60,30);
                        display.println("CURVE");            
                        display.setCursor(45,70);
                        display.println("Select");
                        if(THmode == 0){    
                        display.setCursor(50,130);
                        display.print("LINEAR"); 
                        } else if(THmode == 1){
                        display.setCursor(50,130);
                        display.print("STRONG");
                        } else if(THmode == 2){    
                        display.setCursor(50,130);
                        display.print("MIDDLE"); 
                        } else if(THmode == 3){    
                        display.setCursor(65,130);
                        display.print("SOFT"); 
                        } else if(THmode == 4){    
                        display.setCursor(45,130);
                        display.print("ANGULAR"); 
                        } else if(THmode == 5){    
                        display.setCursor(70,130);
                        display.print("ECO"); 
                        }
                } while (display.nextPage());


                        int triggerState = check_trigger();
                        if (triggerState == 1) {
                                THmode++;    
                                vibration_confirm(30);
                        } 
                        else if (triggerState == 2) {// Long press → conferma selezione                   
                                vibration_confirm(200);
                                exit = true;
                        }
                if (THmode > 5){THmode = 0;}
                delay(100);
        }
        EEPROM.put(16, THmode);
        EEPROM.commit();
        delay(1500);
        setTxPower();
}


void setTrustPower(){
        check_trigger_off(); 
        bool exit = false;
        int timeCounter = 0;
        while(exit == false){
                display.setPartialWindow(0, 0, 200, 200); 
                display.firstPage();
                do {
                        display.setTextSize(2);
                        display.setCursor(20,40);    
                        display.print("Set max trust");  
                        display.setCursor(10,60);
                        display.print("from 65% to 99%");  
                        display.setTextSize(3);
                        display.setCursor(50,3);
                        display.print("Trust");           
                        display.setCursor(45,100);
                        display.println("Select");   
                        display.setTextSize(6);
                        display.setCursor(55,150);
                        display.print(maxPower); 
                        display.print("%"); 
                } while (display.nextPage());
                    int triggerState = check_trigger();
                    if (triggerState == 1) {
                            maxPower = maxPower + 5;    
                            vibration_confirm(30);
                    } 
                    else if (triggerState == 2) {// Long press → conferma selezione                   
                            vibration_confirm(200);
                            exit = true;
                    }
                if (maxPower == 100){maxPower = 99;}
                if (maxPower > 99){maxPower = 65;}
                delay(100);
        }
        EEPROM.write(8, maxPower);
        EEPROM.commit();
        delay(1500);

}

void setTxPower(){
        check_trigger_off(); 
        bool exit = false;
        int timeCounter = 0;
        while(exit == false){
                display.setPartialWindow(0, 0, 200, 200); 
                display.firstPage();
                do {
                    display.setTextSize(3);
                    display.setCursor(27,3);
                    display.println("TX Power");  
                    display.setCursor(0,45);
                    display.println("from 0 to 5");                               
                    display.setCursor(45,90);
                    display.println("Select");   
                    display.setTextSize(8);
                    display.setCursor(80,130);
                    display.print(TxPower); 
                } while (display.nextPage());
                    int triggerState = check_trigger();
                    if (triggerState == 1) {
                            TxPower++;    
                            vibration_confirm(30);
                    } 
                    else if (triggerState == 2) {// Long press → conferma selezione                   
                            vibration_confirm(200);
                            exit = true;
                    }
                if (TxPower > 5){TxPower = 0;}
                delay(100);
        }
        EEPROM.write(9, TxPower);
        EEPROM.commit();
        delay(1000);
        ESP.restart();

}
