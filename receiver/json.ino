void rideToJson(){
      strcpy(jsonFileName,"");
      itoa(fileName, fileNameChar,10);
      strcat(jsonFileName, "/ride");
      strcat(jsonFileName, fileNameChar);
      strcat(jsonFileName, ".json");
    // Open file for writing
      File file = LittleFS.open(jsonFileName, FILE_WRITE);
      if (!file) {
//          Serial.println(F("Failed to create file"));
          return;
      }
      DynamicJsonDocument doc(512);       // Set the values in the document   
      doc["Tminutes"] = Tminutes;
      doc["Tseconds"] = Tseconds;
      doc["maxPower"] = maxPower;
      doc["minVoltage"] = minVoltage;  
      doc["maxCurrent"] = maxCurrent;
      doc["maxTemp"] = maxTemp; 
      doc["powerAverage"] = powerAverage;
      doc["powerIndex"] = powerIndex;   
      doc["watthour"] = watthour; 
      doc["Mminutes"] = Mminutes;
      doc["Mseconds"] = Mseconds;   
      doc["waveBestTime"] = waveBestTime;
      doc["wave"] = wave;
      doc["takeOff"] = takeOff;
      doc["rssiAvgSes"] = rssiAvg; 
      doc["lossPercent"] = lossPercent;
      if (serializeJson(doc, file) == 0) {Serial.println(F("Failed to write to file"));}
      file.close();
}


String dataToJson(){
      DynamicJsonDocument doc(512);
      doc["voltage"] = voltage;
      doc["current"] = current;
      doc["tempMosfet"] = tempMosfet;
      doc["power"] = power;    
      doc["batpercentage"] = batpercentage;
      doc["roll"] = round(roll), 0; 
      doc["pitch"] = round(pitch);
      doc["tilt"] = round(tilt);
      doc["rssiAvgSes"] = rssiAvg; 
      doc["lossPercent"] = lossPercent;
      String output;
      serializeJson(doc, output);
      return output;
}


// Prints the content of a file to the Serial
void printFile(const char *jsonFileName) {
      // Open file for reading
      File file = LittleFS.open(jsonFileName);
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






 