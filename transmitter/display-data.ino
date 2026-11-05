extern SemaphoreHandle_t dataMutex;
extern TxData txData;

// ================= TASK DISPLAY =================
// Wrapper: e' l'unico posto da cui si puo' fare vTaskDelete(NULL) in sicurezza.
// L'enumerazione di vTaskDelete(NULL) e' per-task, quindi NON e' ereditata
// dalla funzione chiamata (i .ino vanno in un unico translation unit).
void displayTask(void *pvParameters) {
    displayTaskReal(pvParameters);
    vTaskDelete(NULL);
}

void displayTaskReal(void *pvParameters) {
    for (;;) {
        TxData snapshot;
        if (xSemaphoreTake(dataMutex, portMAX_DELAY)) {
            snapshot = txData;
            xSemaphoreGive(dataMutex);
        }
        updateDisplay(snapshot);
        vTaskDelay(pdMS_TO_TICKS(displayUpdateInterval));
    }
}

// ================= UPDATE DISPLAY =================
void updateDisplay(const TxData &) {
      // ── RECORD ALERT: schermata a priorità massima ──
      if(recordAlertActive && record > 0 && firstConnection){
            showRecord();
            return;
      }
      display.setPartialWindow(0, 0, 200, 200);
      display.firstPage();
      // AUTO-SLEEP (tick da 100 ms incrementati in espnowTask):
      // - 5 min (3000 tick) se MAI connessi alla ricevente
      // - 10 min (6000 tick) se connessi ma tavola ferma
      // - MAI se la tavola è in movimento (boardMoving azzera sleepTicks)
      unsigned long sleepLimit = firstConnection ? 6000UL : 3000UL;
      if(sleepTicks > sleepLimit){
            systemOff();       // auto sleep
      }else if(((timerOff > 1300) and (firstConnection) and (Tminutes > 0)  and (throttle < 10) and !pumping) or ((gravityAlarm) and (firstConnection) and (Tminutes > 0)  and (throttle < 10))) {
            showData();             
//      }else if ((timerOff > 450) and (firstConnection) and (inpVoltage > 0)){
//            showBatt();
      }else{
            do{
//                  if(((gravityAlarm) or (lock)) and (throttle > 10)){
                  if((gravityAlarm) and (lock) and (throttle > 10)){
                        displayBitmap(0, 0, lock_logo, 200, 200);
                        digitalWrite(LED,1);  
                        delay (100);           
                        digitalWrite(LED,0);  
                        delay(200);

                  }else if((firstConnection) and (!gravityAlarm) and (lock) and (throttle > 10)){
                        showUnlock();

                  }else if((firstConnection) and (throttle < 10) and (lock) and (timerOff < 70) and (Tminutes < 5) and (strlen(incomingReadings.message1) > 1) and (strlen(incomingReadings.message2) > 1)){ 
                        display.setTextSize(4);
                        display.setCursor(0,15);
                        display.print(incomingReadings.message1);
                        display.setCursor(0,60);
                        display.print(incomingReadings.message2);
                        display.setCursor(0,105);
                        display.print(incomingReadings.message3);
                        display.setTextSize(2);   
                        display.setCursor(18,175);
                        display.print("Be safe! Enjoy");   
                  }else{
                        showBottom();
                        digitalWrite(LED,0);
                        uint bar = map(throttle, 0, 99, 0, 200);
                        if(displayMode == 3){// display view 3
                              if ((throttle < 10) and (Tminutes > 3) and (firstConnection) and (!gravityAlarm) and (timerOff > 50) and (espStatus)){
                                          display.setTextSize(11);   
                                          display.setCursor(2,0);
                                          display.print(String(batpercentage));  
                                          display.print("%");
                                          display.fillRect(0, 80, 200, 2, GxEPD_BLACK);
                                          display.fillRect(0, 140, 200, 2, GxEPD_BLACK);
                                          display.setTextSize(6); 
                                          display.setCursor(10,90);
                                          if(surfingSeconds < 10){display.print("0");}
                                          display.print(surfingSeconds);
                                          display.print(":");
                                          if(surfingMinutes < 10){display.print("0");}
                                          display.print(surfingMinutes);
                              }else{
                                          display.setTextSize(15);   
                                          display.setCursor(20,0);
                                          display.print(String(batpercentage));        
                                          display.fillRect(0, 175, bar, 8, GxEPD_BLACK);
                                          display.setTextSize(5);
                                          display.setCursor(40,110);
                                          if(espStatus){ 
                                                if(!gravityAlarm){
                                                      display.print(String(watt,0));
                                                      display.print("W");
                                                      justOneAlert = true;
                                                }else{
                                                      display.print("LOCK");
                                                      if(justOneAlert){
                                                            vibrationTime = 400;
                                                            justOneAlert = false;
                                                      }
                                                }
                                          }
                              }
                        }else if(displayMode == 2){// display view 2                                                     
                              display.setTextSize(8);
                              display.setCursor(0,2);
                              display.print(throttle);    
                              display.setCursor(100,2);
                              display.print(batpercentage);
                              display.fillRect(0, 70, 200, 2, GxEPD_BLACK);
                              display.fillRect(95, 0, 2, 70, GxEPD_BLACK);
                              display.setTextSize(8);  
                              display.setCursor(0,80);
                        }else{  // display view 1
                              display.setTextSize(15);   
                              display.setCursor(20,0);
                              display.print(String(batpercentage));        
                              display.fillRect(0, 175, bar, 8, GxEPD_BLACK);
                              display.setTextSize(5);
                              display.setCursor(40,110);
                        }   
 /*                     // ── PUMP & GLIDE timer overlay (appare in tutti i modi) ──
                        if(pumping && throttle < 10){
                              display.setTextSize(4);
                              display.fillRect(130, 0, 70, 30, GxEPD_WHITE);
                              display.setCursor(132,5);
                              display.setTextColor(GxEPD_BLACK);
                              if(surfingMinutes < 10) display.print("0");
                              display.print(surfingMinutes);
                              display.print(":");
                              if(surfingSeconds < 10) display.print("0");
                              display.print(surfingSeconds);
                              display.setTextSize(2);
                              display.setCursor(155,25);
                              display.print("PUMP");
                              display.setTextColor(GxEPD_BLACK);
                        }
 */                         // display common data view
                        if (counter > 40){counter = 0;}
                        if (espStatus){
                              timerNoConnect = 0;
                              if (displayMode < 3) {
                                    if(!gravityAlarm){
                                          display.print(String(watt,0));
                                          justOneAlert = true;
                                    }else{
                                          display.print("LOCK");
                                          if(justOneAlert){
                                                vibrationTime = 400;
                                                justOneAlert = false;
                                          }
                                    }      
                              }
                              display.setTextSize(3);
                              if (counter < 11){                      
                                          display.setCursor(0,150);
                                          display.print("T.Time");
                                          display.setCursor(110,150);
                                          if(Tminutes < 10){display.print("0");}
                                          display.print(Tminutes);
                                          display.print(":");
                                          if(Tseconds < 10){display.print("0");}
                                          display.print(Tseconds);
                              }
                              if ((counter > 10) and (counter < 21)){                
                                          display.setCursor(0,150);
                                          display.print("M.Time");
                                          display.setCursor(110,150);
                                          if(Mminutes < 10){display.print("0");}
                                          display.print(Mminutes);
                                          display.print(":");
                                          if(Mseconds < 10){display.print("0");}
                                          display.print(Mseconds);
                              }
                              if ((counter > 20) and (counter < 31)){
                                          display.setCursor(0,150);
                              //            display.print("R.VOTE ");
                              //            display.print(powerIndex);
                              display.print("Waves: ");
                              display.print(wave);
                              }
                              if (counter > 30){
                                          display.setCursor(0,150);
                                          display.print("AvPw ");
                                          display.print(powerAverage);
                                          display.print("W");
                              }
                              if ((inpVoltage < 32.2) and (inpVoltage > 0)) {lowBatt = true;} // BATTERY ALARM ACTIVATION
                              if ((inpVoltage > 39.5) and (lowBatt)) {  // reset battery allarm
                                    lowBatt = false;
                                    lock = true;
                              }
                              if (lowBatt){vibrationTime = 1000;}                 
                              counter++;
                        }else{
                              digitalWrite(LED,1);
                              display.setTextSize(3);
                              display.setCursor(23,140);
                              display.println("NO SIGNAL");
                              timerNoConnect ++;
                              if ((firstConnection) and (timerNoConnect > 25)) {digitalWrite(LED, 1);} // time to flash led in case of no connection with board
                              if(timerNoConnect == 50) {                                    
                                    showNoConnect();
                                    return;
                              }else if(timerNoConnect > 50){
                                    timerOff = timerOff + 10;
                                    boardMoving = false;   // senza connessione: dati stale, non bloccare lo sleep
                                    firstConnection = false;      
                                    vibrationTime = 0;
                                    return;     
                              }
                        }   
                  }  
            }while (display.nextPage());
      }
}

void showRecord(){
            int recMin = record / 60;
            int recSec = record % 60;
            do{
                  display.setPartialWindow(0, 0, 200, 200);
                  display.firstPage();
                  display.fillScreen(GxEPD_WHITE);
                  display.setTextColor(GxEPD_BLACK);
                  display.setTextSize(3);
                  display.setCursor(0,0);
                  display.print("BEST RECORD");
                  display.drawLine(0, 35, 200, 35, GxEPD_BLACK);
                  display.setTextSize(6);
                  display.setCursor(20,55);
                  if(recMin < 10) display.print("0");
                  display.print(recMin);
                  display.print(":");
                  if(recSec < 10) display.print("0");
                  display.print(recSec);
                  display.setTextSize(3);
                  display.setCursor(20,120);
                  display.print("PUMP/WAVE");
                  display.setCursor(15,150);
                  display.print("BEST TIME");
                  showBottom();
            }while(display.nextPage());
}


void showBatt(){
      do{
            display.setTextSize(4);
            display.setCursor(20,0);
            display.print("BATTERY");    
            display.fillRect(0, 35, 200, 3, GxEPD_BLACK);
            display.setTextSize(11);
            display.setCursor(2,45);
            display.print(batpercentage);
            display.print("%");
            display.fillRect(0, 130, 200, 3, GxEPD_BLACK);        
            display.setTextSize(4);
            display.setCursor(50,142);
            display.print(inpVoltage, 1);
            display.print("V");
            showBottom();
      }while (display.nextPage());
      if ((inpVoltage > 39.5) and (lowBatt)) {
            lowBatt = false;
            lock = true;
      }
}

void showData() {
      static unsigned long lastSwitch = 0;
      static uint8_t showView = 0;
      digitalWrite(LED,0);

      if (millis() - lastSwitch >= 5000) {
            showView++;
            if(showView > 3){showView = 0;}
            lastSwitch = millis();
      }
      if (showView == 0) {
            // --- VIEW 1 ---
            do {
                  display.setTextSize(3);
                  display.setCursor(10,0);
                  display.print("RIDE DATA");//                  display.print(file);
                  display.setCursor(0,30);
                  display.print("T.Time");  
                  display.setCursor(110,30);
                  display.print(Tminutes); 
                  display.print(":");   
                  display.print(Tseconds);  
                  display.setCursor(0,60);
                  display.print("M.Time");
                  display.setCursor(110,60);  
                  display.print(Mminutes); 
                  display.print(":");   
                  display.print(Mseconds);
                  display.setCursor(0,90);
                  display.print("AvP ");
                  display.print(powerAverage);
                  display.print("W");         
                  display.setCursor(0,120);
                  display.print("B ");      
                  display.print(batpercentage); 
                  display.print("% ");   
                  display.print(inpVoltage, 1);
                  display.print("V"); 
                  display.setCursor(0,150);     
                  display.print("R.VOTE "); 
                  display.print(powerIndex);   
                  display.setCursor(0,180);
                  display.print("Score: ");
                  display.print(score);          // rider score dalla RX (fix 20/08/2026)
            } while (display.nextPage());
      } else if(showView == 2) {
            // --- VIEW 2 ---
//            display.firstPage();
            do {
                  display.setTextSize(3);
                  display.setCursor(0,0);
                  display.print("RIDE DATA"); //display.println(file);
                  display.setCursor(0,45);
                  display.print("Takeoff:"); display.println(takeOff); 
                  display.setCursor(0,80);
                  display.print("Waves: "); display.println(wave); 
                  display.setCursor(0,115);
                  display.println("Best time");
                  display.println("on the wave");
                  display.setCursor(45,170);
                  int waveBestMinutes = waveBestTime / 60;
                  int waveBestSeconds = waveBestTime % 60;
                  if (waveBestMinutes < 10) display.print("0");
                  display.print(waveBestMinutes);
                  display.print(":");
                  if (waveBestSeconds < 10) display.print("0");
                  display.print(waveBestSeconds);
            } while (display.nextPage());
      } else if(showView == 3) {
                  // --- VIEW 3 ---
                  showRecord();
      }else{
                  // --- VIEW 4 ---
                  showBatt();
      }

      vibrationTime = 0;
      if ((inpVoltage > 39.5) && lowBatt) {
            lowBatt = false;
            lock = true;
      }
}


void showNoConnect(){
      do {
            display.setPartialWindow(0, 0, 200, 200);
            display.firstPage();
            display.setTextSize(3);
            display.setCursor(0,0);
            display.print("NO CONNECT");
            display.setCursor(0,35);
            display.print("Check the");  
            display.setCursor(0,65);
            display.print("board box");                   
            display.setCursor(0,95);
            display.print("battery"); 
            display.setCursor(0,125);
            display.print("or the code");       
            display.setCursor(0,155);
            display.print("for pairing");  
            display.nextPage();          
      } while (display.nextPage());
}

void showBottom(){
          display.drawLine(0, 179, 200, 179, GxEPD_BLACK); 
          display.drawLine(0, 180, 200, 180, GxEPD_BLACK); 
          display.setTextSize(2);
          display.setCursor(0,185);
            display.print("Board "); 
          display.print(boardCode, DEC);  // receiver number        
//          display.print("Ride N.:"); 
//          display.print(file); 
          display.setCursor(145,185);
          display.print(receiverVersion); 
}


void displayBitmap(int16_t x, int16_t y, const uint8_t *bitmap, int16_t w, int16_t h) {
      do{
            display.setPartialWindow(x, y, w, h);   
            display.firstPage();
            display.fillScreen(GxEPD_WHITE); 
            display.drawBitmap(x, y, bitmap, w, h, GxEPD_BLACK);
      }while (display.nextPage());
}


void systemOff(){    
            displayBitmap(0, 0, TecnoFlyLogo, 200, 200); 
            delay(2000);                   
            display.hibernate();      
            delay(1000);       
            esp_deep_sleep_start();
}


void showQR(){  // show QR code for the on line manual of the controller
      check_trigger_off(); 
      bool exit = false;
      int timeCounter = 0;
      display.setPartialWindow(0, 0, 200, 200); // Imposta la finestra da aggiornare
      display.firstPage();
      display.fillScreen(GxEPD_WHITE); // Riempi lo schermo di bianco   
      display.setTextSize(2);
      display.setCursor(30,155);
      display.print("USER MANUAL");            
      display.setCursor(8,180);
      display.print("Trigger to exit");   
      delay(400);
      displayBitmap(25, 0, QRcode, 150, 150);    
      check_trigger();
}

void showUnlock(){
            do{
                  display.setPartialWindow(0, 0, 200, 200);
                  display.firstPage();
                  display.fillScreen(GxEPD_WHITE);
                  display.setTextColor(GxEPD_BLACK); 
                  display.setTextSize(4);
                  display.setCursor(15,15);
                  display.print("Press 2");
                  display.setCursor(15,60);
                  display.print("Seconds");
                  display.setCursor(75,105);
                  display.print("to");                  
                  display.setCursor(30,150);
                  display.print("UNLOCK");
            }while(display.nextPage());
            delay(1000);
}

void showGo(){
            do{
                  display.setPartialWindow(0, 0, 200, 200);
                  display.firstPage();
                  display.fillScreen(GxEPD_WHITE);
                  display.setTextColor(GxEPD_BLACK); 
                  display.setTextSize(6);
                  display.setCursor(10,15);
                  display.print("READY");
                  display.setCursor(55,75);
                  display.print("TO");
                  display.setCursor(50,140);
                  display.print("GO!");                  
            }while(display.nextPage());
            delay(1000);
}