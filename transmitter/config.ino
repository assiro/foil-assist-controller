void config_menu(){
      EEPROM.get(8, TXmode);
      if (TXmode > 2){TXmode = 0;}
      EEPROM.get(0, valMax);
      EEPROM.get(4, valMin);
      valMiddle = ((valMax - valMin) / 2) + valMin;
      if(valMax<=valMin or valMax>1000 or valMin>900 or valMax<400 or (valMax - valMin)<50){hallCalibration();}
      hallValue = analogRead(hallPin);
      if (hallValue > valMiddle){ 
            throttle = 200;
            for (int i = 0; i <= 20; i++) {      
            esp_now_send(broadcastAddress, (uint8_t *) &throttle, sizeof(throttle));// Send data via ESP-NOW
            delay(100);
            }

            WiFi.mode(WIFI_STA);  // Set device as a Wi-Fi Station
            WiFi.begin(ssid, password);
            valMax = hallValue - 3;
            EEPROM.put(0, valMax);  // int - so 4 bytes (next address is '4')
            while (hallValue > valMiddle){  // rilascia throttle all'avvio per sicurezza
                  delay(200);
                  hallValue = analogRead(hallPin);
                  display.clearDisplay();
                  display.setTextSize(2);
                  display.setCursor(2,5);
                  display.println("SETUP MODE");
                  display.setTextSize(1);
                  display.setCursor(0,35);
                  display.print("Hall Calibration");                  
                  display.setCursor(105,35);
                  display.print(hallValue);
                  display.display();
                  counter++;
                  if(counter > 500){hallCalibration();}
                  if(WiFi.status() == WL_CONNECTED) {
                        OTAstart();
                        ArduinoOTA.handle();
                        display.setTextSize(1);
                        display.setCursor(0,75);
                        display.print("WiFi & OTA Connected!");
                        display.display();
                  }
            }
            delay(500);
            valMin = analogRead(hallPin) + 15;
            display.setCursor(30,55);
            display.print(valMin);
            display.setCursor(80,55);
            display.print(valMax);
            display.display();
            EEPROM.put(4, valMin);
            boolean ok = EEPROM.commit();
            Serial.println((ok) ? "Hall values stored into eeprom" : "EEPROM write failed");
            valMiddle = ((valMax - valMin) / 2) + valMin;
            OTAstart();
            while (hallValue < valMiddle){  // trhrottle off per restare connesso in rete e aggionare controller
                delay(100);
                hallValue = analogRead(hallPin);
                if(WiFi.status() == WL_CONNECTED) {              
                        ArduinoOTA.handle();
                        display.setTextSize(1);
                        display.setCursor(0,75);
                        display.print("WiFi & OTA Connected!");
                        counter++;
                        display.setCursor(0,90);
                        display.print("Check for Upgrade...");
                        if (counter > 30){
                              checkUpgrade();
                              counter = 0;
                        }
                        display.display();
                }
            }
            digitalWrite(VIBRATOR,1);
            delay (300);
            digitalWrite(VIBRATOR,0);            
//            delay(1500);
//          selectBoost();


            if(noLora == false) {
//                selectMode();
                TXmode = 2;
            }else{
                TXmode = 0;
            }     
            delay(1500);
            selectReceiver();
            delay(1000);
            ESP.restart();
      }  
      Serial.print("Hall max: ");
      Serial.println(valMax);
      Serial.print("Hall min: ");
      Serial.println(valMin); 
}

void selectReceiver(){
      bool exit = false;
      int timeCounter = 0;
      while(exit == false){
            display.clearDisplay();
            display.setTextSize(2);
            display.setCursor(18,0);
            display.println("RECEIVER");
            display.setTextSize(1);
            display.setCursor(12,30);
            display.println("Select a receiver");
            display.setCursor(15,100);
            display.println("Receiver firmware");
            display.setCursor(40,115);
            display.print("Ver. ");
            display.print(receiverVersion); 
            display.setTextSize(2);
            display.setCursor(5,60);
            display.print("Receiver ");
            if (RXmode == 1){
                display.setCursor(110,60);
                display.print("1");
            }else if(RXmode == 2){
                display.setCursor(110,60);
                display.print("2");
            }else if(RXmode == 3){    
                display.setCursor(110,60);
                display.print("3"); 
            }else if(RXmode == 4){    
                display.setCursor(110,60);
                display.print("4"); 
            }else if(RXmode == 5){    
                display.setCursor(110,60);
                display.print("5"); 
            }
            display.display();
            hallValue = analogRead(hallPin);
            if (hallValue > valMiddle){      
                delay(200);      
                while (hallValue > valMiddle){
                    hallValue = analogRead(hallPin);
                    timeCounter ++;
                    if (timeCounter == 6) {
                        exit = true;
                        digitalWrite(VIBRATOR,1);
                        delay (300);
                        digitalWrite(VIBRATOR,0);
                    }
                    delay (200);
                }
                if (timeCounter < 6){RXmode ++;}
                if (RXmode > 5){RXmode = 1;}
//                Serial.print(RXmode, DEC);
                timeCounter = 0;  
            }  
            delay(100);
      }
      EEPROM.put(12, RXmode);
      boolean ok = EEPROM.commit();
      Serial.println((ok) ? "Hall values stored into eeprom" : "EEPROM write failed");
}


void selectMode(){
      bool exit = false;
      int timeCounter = 0;
      while(exit == false){
            display.clearDisplay();
            display.setTextSize(2);
            display.setCursor(2,0);
            display.println("SETUP MODE");
            display.setTextSize(1);
            display.setCursor(18,25);
            display.println("TRASMITION MODE");
            if (TXmode == 0){
                display.setCursor(40,45);
                display.print("ESP-NOW");
            }else if(TXmode == 1){
                display.setCursor(50,45);
                display.print("LORA");
            }else if(TXmode == 2){    
                display.setCursor(25,45);
                display.print("ESP-NOW + LORA"); 
            }
            display.display();
            hallValue = analogRead(hallPin);
            if (hallValue > valMiddle){      
                delay(200);      
                while (hallValue > valMiddle){
                    hallValue = analogRead(hallPin);
                    timeCounter ++;
                    if (timeCounter == 6) {
                        exit = true;
                        digitalWrite(VIBRATOR,1);
                        delay (300);
                        digitalWrite(VIBRATOR,0);
                    }
                    delay (200);
                }
                if (timeCounter < 6){TXmode ++;}
                if (TXmode > 2){TXmode = 0;}
                Serial.print(TXmode, DEC);
                timeCounter = 0;  
            }  
            delay(100);
      }
      EEPROM.put(8, TXmode);
      boolean ok = EEPROM.commit();
      Serial.println((ok) ? "Hall values stored into eeprom" : "EEPROM write failed");
}

void hallCalibration(){
            Serial.print("Hall max: ");
            Serial.println(valMax);
            Serial.print("Hall min: ");
            Serial.println(valMin);
            Serial.print("Hall middle: ");
            Serial.println(valMiddle);            
            display.clearDisplay();
            display.setTextSize(1);
            display.setCursor(33,10);
            display.println("HALL ISSUE");
            display.setCursor(30,25);
            display.print(valMin);
            display.setCursor(65,25);
            display.print(valMax);
            display.display();
            delay (5000);
            display.setCursor(5,40);
            display.println("PULL TRIGGER...");
            display.display();
            delay(3000);
            hallValue = analogRead(hallPin);
            valMax = hallValue - 10;
            EEPROM.put(0, valMax); 
            display.setCursor(5,55);
            display.println("RELEASE TRIGGER...");
            display.display();
            delay(3000);
            hallValue = analogRead(hallPin);
            valMin = hallValue + 20;
            EEPROM.put(4, valMin); 
            boolean ok = EEPROM.commit();
            Serial.println((ok) ? "Hall values stored into eeprom" : "EEPROM write failed");
            display.setCursor(5,70);
            display.println("CALIBRATION DONE");
            display.display();
            delay(3000);
            valMiddle = ((valMax - valMin) / 2) + valMin;
            display.clearDisplay();
}


void selectBoost(){
      Serial.print("work in progress...");
}