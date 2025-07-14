void pairing() {
//    const int firstByte;    
//    const int secondByte;   
    bool pairing = false;

    const int timeoutLimit = 6; 
    const int delayInterval = 200; 
    bool exit = false;
    int timeCounter = 0;
    while (!exit) {
        display.clearDisplay();
        display.setTextSize(2);
        display.setCursor(0,0);
        display.println("REICEVER");
        display.setCursor(0,30);
        display.print("PARING?");                  
        display.setCursor(0,60);
        display.print("Select ");
        if(pairing){
            display.print("YES");
        }else{
            display.print("NO");
        }
        display.display();

        hallValue = analogRead(hallPin);
        if (hallValue > valMiddle) {
            delay(delayInterval);
            while (hallValue > valMiddle) {
                hallValue = analogRead(hallPin);
                timeCounter++;
                if (timeCounter >= timeoutLimit) {
                    exit = true;
                    digitalWrite(VIBRATOR, HIGH);
                    digitalWrite(LED_BUILTIN,1);
                    delay(300);
                    digitalWrite(VIBRATOR, LOW);
                    digitalWrite(LED_BUILTIN,0);
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
    if(pairing){
        display.setTextSize(2);
        display.setCursor(0,90);
        display.println("START...");
        display.display();
        scanWiFi();
    }


}


void scanWiFi() {
        WiFi.disconnect();  
        delay(500);  
 
 /*       Serial.println("WiFi scanning...");
        int n = WiFi.scanNetworks();
        
        if (n == 0) {
            Serial.println("No networks found");
        } else {
            Serial.println("WiFi networks found:");
            for (int i = 0; i < n; ++i) {
            Serial.print(i + 1);
            Serial.print(": ");
            Serial.print(WiFi.SSID(i));
            if(WiFi.SSD(i)) ==
            Serial.print(" (RSSI: ");
            Serial.print(WiFi.RSSI(i));
            Serial.print(" dBm)");
            delay(10); 
            }
        }

*/
    Serial.println("Scanning WiFi...");
    int n = WiFi.scanNetworks();

    if (n == 0) {
        Serial.println("No network found.");
        noBoard();
        return;
    }

    // Find the strongest "TecnoFly " network
    String bestSSID = "";
    int bestRSSI = -1000;  // Initialize to a very low RSSI
    for (int i = 0; i < n; ++i) {
        String ssid = WiFi.SSID(i);
        int rssi = WiFi.RSSI(i);
        Serial.print("Wifi ");
        Serial.print(ssid);
        Serial.print(": ");
        Serial.println(rssi);
        Serial.println(" dBm");
        if (ssid.startsWith("TecnoFly ")) {
            if (rssi > bestRSSI) {
                bestRSSI = rssi;
                bestSSID = ssid;
            }
        }
    }
    if (bestSSID.length() == 0) {
        Serial.println("No matching SSID found.");
        noBoard();
        return;
    }

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
    display.setCursor(0, 110);
    display.print(codeStr);
    display.print(" ..OK");
    vibration_confirm();
    vibration_confirm();
    display.display();

    delay(5000);
    ESP.restart();
}



void noBoard(){
    display.setCursor(0, 110);
    display.println("NO BOARD");
    display.display();
    delay(5000);
}
