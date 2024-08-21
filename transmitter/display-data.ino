void show_display(){
        if(timerOff > 9000){      // sleep mode after 15 min no use throttle
            display.clearDisplay();
            display.setTextSize(2);
            display.setCursor(6,26);
            display.println("SLEEP MODE");
            display.display(); 
            ESP.deepSleep(0);
        }  
        if((timerOff > 300) and (firstConnection) and (minutes > 0) and (!lock)) {
              showData();             
        }else if ((timerOff > 60) and (firstConnection) and (inpVoltage > 0) and (!lock)){
              showBatt();
        }else{
              display.clearDisplay();  // Clear the buffer
              if((firstConnection) and (lock) and (strlen(incomingReadings.message1) > 1) and (strlen(incomingReadings.message2) > 1)){ 
                    display.setTextSize(3);
                    display.setCursor(0,20);
                    display.print(incomingReadings.message1);
                    display.setCursor(0,55);
                    display.print(incomingReadings.message2);
                    display.setCursor(0,90);
                    display.print(incomingReadings.message3);
              }else{
                    showBottom();
                    if(displayMode == 2){
                          display.setTextSize(6);   
                          display.setCursor(15,0);
                          display.print(String(batpercentage) + "%");
                          uint bar = map(throttle, 0, 99, 0, 128);
                          display.drawRect(0, 48, bar,2, WHITE);
                          display.setTextSize(5);   
                    }else{
                          display.setTextSize(5);
                          display.setCursor(2,2);
                          display.print(throttle);    
                          display.setCursor(65,2);
                          display.print(batpercentage);
                          display.drawRect(0, 45, 128,2, WHITE);
                          display.drawRect(60, 0, 2, 45, WHITE);
                          display.setTextSize(4);
                    }   
                    display.setCursor(5,52);
                    if (counter > 120){counter = 0;}
                    if ((espStatus == true) or (TXmode == 1)){
                            display.println(String(watt,0));
                            if (counter < 41){                
                                    display.setTextSize(2);
                                    display.setCursor(0,95);
                                    display.print("TTime");
                                    display.setCursor(67,95);
                                    display.print(Tminutes);
                                    display.print(":");
                                    display.print(Tseconds);
                            }
                            if ((counter > 40) and (counter < 81)){                
                                    display.setTextSize(2);
                                    display.setCursor(0,95);
                                    display.print("MTime");
                                    display.setCursor(67,95);
                                    display.print(minutes);
                                    display.print(":");
                                    display.print(seconds);
                            }
                            if ((counter > 80) and (counter < 101)){
                                    display.setTextSize(2);
                                    display.setCursor(0,95);
                                    display.print("RIDE N. ");
                                    display.print(file);
                            }
                            if ((counter > 100) and (counter < 121)){
                                    display.setTextSize(2);
                                    display.setCursor(0,95);
                                    display.print("AvPw ");
                                    display.print(powerAverage);
                                    display.print("W");
                            }
                            if ((inpVoltage < 33) and (inpVoltage > 0)) {lowBatt = true;}
                            if ((inpVoltage > 39.5) and (lowBatt)) {
                                  lowBatt = false;
                                  lock = true;
                            }
                            if (lowBatt == true){
                                  vibrationTime = 2000;
                            }else{
                                  vibrationTime = 0;
                            }                 
                    }else{
                          if(TXmode != 1){
                                display.setTextSize(2);
                                display.setCursor(10,60);
                                display.println("NO SIGNAL");
                          }
                          timerNoConnect ++;
                          if(timerNoConnect > 300) {showNoConnect();}
                    }   
              }
        }
}
   


void showBatt(){
        display.clearDisplay();
        display.setTextSize(3);
        display.setCursor(2,2);
        display.print("BATTERY");    
        display.drawRect(0, 30, 128,2, WHITE);
        display.setTextSize(7);
        display.setCursor(0,40);
        display.print(batpercentage);
        display.print("%");
        display.setTextSize(2);
        display.setCursor(60,96);
        display.print(inpVoltage, 1);
        display.print("V");
        showBottom();
        display.display();
        if ((inpVoltage > 39.5) and (lowBatt)) {
              lowBatt = false;
              lock = true;
        }
}

void showData(){
      display.clearDisplay();
      display.setTextSize(2);
      display.setCursor(0,3);
      display.print("RIDE N. ");
      display.print(file);
      display.setCursor(0,30);
      display.print("MTime");
      display.setCursor(65,30);  
      display.print(minutes); 
      display.print(":");   
      display.print(seconds); 
      display.setCursor(0,50);
      display.print("AvP ");
      display.print(powerAverage);
      display.print("W");                   
      display.setCursor(0,70);
      display.print("P index "); 
      display.print(powerIndex); 
      display.setCursor(0,90);
      display.print("B ");      
      display.print(batpercentage); 
      display.print("% ");   
      display.print(inpVoltage, 1);
//      display.print("V"); 
      display.setCursor(0,110);     
      display.print("TTime ");  
      display.setCursor(65,110);
      display.print(Tminutes); 
      display.print(":");   
      display.print(Tseconds);       

//      display.print("Temp ");
//      display.print(tempMosfet, 1);            
      vibrationTime = 0;
      if ((inpVoltage > 39.5) and (lowBatt)) {
              lowBatt = false;
              lock = true;
      }
}

void showNoConnect(){
      display.clearDisplay();
      display.setTextSize(2);
      display.setCursor(0,0);
      display.print("NO CONNECT");
      display.setCursor(0,20);
      display.print("Check the");  
      display.setCursor(0,40);
      display.print("board box");                   
      display.setCursor(0,60);
      display.print("receiver"); 
      display.setCursor(0,80);
      display.print("number");       
      display.setCursor(0,100);
      display.print("or battery");  
      lock = true;
      firstConnection = false;      
      vibrationTime = 0;
}

void showBottom(){
              display.drawRect(0, 115, 128,2, WHITE);
              display.setTextSize(1);
              display.setCursor(0,120);
              display.print("RX ");
              display.print(RXmode, DEC);  // receiver number        
              display.setCursor(35,120);
              display.print("Ride "); 
              display.print(file); 
              display.setCursor(85,120);
      //        display.print("Mode "); 
      //        display.print(TXmode); 
              display.print("V ");
              display.print(receiverVersion); 
}


void vibration(){
          int timeOn = vibrationTime;
          if ((vibrationTimeOff + vibrationTime + timeOn < millis()) and (vibrationTime > 0)) {
                digitalWrite(VIBRATOR,1);
                vibrationTimeOff = millis();
          }else if(vibrationTimeOff + vibrationTime < millis()) {
                digitalWrite(VIBRATOR,0);
          }
}