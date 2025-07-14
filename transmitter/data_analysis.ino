
void get_analysis(){
      throttle = 250;    
      esp_now_send(broadcastAddress, (uint8_t *) &throttle, sizeof(throttle));// Send data via ESP-NOW     
      display.clearDisplay();
      display.setTextSize(2);
      display.setCursor(35,0);
      display.println("RIDES");
      display.setCursor(16,20);
      display.println("ANALYSIS");
      display.setCursor(20,45);
      display.print("Please!");
      display.setCursor(38,65);
      display.print("Wait");      
      display.setCursor(3,90);
      display.print("Last ride:    "); 
      uint rideDisplay = file;
      display.print(rideDisplay); 
      display.display();
      vibration_confirm();      

      uint32_t startTime = millis(); 
      while (incomingReadings.rideNumber == 0) {
          if (millis() - startTime >= 40000) { // 125 * 200ms = 25000ms (25 sec)
              display.clearDisplay();
              display.setTextSize(2);
              display.setCursor(0, 30);
              display.println("DATA ERROR");
              display.setCursor(0, 60);
              display.println("DOWNLOAD");
              display.display();
              wait_trigger();
              return;
          }
          delay(10);
      }
//      vibration_confirm(); 

      bool exit = true;
      while(exit){       
            uint trigger = check_trigger();    
            if(trigger == 2){exit = false;}
            display.clearDisplay();
            display.setTextSize(2);
            display.setCursor(0,0);
            display.print("RIDE N. "); display.println(rideDisplay);
            display.setCursor(0,30);
            display.print("Takeoff:"); display.println(takeOffBuffer[rideDisplay]); 
            display.setCursor(0,55);
            display.print("Waves: "); display.println(waveBuffer[rideDisplay]); 
            display.setCursor(0,80);
            display.print("Best time:  "); 
            display.setCursor(20,100);
            display.print(waveBestTimeBuffer[rideDisplay]); display.println(" sec.");
            display.display();
            delay(150);
            if(trigger == 1){rideDisplay ++;}
            if(rideDisplay > 10){rideDisplay = 1;}
      }
}


void unlock(){
      if (!gravityAlarm){
            display.clearDisplay(); 
            display.drawBitmap(0, 0, lock_logo, 128, 128, SH110X_WHITE);
            display.display(); // Show the display buffer on the screen
            bool exit = true;
            int goAnalysis = 0;
            while(exit){
            read_throttle();
            if(throttle < 10) {exit = false;}
            delay(200);
            goAnalysis++;
            if((goAnalysis > 20) and (firstConnection)){
                  get_analysis();
                  check_trigger_off();
                  return;
            }         
            }
            lock = false;
            vibration_confirm();
      }
}