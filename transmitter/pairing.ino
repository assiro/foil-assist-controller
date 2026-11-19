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

//    Serial.println("Initializing WiFi for scanning...");
    WiFi.mode(WIFI_STA);
    WiFi.disconnect(true);
    delay(1000);
    int n = WiFi.scanNetworks();
//    Serial.println("Scanning WiFi...");
    if (n <= 0) {
        WiFi.scanDelete();
        noBoard();
        return;
    }

    String bestSSID;
    uint16_t bestBoardCode = 0;
    int bestRSSI = -1000;
    const char prefix[] = "TecnoFly ";
    for (int i = 0; i < n; ++i) {
        String ssid = WiFi.SSID(i);
        int rssi = WiFi.RSSI(i);
        Serial.printf("WiFi %s: %d dBm\n", ssid.c_str(), rssi);
        if (!ssid.startsWith(prefix)) continue;

        String codeStr = ssid.substring(sizeof(prefix) - 1);
        if (codeStr.length() == 0) continue;
        uint32_t parsed = 0;
        bool valid = true;
        for (size_t j = 0; j < codeStr.length(); ++j) {
            char c = codeStr.charAt(j);
            if (c < '0' || c > '9') {
                valid = false;
                break;
            }
            parsed = parsed * 10U + (uint32_t)(c - '0');
            if (parsed > 65535U) {
                valid = false;
                break;
            }
        }
        if (!valid) {
            Serial.printf("Ignoring invalid board code in SSID: %s\n", ssid.c_str());
            continue;
        }
        if (rssi > bestRSSI) {
            bestRSSI = rssi;
            bestSSID = ssid;
            bestBoardCode = (uint16_t)parsed;
        }
    }
    WiFi.scanDelete();

    if (bestSSID.length() == 0) {
        noBoard();
        return;
    }

    String codeStr = bestSSID.substring(sizeof(prefix) - 1);
    Serial.printf("Valid board code: %s (0x%04X)\n", codeStr.c_str(), bestBoardCode);
    EEPROM.write(13, bestBoardCode & 0xFF);
    EEPROM.write(14, (bestBoardCode >> 8) & 0xFF);
    if (!EEPROM.commit()) {
        Serial.println("EEPROM commit failed; pairing was not saved.");
        display.setPartialWindow(0, 0, 200, 200);
        display.firstPage();
        do {
            display.fillScreen(GxEPD_WHITE);
            display.setTextSize(3);
            display.setCursor(10, 35);
            display.println("PAIR SAVE");
            display.setCursor(35, 75);
            display.println("FAILED");
        } while (display.nextPage());
        vibration_confirm(500);
        delay(3000);
        return;
    }

    display.setPartialWindow(0, 0, 200, 200);
    display.firstPage();
    do {
        display.fillScreen(GxEPD_WHITE);
        display.setTextSize(3);
        display.setCursor(0, 60);
        display.println("FOUND BOARD");
        display.setCursor(45, 90);
        display.print(codeStr);
        display.setCursor(35, 130);
        display.print("OK done!");
    } while (display.nextPage());
    vibration_confirm(300);
    delay(3000);
    ESP.restart();
}


void noBoard(){
    display.setPartialWindow(0, 0, 200, 200);
    display.firstPage();
    do {
        display.fillScreen(GxEPD_WHITE);
        display.setTextSize(4);
        display.setCursor(0, 70);
        display.println("NO BOARD");
        display.println("FOUND");
    } while (display.nextPage());
    delay(5000);
}
