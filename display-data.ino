void show_display(){
        if(timerOff > 9000){      // sleep mode after 15 min no use throttle
            display.clearDisplay();
            display.setTextSize(2);
            display.setCursor(6,26);
            display.println("SLEEP MODE");
            display.display(); 
            ESP.deepSleep(0);
        }  
        if(timerOff > 400) {
              showData();             
        }else if (timerOff > 150){
              showBatt();
        }else{

              display.clearDisplay();  // Clear the buffer
              showBottom();
              display.setTextSize(5);
              display.setCursor(2,2);
              display.print(throttle);    
              display.setCursor(65,2);
//              if(batpercentage > 99){batpercentage = 99;}
              display.print(batpercentage);
              display.drawRect(0, 45, 128,2, WHITE);
              display.drawRect(60, 0, 2, 45, WHITE);
              display.setTextSize(4);
              display.setCursor(5,52);     
              if (counter > 120){counter = 0;}
              if (espStatus == true){
      //              if (counter < 51){display.println("V: " + String(inpVoltage,1));}
      //              if (counter > 40 and counter < 81){display.println("C: " + String(avgInputCurrent,1));}
      //              if ((counter > 50) and (counter < 91)){display.println("T: " + String(tempMosfet,0));}  
      //              if ((counter > 90) and (counter < 121)){display.println("W: " + String(watt,0));}  
      //                if(lowBatt30  == false){
                              display.println(String(watt,0) + "W"); // visualizzo watt motore
                              if (counter < 80){                
                                      display.setTextSize(2);
                                      display.setCursor(0,90);
                                      display.print("Time ");
                                      display.print(minutes);
                                      display.print(":");
                                      display.print(seconds);
                              }
                              if ((counter > 80) and (counter < 101)){
                                      display.setTextSize(2);
                                      display.setCursor(10,90);
                                      display.print("RIDE N. ");
                                      display.print(file);
                              }
                              if ((counter > 100) and (counter < 121)){
                                      display.setTextSize(2);
                                      display.setCursor(0,90);
                                      display.print("AvPw ");
                                      display.print(powerAverage);
                                      display.print("W");
                              }
      /*                }else{
                              display.setTextSize(3);
                              display.setCursor(0,52); 
                              display.print("BATTERY");
                              display.setCursor(40,82); 
                              display.print("LOW");

                      }
      */
                    if(battery10s == false){
                            // battery low vibration alarm
                            if ((inpVoltage < 29.2) and (inpVoltage > 0) and (lowBatt50 == false)) {
                                  vibrationTime = 2000;
                                  lowBatt50 = true;
                                  vibration();
                            }
                            if ((inpVoltage < 27.5) and (inpVoltage > 0) and (lowBatt30 == false)) {
                                  lowBatt30 = true;
                                  vibrationTime = 4000;
                                  vibration();
                            }            
                            if ((inpVoltage < 26.0) and (inpVoltage > 0)) {lowBatt = true;}                
                            
                            if (inpVoltage > 31.5) { // reset flags low battery
                                  lowBatt50 = false;
                                  lowBatt30 = false;
                                  lowBatt = false;
                            }
                    }else{
                            if ((inpVoltage < 32.5) and (inpVoltage > 0)) {lowBatt = true;}
                            if ((inpVoltage < 34.5) and (inpVoltage > 0) and (lowBatt30 == false)) {
                                  lowBatt30 = true;
                                  vibrationTime = 4000;
                                  vibration();
                            } 
                            if ((inpVoltage < 36.5) and (inpVoltage > 0) and (lowBatt50 == false)) {
                                  vibrationTime = 2000;
                                  lowBatt50 = true;
                                  vibration();
                            }
                            if (inpVoltage > 39) { // reset flags low battery
                                  lowBatt50 = false;
                                  lowBatt30 = false;
                                  lowBatt = false;
                            }
                    }

                    if (lowBatt == true){
                            vibrationTime = 2500;
                            if((counter == 0) or (counter == 20) or (counter == 40) or (counter == 65) or (counter == 80) or (counter == 100)){  //ripetizione vibrazione ogni 2,5 secondi
                                vibration();
                            }
                    }
              }else{
                    if(TXmode != 1){
                          display.setTextSize(2);
                          display.setCursor(10,60);
                          display.println("NO SIGNAL");
                    }
                    timerNoConnect ++;
                    if(timerNoConnect > 300) {
                          showData();             
                    }
              }
        }
}

void showBatt(){
        display.clearDisplay();
        display.setTextSize(3);
        display.setCursor(2,2);
        display.print("BATTERY");    
        display.drawRect(0, 35, 128,2, WHITE);
        display.setTextSize(7);
        display.setCursor(0,48);
        display.print(batpercentage);
        display.print("%");
        showBottom();
        display.display();

}

void showData(){
      display.clearDisplay();
      display.setTextSize(2);
      display.setCursor(0,5);
      display.print("RIDE N. ");
      display.print(file);
      display.setCursor(0,40);
      display.print("Time ");  
      display.print(minutes); 
      display.print(":");   
      display.print(seconds); 
      display.setCursor(0,60);
      display.print("AvP ");
      display.print(powerAverage);
      display.print("W");                   
      display.setCursor(0,80);
      display.print("P index "); 
      display.print(powerIndex); 
      display.setCursor(0,100);
      display.print("BATT ");      
      display.print(batpercentage); 
      display.print("%");     
}

void showBottom(){
              display.drawRect(0, 115, 128,2, WHITE);
              display.setTextSize(1);
              display.setCursor(0,120);
              display.print("RX ");
              display.print(RXmode, DEC);  // receiver number        
              display.setCursor(40,120);
              display.print("Ride "); 
              display.print(file); 
              display.setCursor(90,120);
      //        display.print("Mode "); 
      //        display.print(TXmode); 
              display.print("V ");
              display.print(receiverVersion); 
}