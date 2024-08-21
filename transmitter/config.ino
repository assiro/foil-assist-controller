// EEPROM byte usage:
// 0-3 = valMax; 4-7 = valMin; 8-11 = TXmode; 12-15 = RXmode; 16-19 = THmode; 20-23 = display mode

void config_menu(){
      EEPROM.get(20, displayMode); // display view mode
      EEPROM.get(16, THmode); //throttle mode
//      Serial.print("Throttle mode: "); Serial.println(THmode);
      EEPROM.get(8, TXmode);
      if (TXmode > 2){TXmode = 0;}
//      TXmode = 0; if(noLora == false) {TXmode = 2;} // insert this row if remove select mode from menu
      EEPROM.get(0, valMax);
      EEPROM.get(4, valMin);
      valMiddle = ((valMax - valMin) / 2) + valMin;
      if(valMax<=valMin or valMax>1000 or valMin>900 or valMax<400 or (valMax - valMin) < 40){hallCalibration();}
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
            valMin = analogRead(hallPin) + 10;
            display.setCursor(30,55);
            display.print(valMin);
            display.setCursor(80,55);
            display.print(valMax);
            display.display();
            EEPROM.put(4, valMin);
            EEPROM.commit();
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
            delay(1500);
//          selectBoost();  
//            delay(1500);
            selectMode();   // SELECTION OF TRANSMIT MODE LORA ESP-NOW          
            delay(1500);
            selectThrottleMode(); // throttle mode
            delay(1500);
            selectDisplayMode(); // display view mode
            delay(1500);
            selectReceiver();
            delay(1000);
            ESP.restart();
      }  
/*      Serial.print("Hall max: ");
      Serial.println(valMax);
      Serial.print("Hall min: ");
      Serial.println(valMin); */
      display.clearDisplay();
      display.setTextSize(2);
      display.setCursor(35,0);
      display.print("BOARD");
      display.setCursor(30,30);
      display.print("NUMBER");  
      display.setTextSize(8);
      display.setCursor(48,60);
      display.print(RXmode);    
      display.display();
      delay(1500);
      digitalWrite(VIBRATOR,1);
      delay(200);          
      digitalWrite(VIBRATOR,0);
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
            }else if(RXmode == 6){
                display.setCursor(110,60);
                display.print("6");
            }else if(RXmode == 7){    
                display.setCursor(110,60);
                display.print("7"); 
            }else if(RXmode == 8){    
                display.setCursor(110,60);
                display.print("8"); 
            }else if(RXmode == 9){    
                display.setCursor(110,60);
                display.print("9"); 
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
                if (RXmode > 9){RXmode = 1;}
//                Serial.print(RXmode, DEC);
                timeCounter = 0;  
            }  
            delay(100);
      }
      EEPROM.put(12, RXmode);
      EEPROM.commit();
//      boolean ok = EEPROM.commit();
//      Serial.println((ok) ? "Hall values stored into eeprom" : "EEPROM write failed");
}

/////////////////////////////
// Throttle mode selection //
/////////////////////////////
void selectThrottleMode(){
      bool exit = false;
      int timeCounter = 0;
      while(exit == false){
            display.clearDisplay();
            display.setTextSize(2);
            display.setCursor(18,0);
            display.println("THROTTLE");
            display.setCursor(35,25);
            display.println("CURVE");            
            display.setCursor(32,55);
            display.println("Select");
            if(THmode == 0){    
                display.setCursor(28,90);
                display.print("LINEAR"); 
            } else if(THmode == 1){
                display.setCursor(28,90);
                display.print("STRONG");
            } else if(THmode == 2){    
                display.setCursor(28,90);
                display.print("MIDDLE"); 
            } else if(THmode == 3){    
                display.setCursor(40,90);
                display.print("SOFT"); 
            } else if(THmode == 4){    
                display.setCursor(25,90);
                display.print("ANGULAR"); 
            } else if(THmode == 5){    
                display.setCursor(48,90);
                display.print("ECO"); 
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
                if (timeCounter < 6){THmode ++;}
                if (THmode > 5){THmode = 0;}
                timeCounter = 0;  
            }  
            delay(100);
      }
      EEPROM.put(16, THmode);
      EEPROM.commit();
}
///////////////////////////////////////////////////

void selectMode(){
      bool exit = false;
      int timeCounter = 0;
      while(exit == false){
            display.clearDisplay();
            display.setTextSize(2);
            display.setCursor(2,0);
            display.println("SETUP MODE");
            display.setTextSize(1);
            display.setCursor(18,35);
            display.println("TRASMITION MODE");
            display.setTextSize(2);
            if (TXmode == 0){
                display.setCursor(25,65);
                display.println("ESP-NOW");
                display.setCursor(20,90);
                display.print("(2.4GHz)");
            }else if(TXmode == 1){
                display.setCursor(43,65);
                display.print("LoRa");
                display.setCursor(15,90);
                display.print("(433MHz)");
            }else if(TXmode == 2){    
                display.setCursor(25,65);
                display.print("ESP-NOW"); 
                display.setCursor(25,90);
                display.print("+ LoRa"); 
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
      EEPROM.commit();
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

void selectDisplayMode(){
      bool exit = false;
      int timeCounter = 0;
      while(exit == false){
            display.clearDisplay();
            display.setTextSize(2);
            display.setCursor(25,0);
            display.println("DISPLAY");
            display.setCursor(40,25);
            display.println("MODE");
            if (displayMode == 1){
                display.setCursor(15,75);
                display.println("Display 1");
            }else if(displayMode > 1 ){
                display.setCursor(15,75);
                display.print("Display 2"); 
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
                if (timeCounter < 6){displayMode ++;}
                if (displayMode > 2){displayMode = 1;}
                timeCounter = 0;  
            }  
            delay(100);
      }
      EEPROM.put(20, displayMode);
      EEPROM.commit();
}

void selectBoost(){
      Serial.print("work in progress...");
}