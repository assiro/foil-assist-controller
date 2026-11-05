
// fix 22/10/2026: riscrive il riepilogo del ride a ZERO (non si cancella il file:
// la pagina rides.html non riuscirebbe a caricare la card finche non viene rigenerato).
void resetRideJson(){
      snprintf(jsonFileName, sizeof(jsonFileName), "/ride%d.json", fileName);
      File file = LittleFS.open(jsonFileName, FILE_WRITE);
      if (!file) { return; }
      file.print("{\"Tminutes\":0,\"Tseconds\":0,\"maxPower\":0,\"minVoltage\":0,\"maxCurrent\":0,\"maxTemp\":0,\"powerAverage\":0,\"powerIndex\":0,\"watthour\":0,\"Mminutes\":0,\"Mseconds\":0,\"waveBestTime\":0,\"wave\":0,\"takeOff\":0,\"rssiAvgSes\":0,\"lossPercent\":0,\"score\":0,\"vesc\":0,\"C1\":0,\"C2\":0,\"C3\":0,\"C4\":0}");
      file.close();
}

//UCCIO: sostituito ArduinoJson con string building manuale
void rideToJson(){
      snprintf(jsonFileName, sizeof(jsonFileName), "/ride%d.json", fileName);
    // Open file for writing
      File file = LittleFS.open(jsonFileName, FILE_WRITE);
      if (!file) {
          return;
      }
      String doc = "{";
      doc.reserve(384);
      doc += "\"Tminutes\":" + String(Tminutes) + ",";
      doc += "\"Tseconds\":" + String(Tseconds) + ",";
      doc += "\"maxPower\":" + String(maxPower) + ",";
      doc += "\"minVoltage\":" + String(minVoltage, 1) + ",";
      doc += "\"maxCurrent\":" + String(maxCurrent, 1) + ",";
      doc += "\"maxTemp\":" + String(maxTemp, 1) + ",";
      doc += "\"powerAverage\":" + String(powerAverage) + ",";
      doc += "\"powerIndex\":" + String(powerIndex) + ",";
      doc += "\"watthour\":" + String(watthour, 1) + ",";
      doc += "\"Mminutes\":" + String(Mminutes) + ",";
      doc += "\"Mseconds\":" + String(Mseconds) + ",";
      doc += "\"waveBestTime\":" + String(waveBestTime) + ",";
      doc += "\"wave\":" + String(wave) + ",";
      doc += "\"takeOff\":" + String(takeOff) + ",";
      doc += "\"rssiAvgSes\":" + String(rssiAvg, 2) + ",";
      doc += "\"lossPercent\":" + String(lossPercent, 1) + ",";
      doc += "\"score\":" + String(score) + ",";
      doc += "\"vesc\":" + String(vescError) + ",";
      doc += "\"C1\":" + String(C1) + ",";
      doc += "\"C2\":" + String(C2) + ",";
      doc += "\"C3\":" + String(C3) + ",";
      doc += "\"C4\":" + String(C4);
      doc += "}";
      
      file.print(doc);
      file.close();
}



//UCCIO: sostituito ArduinoJson con string building manuale
String dataToJson(){
      uint SDthrottle = map(throttle, 0, 180, 0, 99); 
      uint8_t channel;
      wifi_second_chan_t second;
      esp_wifi_get_channel(&channel, &second);
      String output = "{";
      output.reserve(320);
      output += "\"ride\":" + String(fileName) + ",";
      output += "\"voltage\":" + String(voltage, 1) + ",";
      output += "\"current\":" + String(current, 1) + ",";
      output += "\"tempMosfet\":" + String(tempMosfet, 1) + ",";
      output += "\"power\":" + String(power) + ",";
      output += "\"batpercentage\":" + String(batpercentage) + ",";
      output += "\"roll\":" + String((int)round(roll)) + ",";
      output += "\"pitch\":" + String((int)round(pitch)) + ",";
      output += "\"tilt\":" + String((int)round(tilt)) + ",";
      output += "\"rssiAvgSes\":" + String(rssiAvg, 1) + ",";
      output += "\"lossPercent\":" + String(lossPercent, 1) + ",";
      output += "\"rssi\":" + String((int)rssiLast) + ",";
      output += "\"packets\":" + String(packetCount) + ",";
      output += "\"lost\":" + String(packetLost) + ",";
      output += "\"throttle\":" + String(SDthrottle) + ",";
      output += "\"channel\":" + String(channel) + ",";
      output += "\"vesc\":" + String(vescError);
      output += "}";
      return output;
}



