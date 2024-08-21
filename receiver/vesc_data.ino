void getVescData(){  
if (millis() - lastDataVesc >= 1000) {       
//Serial.println(EspDelays);
//Serial.println(LoRaDelays);
//if(espStatus == true){Serial.println("ESP fails");}       
//if(loraStatus == true){Serial.println("LoRa fails");}
          if(firtsConnection){ //increment total time
                totalTime ++;
                Tminutes = totalTime / 60;
                Tseconds = totalTime - 60 * Tminutes;
          }
          lastDataVesc = millis(); 
          if (inactivityCounter > 30){recordEnable = false;} //seconds to stop data record if inactivity
          if(firtsConnection and recordEnable){ //increment time of riding
                rowCounter ++;
                Rminutes = rowCounter / 60;
                Rseconds = rowCounter - 60 * Rminutes;
          }
          receiverVersion = VERSION.toFloat();
          if ( UART.getVescValues() ) {
                  rpm = UART.data.rpm / (Poles / 2);                                // UART.data.rpm returns cRPM.  Divide by no of pole pairs in the motor for actual.
                  voltage = (UART.data.inpVoltage) + voltageOffset;      
                  current = (UART.data.avgInputCurrent);   
                  tempMosfet = (UART.data.tempMosfet);    
                  if(current<0){
                      power = 0;
                  }else{
                      power = voltage*current;
                  }
                  amphour = (UART.data.ampHours);                                  
                  watthour = watthour + (current*voltage/3600);  
                  motorCurrent = (UART.data.avgMotorCurrent);
                  if(power > 30){
                        totalWatt = totalWatt + power;
                        powerCounter ++;
                        powerAverage = totalWatt / powerCounter;
                        // POWER INDEX calc, HIGH power index is good performance MORE OF 50
                        if (Rminutes < 5 or powerAverage < 400){
                                powerIndex = 0;
                        }else{
                                int lowConsuption = 500; int highConsuption = 1500; int highRidingTime = 30; int lowRidingTime = 5;
                                int powerIndex1 = map(powerAverage, lowConsuption, highConsuption, 100, 0); // funcion of average power
                                int powerIndex2 = map(Rminutes, lowRidingTime, highRidingTime, 0, 100); // funcion of riding time
                                if(powerIndex1 > 99){powerIndex1 = 99;}
                                if(powerIndex2 > 99){powerIndex2 = 99;} 
                                powerIndex = (powerIndex1 + powerIndex2) / 2;
                        }         
                  } 
////////////////////////////////////////////////////////////////////////////////////////////
                  // calc of battery percentage from power consuption wh
                  if((batpercentage > 45) and (voltage <= 38) and (watthour < 110) and (power < 100)) {batCapacity = 250 * batCapacity / battFullCapacity;}
                  //if((voltage < 34.5) and (batpercentage > 20) and (power < 200)) {batCapacity = 200 * batCapacity / battFullCapacity;}

                  batpercentage = batCapacity - watthour;
                  if(watthour > batCapacity){batpercentage = 0;}
                  int batPercentageCapacity = map(batpercentage, 0, batCapacity, 0, 99);
                  batpercentage = map(batpercentage, 0, batCapacity, 0, percentageOfCapacity); 
////////////////////////////////////////////////////////////////////////////////////////////

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
                  data.Rminutes = Rminutes;
                  data.Rseconds = Rseconds;
                  data.powerCounter = powerCounter; 
                  data.powerAverage = powerAverage;
                  data.powerIndex = powerIndex;
                  data.receiverVersion = receiverVersion;
                  data.Tminutes = Tminutes;
                  data.Tseconds = Tseconds;  

 /*                  //Debug on serial
                  int SDthrottle = map(throttle, 0, 180, 0, 99); 
                  Serial.print("RPM "); Serial.print(rpm,0);Serial.print("\t");
                  Serial.print("|V "); Serial.print(voltage,1);Serial.print("\t");
                  Serial.print("|I "); Serial.print(current,1);Serial.print("\t");
                  Serial.print("|W "); Serial.print(power,0);Serial.print("\t");                  
                  Serial.print("|T "); Serial.print(tempMosfet,1);Serial.print("\t");
                  Serial.print("|B "); Serial.print(batpercentage);Serial.print("%\t");
                  Serial.print("|TH "); Serial.print(SDthrottle);Serial.print("%\t");
                  Serial.print("|delay "); Serial.print(delays);Serial.print("\t");
                  Serial.print("|BC "); Serial.print(batCapacity);Serial.print("Wh\t");
                  Serial.println();
*/            
                  if(sdcard and recordEnable){
                        appendToFile(dataFileName);
                        rideToJson();
                  }
          }else{
//                  Serial.println("No VESC data");            
                  data.voltage = 0.0;
                  data.tempMosfet = 0;
                  data.current = 0.0;
                  data.power = 0;
                  data.batt = 0;
                  data.file = fileName;
                  data.powerAverage = 0;
                  data.receiverVersion = receiverVersion;
                  data.Tminutes = Tminutes;
                  data.Tseconds = Tseconds;  
          }
          esp_now_send(broadcastAddress, (uint8_t *) &data, sizeof(data)); 
          inactivityCounter++;       
      }
}

void appendToFile(const char * path){
      file = SD.open(path, FILE_APPEND);
      if(!file){
          Serial.println("Failed to open txt file for appending");
          return;
      }
      if(file){
          int SDthrottle = map(throttle, 0, 180, 0, 99); // Write the PWM signal to the ESC (0-255).
          //file.print(rowCounter); file.print("\t"); file.print("|");
          if (Rminutes<10){file.print("0");}
          file.print(Rminutes); file.print(":"); 
          if (Rseconds<10){file.print("0");}
          file.print(Rseconds); file.print("\t"); file.print("|");
          file.print("R "); file.print(rpm,0); file.print("\t");
          file.print("|V "); file.print(voltage,1); file.print("\t");
          file.print("|I "); file.print(current,1); file.print("\t");
          file.print("|M "); file.print(motorCurrent,1); file.print("\t");
          file.print("|W "); file.print(power,0); file.print("\t");
          file.print("|TH "); file.print(SDthrottle); file.print("%\t");   
          file.print("|B "); file.print(batpercentage); file.print("%\t");   
          file.print("|Wh "); file.print(watthour,0); file.print("\t");                  
          file.print("|T "); file.print(tempMosfet,1); file.print("\t");     
          file.print("|Pa "); file.print(powerAverage); file.print("W\t");
          file.print("|Pi "); file.print(powerIndex); file.print("\t");    
//          file.print("|delay "); file.print(delays); file.print("ms\t");          
          if(espStatus == true){file.print("|ESP"); file.print("\t");}       
          if(loraStatus == true){file.print("|LoRa"); file.print("\t");}               
          file.println();
      } else {
          Serial.println("Append failed");
      }
      file.close();
}

void writeToFile(const char * path){
  file = SD.open(path, FILE_WRITE);
  if (file){
      file.print("");
      file.close(); 
      Serial.println(" New data file created!");
   }else{ 
      Serial.println(" Error opening file");
   }
}

void fileNumber(const char * path, int fileNumber){
  file = SD.open(path, FILE_WRITE);
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
          if(offset > 100){
              voltageOffset = (float)-(offset - 100) / 100;
          }else{
              voltageOffset = (float)offset / 100;
          }
          Serial.print("VOLTAGE OFFSET: "); Serial.print(voltageOffset); Serial.println("V");
}

void batStatus(){   // staus of battery at start up
      offsetCalc();

// POWER INDEX TEST AREA
/*
powerAverage = 900;
Rminutes = 7;
if (Rminutes < 5 or powerAverage < 400){
  powerIndex = 0;
  }else{
                        int lowConsuption = 500; int highConsuption = 1500; int highRidingTime = 30; int lowRidingTime = 10;
                        int powerIndex1 = map(powerAverage, lowConsuption, highConsuption, 100, 0); // funcion of average power
                        int powerIndex2 = map(Rminutes, lowRidingTime, highRidingTime, 0, 100); // funcion of riding time
                   //     if(powerIndex1 > 99){powerIndex1 = 99;}
                   //     if(powerIndex2 > 99){powerIndex2 = 99;}
          Serial.print("INDEX1: "); Serial.println(powerIndex1); 
          Serial.print("INDEX2: "); Serial.println(powerIndex2); 
                        powerIndex = (powerIndex1 + powerIndex2) / 2;
          
}         
Serial.print("INDEX: "); Serial.println(powerIndex);
*/


      for (int a=0; a<5; a++) {
          if ( UART.getVescValues() ) {
                  voltage = (UART.data.inpVoltage) + voltageOffset;                                 //Battery Voltage
                  tempMosfet = (UART.data.tempMosfet);    
                  Serial.print("Battery Voltage: "); Serial.print(voltage,1);Serial.println("V");
                  batpercentage = (voltage - 3.2*10) / (42 - 3.2*10) * 100;   // battery 10s
                  if(batpercentage > 99){batpercentage = 99;}
                  batCapacity = map(batpercentage, 0, 99, 0, battFullCapacity);

                  int V = voltage * 10;
                  int batConst = constrain(V, batEmptyValue, batFullValue);
                  batpercentage = map(batConst, batEmptyValue, batFullValue, 0, 99);

                  percentageOfCapacity = batpercentage;
                  Serial.print("Battery Capacity: "); Serial.print(batCapacity);Serial.println("Wh");
                  Serial.print("Battery: "); Serial.print(batpercentage);Serial.println("%");
                  vescReady = true;
                  if(batpercentage>0){return;}
          }else{
                   Serial.println("NO VESC DATA!");
                   batCapacity = 0;
          }
          delay(500);
      }
}

void dataFileCreate(){
      // check for file number
    const char* path = "/fileNumb.txt";
    size_t len = 0;
//    Serial.printf("Reading file: %s\n", path);
    File file = SD.open(path);
    fileName;
    if(file.available()){
        if(file.size() == 0){
              Serial.println("File number empty!");
              fileNumber(path, 0); // creo nuovo file
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
 //             Serial.println("File number incrementato!");              
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
