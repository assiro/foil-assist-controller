void getVescData(){  
if (millis() - lastDataVesc >= 1000) {       
//Serial.println(EspDelays);
//Serial.println(LoRaDelays);
//if(espStatus == true){Serial.println("ESP fails");}       
//if(loraStatus == true){Serial.println("LoRa fails");}
//Serial.print("RPM "); Serial.println(rpm,0);
          lastDataVesc = millis(); 

          if (inactivityCounter > 30){recordEnable = false;} //seconds to stop data record if inactivity

          if(firtsConnection and recordEnable){ //increment time of riding
                rowCounter ++;
                minutes = rowCounter / 60;
                seconds = rowCounter - 60 * minutes;
          }
          receiverVersion = VERSION.toFloat();
          if ( UART.getVescValues() ) {
                  rpm = UART.data.rpm / (Poles / 2);                                // UART.data.rpm returns cRPM.  Divide by no of pole pairs in the motor for actual.
                  voltage = (UART.data.inpVoltage);                                 //Battery Voltage
                  current = (UART.data.avgInputCurrent);   
                  tempMosfet = (UART.data.tempMosfet);    
                  power = voltage*current;
                  amphour = (UART.data.ampHours);                                  
                  watthour = amphour*voltage;           
                  if(power > 30){
                        totalWatt = totalWatt + power;
                        powerCounter ++;
                        powerAverage = totalWatt / powerCounter;
                        powerIndex = map(powerAverage, 500, 1700, 100, 0);
                        if(powerIndex > 99){powerIndex = 99;}
                  } 

                  batpercentage = batCapacity - watthour;
                  if(watthour > batCapacity){batpercentage = 0;}
                  int batPercentageCapacity = map(batpercentage, 0, batCapacity, 0, 99);
                  batpercentage = map(batpercentage, 0, batCapacity, 0, percentageOfCapacity); 
                  data.voltage = voltage;
                  data.tempMosfet = tempMosfet;
                  data.current = current;
                  data.power = power;
                  data.batt = batpercentage;
                  data.file = fileName;
                  data.minutes = minutes;
                  data.seconds = seconds;
                  data.powerCounter = powerCounter;
                  data.powerAverage = powerAverage;
                  data.powerIndex = powerIndex;
                  data.receiverVersion = receiverVersion;
              /*   
                  //Debug on serial
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

                  if(sdcard and recordEnable){appendToFile(dataFileName);}
          }else{
//                  Serial.println("No VESC data");            
                  data.voltage = 0.0;
                  data.tempMosfet = 0;
                  data.current = 0.0;
                  data.power = 0;
                  data.batt = 0;
                  data.file = fileName;
                  data.receiverVersion = receiverVersion;
          }
          esp_now_send(broadcastAddress, (uint8_t *) &data, sizeof(data)); 
          inactivityCounter++;       
      }
}

void appendToFile(const char * path){
      file = SD.open(path, FILE_APPEND);
      if(!file){
        Serial.println("Failed to open file for appending");
        return;
      }
      if(file){
          int SDthrottle = map(throttle, 0, 180, 0, 99); // Write the PWM signal to the ESC (0-255).
          //file.print(rowCounter); file.print("\t"); file.print("|");
          if (minutes<10){file.print("0");}
          file.print(minutes); file.print(":"); 
          if (seconds<10){file.print("0");}
          file.print(seconds); file.print("\t"); file.print("|");
          file.print("R "); file.print(rpm,0); file.print("\t");
          file.print("|V "); file.print(voltage,1); file.print("\t");
          file.print("|I "); file.print(current,1); file.print("\t");
          file.print("|W "); file.print(power,0); file.print("\t");
          file.print("|TH "); file.print(SDthrottle); file.print("%\t");   
          file.print("|B "); file.print(batpercentage); file.print("%\t");   
          file.print("|Wh "); file.print(watthour,0); file.print("\t");                  
          file.print("|T "); file.print(tempMosfet,1); file.print("\t");     
          file.print("|Pa "); file.print(powerAverage); file.print("W\t");
          file.print("|Pi "); file.print(powerIndex); file.print("\t");    
//          file.print("|delay "); file.print(delays); file.print("ms\t");          
          if(espStatus == true){file.print("|ESP fails"); file.print("\t");}       
          if(loraStatus == true){file.print("|LoRa fails"); file.print("\t");}               
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

void initSDCard(){
          if (!SD.begin(chipSelect)) {
              Serial.println("SD CARD initialization failed");
              sdcard = false;
          } else {
              Serial.println("Card is present.");
              sdcard = true;
          }
}

void batStatus(){
      for (int a=0; a<5; a++) {
          if ( UART.getVescValues() ) {
                  voltage = (UART.data.inpVoltage);                                 //Battery Voltage
                  tempMosfet = (UART.data.tempMosfet);    
                  Serial.print("Battery Voltage: "); Serial.print(voltage,1);Serial.println("V");
                  if(voltage > 34){
                        batpercentage = (voltage - 3.2*10) / (41.5 - 3.2*10) * 100;   // battery 10s
                        if(batpercentage > 99){batpercentage = 99;}
                        batCapacity = map(batpercentage, 0, 99, 0, 120);
                        Serial.println("Battery type 10s");
                  }else{
                        batpercentage = (voltage - 3.2*8) / (33.3 - 3.2*8) * 100;   // Percentuale = (V - Vminimo) / (Vmassimo - Vminimo) * 100
                        if(batpercentage > 99){batpercentage = 99;}
                        batCapacity = map(batpercentage, 0, 99, 0, 240);
                        Serial.println("Battery type 8s");
                  }
                  percentageOfCapacity = batpercentage;
                  Serial.print("Battery Capacity: "); Serial.print(batCapacity);Serial.println("Wh");
                  Serial.print("Battery: "); Serial.print(batpercentage);Serial.println("%");
                  vescReady = true;
                  return;
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
