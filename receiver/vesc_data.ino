// Get vesc data if connected and send call back by espnow

void getVescData(){  // get vesc data every second and store to flash
//      if (millis() - lastDataVesc >= 1000) {
            //stop record data record if inactivity 
            if ((inactivityCounter > timeNoRecord) or (gravityAlarm)){recordEnable = false;}  

if ((inactivityCounter > 100) and (gravityAlarm) and (accelerometerConnected) and (imuFile)){closeImuFile();}

            if(firstConnection and recordEnable){ //increment total time
                  totalTime ++;
                  Tminutes = totalTime / 60;
                  Tseconds = totalTime - 60 * Tminutes;
            }
            if(firstConnection and recordEnable and throttle > 20){ //increment time of motor riding
                  rowCounter ++;
                  Mminutes = rowCounter / 60;
                  Mseconds = rowCounter - 60 * Mminutes;
            }
            //data
            receiverVersion = VERSION.toFloat();
            data.receiverVersion = receiverVersion;
            data.Tminutes = Tminutes;
            data.Tseconds = Tseconds;
            data.takeOff = takeOff;
            data.wave = wave;
            data.waveBestTime = waveBestTime;
            data.gravityAlarm = gravityAlarm;
            data.riderAlert = riderAlert;
            if ( UART.getVescValues()){
                        rpm = UART.data.rpm / (Poles / 2);                                // UART.data.rpm returns cRPM.  Divide by no of pole pairs in the motor for actual.
                        voltage = roundf(((UART.data.inpVoltage) + voltageOffset) * 10) / 10;      
                        current = roundf(UART.data.avgInputCurrent);   
                        tempMosfet = roundf((UART.data.tempMosfet) * 10) / 10;   
                        watthour = UART.data.wattHours;
                        if(current < 0){
                        power = 0;
                        }else{
                        power = voltage*current;
                        }         
                        if(power > 30){
                              totalWatt = totalWatt + power;
                              powerCounter ++;
                              powerAverage = totalWatt / powerCounter;
      // POWER INDEX calc, HIGH power index is good performance MORE OF 50
                              if (Mminutes < 5 or powerAverage < 400){
                                    powerIndex = 0;
                              }else{
                                    int lowConsuption = 500; int highConsuption = 1500; int highRidingTime = 30; int lowRidingTime = 5;
                                    int powerIndex1 = map(powerAverage, lowConsuption, highConsuption, 100, 0); // funcion of average power
                                    int powerIndex2 = map(Tminutes, lowRidingTime, highRidingTime, 0, 100); // funcion of riding time
                                    powerIndex1 = constrain(powerIndex1, 0, 100);
                                    powerIndex2 = constrain(powerIndex2, 0, 100);
                                    powerIndex = (powerIndex1 + powerIndex2) / 2; //VOTE in decimal calculation of votation for the ride
                              }         
                        } 
      ////////////////////////////////////////////////////////////////////////////////////////////
                        // calc of battery percentage from power consuption wh
            //          if((batpercentage > 45) and (voltage <= 38) and (watthour < 110) and (power < 100)) {batCapacity = 250 * batCapacity / battFullCapacity;}
                        if((voltage < 32.5) and (batpercentage > 10) and (power < 200)) {batCapacity = 200 * batCapacity / battFullCapacity;}

                        batpercentage = batCapacity - watthour;
                        batpercentage = map(batpercentage, 0, batCapacity, 0, percentageOfCapacity); 
                        batpercentage = constrain(batpercentage, 0, 99);

                        if((batpercentage > 15) & (voltage < 33.5) & (power < 200)) {batpercentage = 10;}

                        // values peak calc
                        if(voltage < minVoltage){minVoltage = voltage;}
                        if(tempMosfet > maxTemp){maxTemp = tempMosfet;}
                        if(current > maxCurrent){maxCurrent = current;}
                        if(power > maxPower){maxPower = power;}
                        data.voltage = voltage;
                        data.tempMosfet = tempMosfet;
                        data.current = current;
                        data.power = power;
                        data.batt = batpercentage;
                        data.file = fileName;
                        data.Mminutes = Mminutes;
                        data.Mseconds = Mseconds;
                        data.powerCounter = powerCounter; 
                        data.powerAverage = powerAverage;
                        data.powerIndex = powerIndex;
//                        data.watthour = watthour;    
                        if(recordEnable){
                              appendToFile(dataFileName);
                              rideToJson();
                        }
            }else{
//                        Serial.println("No VESC data");          
                        data.voltage = 0.0;
                        data.tempMosfet = 0;
                        data.current = 0.0;
                        data.power = 0;
                        data.batt = 0;
                        data.file = fileName;
                        data.powerAverage = 0;
                        data.powerIndex = 0;
                        data.Mminutes = 0;
                        data.Mseconds = 0;    
            }
            esp_now_send(broadcastAddress, (uint8_t *) &data, sizeof(data)); 
            data.rideNumber = 0;
            inactivityCounter++;      
            riderAlert = false; 
//            lastDataVesc = millis();
//      }
}

void appendToFile(const char * path){
      file = LittleFS.open(path, FILE_APPEND);
      if(file){
            uint SDthrottle = map(throttle, 0, 180, 0, 99); 
            if (Tminutes<10){file.print("0");}
            file.print(Tminutes); file.print(":"); 
            if (Tseconds<10){file.print("0");}

// NEW DATA FILE FORMAT CSV format: time, RPM, voltage, current, power(W), throttle%, battery%, Wh, temp, power Average(W)
            file.print(Tseconds); file.print(",");
            file.print(rpm,0); file.print(",");
            file.print(voltage,1); file.print(",");
            file.print(current,0); file.print(",");
            file.print(power,0); file.print(",");
            file.print(SDthrottle); file.print(",");   
            file.print(batpercentage); file.print(",");   
            file.print(watthour,0); file.print(",");                  
            file.print(tempMosfet,0); file.print(",");     
            file.print(powerAverage); file.print(",");   
            file.print(rms); file.print(","); 
            file.print(peak); file.print(","); 
            
            file.print(tilt); file.print(","); 
            file.print(pitch); file.print(","); 
            file.print(roll); 
             

            file.println();         
      } else {
//          Serial.println("Append failed");
            return;
      }
      file.close();
}

void writeToFile(const char * path){
      file = LittleFS.open(path, FILE_WRITE);
      if (file){
            file.print("");
            file.close(); 
            Serial.println(" New data file created!");
      }else{ 
            Serial.println(" Error opening file");
      }
}


void fileNumber(const char * path, int fileNumber){
      file = LittleFS.open(path, FILE_WRITE);
      if (file){
            file.print(fileNumber);
            file.close(); 
      //      Serial.println("New file number created!");
      }else{ 
            Serial.println("Error creating file!");
      }
}


void offsetCalc(){    // VESC reading voltage offset calc
          int offset = EEPROM.read(8);
          if(offset>200){offset = 0;}
          if(offset > 100){
              voltageOffset = (float)-(offset - 100) / 100;
          }else{
              voltageOffset = (float)offset / 100;
          }
          Serial.print("VOLTAGE OFFSET: "); Serial.print(voltageOffset); Serial.println("V");
}

void batStatus(){   // staus of battery at start up
      offsetCalc();
      for (int a=0; a<5; a++) {
            if ( UART.getVescValues() ) {
                        voltage = (UART.data.inpVoltage) + voltageOffset;                                 //Battery Voltage
                        tempMosfet = (UART.data.tempMosfet);    
                        Serial.print("Battery Voltage: "); Serial.print(voltage,1);Serial.println("V");
      // SPERIMENTALE PER 12s/////////////////////////////////
                        if(voltage > 43){
                                    Serial.println("Battery 12s");
                                    batpercentage = (voltage - minvoltage12s) / (50 - minvoltage12s) * 100;   // battery 12s
                                    if(batpercentage > 99){batpercentage = 99;}
                                    Serial.print("Battery Full Capacity: "); Serial.println(battFullCapacity);

                                    batCapacity = map(batpercentage, 0, 99, 0, battFullCapacity);
                                    int V = voltage * 10;
                                    int batConst = constrain(V, batEmptyValue12s, batFullValue12s);
                                    batpercentage = map(batConst, batEmptyValue12s, batFullValue12s, 0, 99);
                                    batpercentage = constrain(batpercentage, 0, 99);
                                    percentageOfCapacity = batpercentage;
                                    Serial.print("Battery Capacity: "); Serial.print(batCapacity);Serial.println("Wh");
                                    Serial.print("Battery: "); Serial.print(batpercentage);Serial.println("%");
                                    vescReady = true;
                                    delay(600);
                                    if(batpercentage>0){return;}

      ////////////////////////////////  10s  ///////////////////////////////////////////////////////

                        }else{
                                    Serial.println("Battery 10s");
                                    batpercentage = (voltage - minvoltage) / (42 - minvoltage) * 100;   // battery 10s
                                    if(batpercentage > 99){batpercentage = 99;}
                                    Serial.print("Battery Full Capacity: "); Serial.println(battFullCapacity);
                                    batCapacity = map(batpercentage, 0, 99, 0, battFullCapacity);
      batCapacity = constrain(batCapacity, 0, battFullCapacity);
                                    int V = voltage * 10;
                                    int batConst = constrain(V, batEmptyValue, batFullValue);
                                    batpercentage = map(batConst, batEmptyValue, batFullValue, 0, 99);
                                    batpercentage = constrain(batpercentage, 0, 99);
                                    percentageOfCapacity = batpercentage;
                                    Serial.print("Battery Capacity: "); Serial.print(batCapacity);Serial.println("Wh");
                                    Serial.print("Battery: "); Serial.print(batpercentage);Serial.println("%");
                                    vescReady = true;
                                    delay(600);
                                    if(batpercentage>0){return;}
                        }
            }else{
                        Serial.println("NO VESC DATA!");
                        batCapacity = 0;
            }
            delay(500);
      }
}


void dataFileCreate(){
      // check for file number
      memset(dataFileName, 0, sizeof(dataFileName));      
      const char* path = "/fileNumb.txt";
      size_t len = 0;
      //    Serial.printf("Reading file: %s\n", path);
      File file = LittleFS.open(path);
      fileName;
      if(file.available()){
            if(file.size() == 0){
                  Serial.println("File number empty!");
                  fileNumber(path, 0); 
            }else{
                  char inChar;
                  inChar = file.read();
      //              Serial.println(inChar);
                  fileName = inChar - '0';
                  fileName++;
                  if(fileName > 9){ 
                        fileNumber(path, 0);
                        fileName = 0;
                  }else{
                        fileNumber(path, fileName);
                  }           
            } 
      }else{
            Serial.println("file number not present, created");
            fileNumber(path, 0);  // creo nuovo file
      }
      file.close();
      itoa(fileName, fileNameChar,10);
      strcat(dataFileName, "/vescDat");
      strcat(dataFileName, fileNameChar);
      strcat(dataFileName, ".txt");
      Serial.print(dataFileName);
      writeToFile(dataFileName);  // creo nuovo file dati
}
