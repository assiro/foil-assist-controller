// EEPROM byte usage:
// 0-3 = valMax; 4-7 = valMin; 8 = TRUST; 9 = Tx Power; 12-15 = RXmode; 16-19 = THmode; 20-23 = display mode

void read_throttle(){
      hallValue = analogRead(hallPin);
      throttle = map(hallValue, valMin, valMax, 0, maxPower);
      throttle = constrain(throttle, 0, maxPower);
}

void hallCalibration(){
            Serial.print("Hall max: ");
            Serial.println(valMax);
            Serial.print("Hall min: ");
            Serial.println(valMin);
            Serial.print("Hall middle: ");
            Serial.println(valMiddle);            
            display.setPartialWindow(0, 0, 200, 200); // Imposta la finestra da aggiornare
            display.firstPage();
            display.fillScreen(GxEPD_WHITE);
            display.setTextSize(3);
            display.setCursor(45,0);
            display.println("SENSOR");
            display.setCursor(50,30);
            display.println("ISSUE");            
            display.setTextSize(2);
            display.setCursor(40,65);
            display.print(valMin);
            display.setCursor(120,65);
            display.print(valMax);
            display.setCursor(5,90);
            display.println("TO DO PROCEDURE:");
            display.nextPage();
            delay (6000);
            display.setCursor(0,120);
            display.println("-PULL TRIGGER");
            Serial.println("PULL TRIGGER");
            display.nextPage();
            delay(3000);
            hallValue = analogRead(hallPin);
            valMax = hallValue - 10;
            EEPROM.put(0, valMax); 
            display.setCursor(0,150);
            display.println("-RELEASE TRIGGER");
            Serial.println("RELEASE TRIGGER");
            display.nextPage();
            delay(3000);
            hallValue = analogRead(hallPin);
            valMin = hallValue + 20;
            EEPROM.put(4, valMin); 
            boolean ok = EEPROM.commit();
//            Serial.println((ok) ? "Hall values stored into eeprom" : "EEPROM write failed");
            display.setCursor(0,180);
            display.println("CALIBRATION DONE");
            display.nextPage();
            delay(5000);
            valMiddle = ((valMax - valMin) / 2) + valMin;
            if(valMax<=valMin or valMax>2800 or valMin>2600 or valMax<2100 or (valMax - valMin) < 100){hall_issue();}
}

void hall_issue(){
      display.setPartialWindow(0, 0, 200, 200); // Imposta la finestra da aggiornare
      display.firstPage();
      display.fillScreen(GxEPD_WHITE); // Riempi lo schermo di bianco
      display.setTextSize(3);
      display.setCursor(35,0);
      display.print("TRIGGER");
      display.setCursor(45,30);
      display.print("SENSOR");  
      display.setCursor(50,60);
      display.print("ERROR");                   
      display.setCursor(0,100);
      display.print("Restart"); 
      display.setCursor(0,130);
      display.print("and do the");       
      display.setCursor(0,160);
      display.print("procedure.");  
      display.nextPage();
      while(true){
        delay(1000);
        if(timerOff > 60){
            displayBitmap(0, 0, TecnoFlyLogo, 200, 200); 
            ESP.deepSleep(0);
        }  
        timerOff ++;
      }
}

void hallTest(){
      WiFi.mode(WIFI_STA);  // Set device as a Wi-Fi Station
      WiFi.begin(ssidService, passwordService);
      while (true){ 
            if(WiFi.status() == WL_CONNECTED) {
                  OTAstart();
                  ArduinoOTA.handle();
            }  
            hallValue = analogRead(hallPin);
            do{
                display.setPartialWindow(0, 0, 200, 200); // Imposta la finestra da aggiornare
                display.firstPage();
                display.setTextSize(3);
                display.setCursor(10,0);
                display.println("SETUP MODE");
                display.setTextSize(2);
                display.setCursor(5,35);
                display.print("Hall Calibration");                  
                display.setCursor(80,105);
                display.print(hallValue);
              }while (display.nextPage());    
            delay(500);                  
      }      
}

