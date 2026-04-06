void pairing() {
    bool pairing = false;
    const int timeoutLimit = 6; 
    const int delayInterval = 200; 
    bool exit = false;
    int timeCounter = 0;
    while (!exit) {
        display.setPartialWindow(0, 0, 200, 200);
        display.firstPage();
        display.setTextSize(3);
        display.setCursor(28, 0);
        display.println("RECEIVER");
        display.setCursor(40, 35);
        display.println("PAIRING");
        display.setTextSize(2);
        display.setCursor(20, 160);
        display.println("Board firmware");
        display.setCursor(40, 180);
        display.print("Ver. ");
        display.print(receiverVersion);
        display.setTextSize(3);
        display.setCursor(45, 65);
        display.print("Select");
        display.setTextSize(6);
        display.setCursor(55, 100);
        if(pairing){
            display.print("YES");
        }else{
            display.print("NO");
        }
        display.nextPage();
        hallValue = analogRead(hallPin);
        if (hallValue > valMiddle) {
            delay(delayInterval);
            while (hallValue > valMiddle) {
                hallValue = analogRead(hallPin);
                timeCounter++;
                if (timeCounter >= timeoutLimit) {
                    exit = true;
                    vibration_confirm(200);
                    break;
                }
                delay(delayInterval);
            }
            if (timeCounter < timeoutLimit) {
                  if(pairing){
                      pairing = false;
                  }else{
                      pairing = true;
                  }
            }
            timeCounter = 0;
        }
        delay(100);
    }
    if(pairing){scanWiFi();}
}


void scanWiFi() {
    display.setPartialWindow(0, 0, 200, 200);
    display.firstPage();
    display.setTextSize(3);
    display.setCursor(28, 0);
    display.println("RECEIVER");
    display.setCursor(40, 35);
    display.println("PAIRING");
    display.setCursor(55, 80);
    display.print("START");
    display.setTextSize(5);
    display.setCursor(80, 100);
    display.nextPage();

    Serial.println("Initializing WiFi for scanning...");
    WiFi.mode(WIFI_STA);        // ESP32 in modalità stazione
    WiFi.disconnect(true);      // Disconnessione forzata da eventuali reti
    delay(1000);                // Attendere che la disconnessione si completi
    int n = WiFi.scanNetworks();
    Serial.println("Scanning WiFi...");
    if (n == 0) {
        Serial.println("No network found.");
        noBoard();
    }else{
        Serial.println(String(n) + " networks found");
 
        // Find the strongest "TecnoFly " network
        String bestSSID = "";
        int bestRSSI = -1000;  // Initialize to a very low RSSI
        for (int i = 0; i < n; ++i) {
            String ssid = WiFi.SSID(i);
            int rssi = WiFi.RSSI(i);
            Serial.print("Wifi ");
            Serial.print(ssid);
            Serial.print(": ");
            Serial.print(rssi);
            Serial.println(" dBm");
            if (ssid.startsWith("TecnoFly ")) {
                if (rssi > bestRSSI) {
                    bestRSSI = rssi;
                    bestSSID = ssid;
                }
            }
        }
        if (bestSSID.length() == 0) {
            noBoard();
        }else{
            // Extract code substring (after "TecnoFly ")
            String codeStr = bestSSID.substring(9);
            Serial.print("Code found: ");
            Serial.println(codeStr);

            uint16_t numberValue = (uint16_t) codeStr.toInt();  
            Serial.print("HEX: 0x");
            Serial.println(numberValue, HEX);

            // Store in EEPROM (LSB at address 13, MSB at 14)
            EEPROM.write(13, numberValue & 0xFF);
            EEPROM.write(14, (numberValue >> 8) & 0xFF);
            EEPROM.commit();

        // Update display and restart
            display.setTextSize(3);
            display.setCursor(0, 120);
            display.println("FOUND BOARD:");
            display.setCursor(0, 150);
            display.print(codeStr);
            display.print(" ..OK");
            vibration_confirm(300);
            display.nextPage();
            delay(5000);
            ESP.restart();
        }
    }
}

void noBoard(){
    display.setTextSize(4);
    display.setCursor(0, 130);
    display.println("NO BOARD");
    display.println("FOUND");
    display.nextPage();
    delay(5000);
}

/*
void testScan(){
Serial.println("Initializing WiFi for scanning...");
  WiFi.mode(WIFI_STA);        // ESP32 in modalità stazione
  WiFi.disconnect(true);      // Disconnessione forzata da eventuali reti

  delay(1000);                // Attendere che la disconnessione si completi

  Serial.println("Scanning WiFi networks...");
  int nn = WiFi.scanNetworks();

  if (nn == 0) {
    Serial.println("No networks found");
  } else {
    Serial.println(String(nn) + " networks found");
    for (int i = 0; i < nn; ++i) {
      String ssid = WiFi.SSID(i);
      int rssi = WiFi.RSSI(i);
      Serial.print("SSID: ");
      Serial.print(ssid);
      Serial.print(" | RSSI: ");
      Serial.print(rssi);
      Serial.println(" dBm");
    }
  }
}
*/