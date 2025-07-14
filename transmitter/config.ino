// EEPROM byte usage:
// 0-3 = valMax; 4-7 = valMin; 8-11 = ???; 13-14 = MAC ADD; 16-19 = THmode; 20-23 = display mode

void config_menu(){
      EEPROM.get(20, displayMode); // display view mode
      EEPROM.get(16, THmode); //throttle mode
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
            while (hallValue > valMiddle){  
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
            while (hallValue < valMiddle){  // throttle off 
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
            digitalWrite(LED_BUILTIN,1);  
            delay (300);
            digitalWrite(VIBRATOR,0);            
            digitalWrite(LED_BUILTIN,0);  
            delay(1000);
//          selectBoost();  
//            delay(1500);
            selectThrottleMode(); // throttle mode
            delay(1000);
            selectDisplayMode(); // display view mode
            delay(1000);
            pairing();
            delay(1000);
            ESP.restart();
      }  
/*      Serial.print("Hall max: ");
      Serial.println(valMax);
      Serial.print("Hall min: ");
      Serial.println(valMin); */
      display.clearDisplay();
      display.setTextSize(3);
      display.setCursor(20,0);
      display.print("BOARD");
      display.setCursor(25,40);
      display.print("CODE");  
      display.setTextSize(4);
      display.setCursor(5,85);
      display.print(boardCode);    
      display.display();
      delay(2000);
      /*
      digitalWrite(VIBRATOR,1);
      delay(200);          
      digitalWrite(VIBRATOR,0);*/
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
                        digitalWrite(LED_BUILTIN,1);                       
                        delay (300);
                        digitalWrite(VIBRATOR,0);
                        digitalWrite(LED_BUILTIN,0);                       
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

void selectDisplayMode(){
      bool exit = false;
      int timeCounter = 0;
      while(exit == false){
            display.clearDisplay();
            display.setTextSize(2);
            display.setCursor(25,0);
            display.println("DISPLAY");
            display.setCursor(40,25);
            display.println("VIEW");
            display.setCursor(10,75);
            display.println("Display ");            
            if (displayMode == 1){
                display.setCursor(103,75);
                display.println("1");
            }else if(displayMode == 2 ){
                display.setCursor(103,75);
                display.print("2"); 
            }else if(displayMode > 2 ){
                display.setCursor(103,75);
                display.print("3");     
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
                        digitalWrite(LED_BUILTIN,1);
                        delay (300);
                        digitalWrite(VIBRATOR,0);
                        digitalWrite(LED_BUILTIN,0);
                    }
                    delay (200);
                }
                if (timeCounter < 6){displayMode ++;}
                if (displayMode > 3){displayMode = 1;}
                timeCounter = 0;  
            }  
            delay(100);
      }
      EEPROM.put(20, displayMode);
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
            display.setTextSize(2);
            display.setCursor(27,0);
            display.println("SENSOR");
            display.setCursor(32,20);
            display.println("ISSUE");            
            display.setTextSize(1);
            display.setCursor(30,45);
            display.print(valMin);
            display.setCursor(65,45);
            display.print(valMax);
            display.setCursor(5,60);
            display.println("TO DO PROCEDURE:");
            display.display();
            delay (6000);
            display.setCursor(5,80);
            display.println("- PULL TRIGGER...");
            display.display();
            delay(3000);
            hallValue = analogRead(hallPin);
            valMax = hallValue - 10;
            EEPROM.put(0, valMax); 
            display.setCursor(5,100);
            display.println("- RELEASE TRIGGER...");
            display.display();
            delay(3000);
            hallValue = analogRead(hallPin);
            valMin = hallValue + 20;
            EEPROM.put(4, valMin); 
            boolean ok = EEPROM.commit();
//            Serial.println((ok) ? "Hall values stored into eeprom" : "EEPROM write failed");
            display.setCursor(5,120);
            display.println(" CALIBRATION DONE");
            display.display();
            delay(5000);
            valMiddle = ((valMax - valMin) / 2) + valMin;
            display.clearDisplay();
            if(valMax<=valMin or valMax>1000 or valMin>900 or valMax<400 or (valMax - valMin) < 40){hall_issue();}
}

void hall_issue(){
      display.clearDisplay();
      display.setTextSize(2);
      display.setCursor(20,0);
      display.print("TRIGGER");
      display.setCursor(30,20);
      display.print("SENSOR");  
      display.setCursor(35,40);
      display.print("ERROR");                   
      display.setCursor(0,65);
      display.print("Restart"); 
      display.setCursor(0,85);
      display.print("and do the");       
      display.setCursor(0,105);
      display.print("procedure.");  
      display.display();
      while(true){
          delay(100);
          if(timerOff > 3000){ESP.deepSleep(0);}  
          timerOff ++;
      }
}

void wait_trigger(){
    read_throttle();
    while(throttle < 50){
        read_throttle();
        delay(100);
    }
    check_trigger_off();
}

void check_trigger_off() {
    while(throttle > 50){
        read_throttle();
        delay(100);
    }
}

int check_trigger() {
    uint32_t startTime = millis();
    read_throttle();
    if (throttle > 50) { 
        while (throttle > 50) { 
            read_throttle();
            if (millis() - startTime >= 1200) { // 6*200ms → Long press
                digitalWrite(VIBRATOR, 1);
                digitalWrite(LED_BUILTIN,1); 
                delay(300);
                digitalWrite(VIBRATOR, 0);
                digitalWrite(LED_BUILTIN,0);                
                return 2;  // Long press
            }
        }
        return 1;  // Short press
    }
    return 0; 
}

void selectBoost(){
      Serial.print("work in progress...");
}