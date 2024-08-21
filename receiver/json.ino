void rideToJson(){
      strcpy(jsonFileName,"");
      itoa(fileName, fileNameChar,10);
      strcat(jsonFileName, "/ride");
      strcat(jsonFileName, fileNameChar);
      strcat(jsonFileName, ".json");
    // Open file for writing
      File file = SD.open(jsonFileName, FILE_WRITE);
      if (!file) {
//          Serial.println(F("Failed to create file"));
          return;
      }
      DynamicJsonDocument doc(512);
      // Set the values in the document
      doc["minutes"] = Rminutes;
      doc["seconds"] = Rseconds;
      doc["maxPower"] = maxPower;
      doc["minVoltage"] = minVoltage;  
      doc["maxCurrent"] = maxCurrent;
      doc["maxTemp"] = maxTemp; 
      doc["powerAverage"] = powerAverage;
      doc["powerIndex"] = powerIndex;   

      // Serialize JSON to file
      if (serializeJson(doc, file) == 0) {
        Serial.println(F("Failed to write to file"));
      }
      // Close the file
      file.close();
      // Print test file
      //printFile(jsonFileName);
}


// Prints the content of a file to the Serial
void printFile(const char *jsonFileName) {
      // Open file for reading
      File file = SD.open(jsonFileName);
      if (!file) {
        Serial.println(F("Failed to read file"));
        return;
      }  
      // Extract each characters by one by one
      while (file.available()) {
        Serial.print((char)file.read());
      }
      Serial.println();
      // Close the file
      file.close();
}


String dataToJson(){
      DynamicJsonDocument doc(512);
      doc["voltage"] = voltage;
      doc["current"] = current;
      doc["tempMosfet"] = tempMosfet;
      doc["power"] = power;  
      doc["amphour"] = amphour;
      doc["watthour"] = watthour; 
      doc["rpm"] = rpm;
      doc["powerIndex"] = powerIndex;   
      doc["powerIndex"] = powerAverage;  
      doc["batpercentage"] = batpercentage;
      doc["minutes"] = Rminutes; 
      doc["seconds"] = Rseconds;
      doc["minVoltage"] = minVoltage;  
      doc["maxTemp"] = maxTemp;
      doc["maxCurrent"] = maxCurrent; 
      doc["maxPower"] = maxPower;
      if(loraStatus == true){
            doc["rssi"] = -100; 
            doc["snr"] = -15;
            doc["lora"] = 1500;
      }else{
            doc["rssi"] = LoRa.packetRssi(); 
            doc["snr"] = LoRa.packetSnr();
            doc["lora"] = LoRaDelays; 
      }
      doc["esp"] = EspDelays;
      doc["minutes"] = Tminutes; 
      doc["seconds"] = Tseconds;      
      String output;
      serializeJson(doc, output);
      return output;
}


