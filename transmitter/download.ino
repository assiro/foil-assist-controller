// OTA upgrade module

void firmwareUpgrade(){
            check_trigger_off();
            display.setPartialWindow(0, 0, 200, 200); 
            display.firstPage();
            do{
                  display.setTextColor(GxEPD_BLACK);
                  display.setTextSize(3);
                  display.setCursor(30,0);
                  display.println("Firmware");
                  display.setCursor(35,30);      
                  display.println("Upgrade"); 
            } while (display.nextPage());

            throttle = 200; // SETUP COMMAND by throttle
            for (int i = 0; i <= 20; i++) {      
                  esp_now_send(broadcastAddress, (uint8_t *) &throttle, sizeof(throttle));// Send data via ESP-NOW
                  delay(100);
            }
            delay(500);

            esp_now_deinit();
            delay(50);
            WiFi.disconnect(true, true);
            delay(100);
            WiFi.mode(WIFI_OFF);
            delay(100);
            WiFi.mode(WIFI_STA);
            delay(100);
            WiFi.setTxPower(WIFI_POWER_8_5dBm);

            WiFi.begin(ssidService, passwordService);
            unsigned long start = millis();
            while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
                  delay(200);
                  yield();
            }
            hallValue = 0;
            bool otaStarted = false;
            unsigned long wifiWaitStart = millis();
            while (hallValue < valMiddle){  // throttle off 
                  delay(100);
                  hallValue = analogRead(hallPin);
                  if(WiFi.status() == WL_CONNECTED) {
                              if (!otaStarted) {
                                    OTAstart();
                                    otaStarted = true;
                              }
                              ArduinoOTA.handle();
                              display.setPartialWindow(0, 0, 200, 200);
                              display.firstPage();
                              do{
                                    display.setTextSize(2);
                                    display.setCursor(0,110);
                                    display.print("Connected!");
                              }while (display.nextPage());    
                              uint attempts = 0;
                              while(attempts<5){
                                    checkUpgrade();
                                    attempts ++;
                                    delay(2000);
                              }
                              Serial.println("finish");
                              check_trigger();
                              
                  }else if(millis() - wifiWaitStart < 30000){
                              display.setPartialWindow(0, 0, 200, 200);
                              display.firstPage();
                              do{ 
                                    display.setTextSize(2);
                                    display.setCursor(0,90);
                                    display.print("Searching WiFi");
                                    display.setCursor(0,120);
                                    display.print((millis() - wifiWaitStart)/1000);
                                    display.print("s");
                              }while (display.nextPage());
                  }else{
                              // WiFi timeout after 30s — exit
                              display.setPartialWindow(0, 0, 200, 200);
                              display.firstPage();
                              do{
                                    display.setTextSize(2);
                                    display.setCursor(20,70);
                                    display.println("WiFi FAIL!");
                                    display.setCursor(5,100);
                                    display.println("Press trigger");
                              }while(display.nextPage());
                              check_trigger();
                              break;
                  }
            }
            check_trigger_off();
}


boolean checkUpgrade(){
      Serial.println("Check for upgrade...");
      delay(400);
      display.setPartialWindow(0, 0, 200, 200);
      display.firstPage();
      do{
            display.setTextColor(GxEPD_BLACK);
            display.setTextSize(2);
            display.setCursor(0,130);
            display.print("Check Upgrade");
      }while (display.nextPage());
      String payload;
      WiFiClient client;
      HTTPClient http;
      String serverPath = upgradeServer + "foilAssistUpgrade.php?T=C&V=" + VERSION;
      Serial.println(serverPath);

      http.begin(client, serverPath.c_str());
      int httpResponseCode = http.GET();
      if (httpResponseCode>0) {
            Serial.print("HTTP Response code: ");
            Serial.println(httpResponseCode);
            payload = http.getString();
            Serial.println(payload);
      }
      else {
            Serial.print("Error code: ");
            Serial.println(httpResponseCode);
      }
      http.end();
      if(payload == "true"){
            display.setPartialWindow(0, 0, 200, 200);
            display.firstPage();
            do{
                  display.setTextColor(GxEPD_BLACK);
                  display.setTextSize(2);
                  display.setCursor(0,150);
                  display.println("Upgrade found");
                  display.println("PLEASE WAIT");
            }while(display.nextPage());
            performOTA();

      }else{
            display.setPartialWindow(0, 0, 200, 200);
            display.firstPage();
            do{
                  display.setTextColor(GxEPD_BLACK);
                  display.setTextSize(2);
                  display.setCursor(0,150);
                  display.println("NO NEW UPGRADE");
                  display.println("CLICK TRIGGER");
            }while(display.nextPage());
            return false;
      }
}


void update_progress(int percent) {
      display.setPartialWindow(0, 0, 200, 200); 
      display.firstPage();
      do {
          display.setTextColor(GxEPD_BLACK);
          display.setTextSize(3);
          display.setCursor(30,0);
          display.println("Firmware");
          display.setCursor(35,30);      
          display.println("Upgrade"); 
          display.setCursor(11,70);  
          display.println(" Download");        
          display.setTextSize(3);
          display.setCursor(75,115);
          display.print(percent);
          display.print("%");
          int barWidth = map((int)percent, 0, 100, 0, 180);
          display.drawRect(10, 150, 182, 40, GxEPD_BLACK);
          display.fillRect(10, 150, barWidth, 40, GxEPD_BLACK);
      } while (display.nextPage());
}

void OTAstart(){
      ArduinoOTA.setHostname("Tecnofly-controller");
      ArduinoOTA.onStart([]() {Serial.println("OTA Started");});
      ArduinoOTA.onEnd([]() {
          Serial.println("\nEnd");
      });
      ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
          do {
              display.setPartialWindow(0, 0, 200, 200);
              display.firstPage();
              display.setTextSize(3);
              display.setCursor(5,10);
              display.println("On The Air");
              display.setCursor(30,45);
              display.println("Upgrade");
              display.drawRect(10, 150, 182, 40, GxEPD_BLACK);
              display.setTextSize(3);
              display.setCursor(75,100);
              display.print(progress / (total / 100));
              display.print("%");
              display.fillRect(10, 150, (progress * 182) / total, 40, GxEPD_BLACK);
          } while (display.nextPage());
      });
      ArduinoOTA.onError([](ota_error_t error) {
        Serial.printf("Error[%u]: ", error);
        if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
        else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
        else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
        else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
        else if (error == OTA_END_ERROR) Serial.println("End Failed");
      });
      ArduinoOTA.begin();
      Serial.println("OTA Ready");
      Serial.print("IP address: ");
      Serial.println(WiFi.localIP());
}

bool performOTA() {
      WiFiClient client;
      HTTPClient http;
      http.setConnectTimeout(15000);
      http.begin(client, upgradeServer + upgradeFile);
      int httpCode = http.GET();
      if (httpCode != HTTP_CODE_OK) {
            Serial.printf("[OTA] Errore HTTP: %d -> %s\n", httpCode, http.errorToString(httpCode).c_str());
            http.end();
            return false;
      }

      int contentLength = http.getSize();
      if (contentLength <= 0) {
            Serial.println("[OTA] Errore: dimensione file non valida.");
            http.end();
            return false;
      }

      if (!Update.begin(contentLength)) {
            Serial.println("[OTA] Error update initialization");
            http.end();
            return false;
      }

      WiFiClient *stream = http.getStreamPtr();
      uint8_t buff[512];
      size_t written = 0;
      unsigned long lastProgressPrint = millis();

      Serial.println("[OTA] Download is running...");

      while (http.connected() && (written < (size_t)contentLength || contentLength == -1)) {
      size_t available = stream->available();
      if (available) {
            int len = stream->readBytes(buff, ((available > sizeof(buff)) ? sizeof(buff) : available));
            size_t writtenNow = Update.write(buff, len);
            if (writtenNow != len) {
                Serial.println("[OTA] Write error!");
                Update.abort();
                http.end();
                return false;
            }
            written += writtenNow;
      }

      if (millis() - lastProgressPrint >= 3000) {
            float progress = (float)written / (float)contentLength * 100.0;
            Serial.printf("[OTA] Status: %.1f%% \n", progress);
            lastProgressPrint = millis();
            update_progress(progress);
      }

      delay(10);
      }

      if (Update.end()) {
      if (Update.isFinished()) {
            Serial.println("\n[OTA] Upgrade done... reboot");
            http.end();
            delay(2000);
            ESP.restart();
            return true;
      } else {
            Serial.println("[OTA] Error: Upgrade not complete.");
            http.end();
            return false;
      }
      } else {
      Serial.printf("[OTA] Error Update.end(): %s\n", Update.errorString());
      http.end();
      return false;
      }
}


// ============================================================
// OTA LOCALE — SOTTOMENU FIRMWARE
// ============================================================

void firmwareMenu(){
      check_trigger_off();
      const int items = 2;
      const char* opts[items] = {
            "Update by OTAIDE",
            "Update by Server"
      };
      int sel = 0;
      bool exit = false;
      while(!exit){
            display.setPartialWindow(0, 0, 200, 200);
            display.firstPage();
            do {
                  display.setTextSize(3);
                  display.setCursor(0, 0);
                  display.println("SELECT TYPE");
                  for(int i=0; i<items; i++){
                        int y = 50 + i*60;
                        if(i==sel){
                              display.fillRect(0, y-3, 200, 50, GxEPD_BLACK);
                              display.setTextColor(GxEPD_WHITE);
                        } else {
                              display.setTextColor(GxEPD_BLACK);
                        }
                        display.setTextSize(2);
                        display.setCursor(5, y+8);
                        display.print(opts[i]);
                  }
            } while(display.nextPage());
            int t = check_trigger();
            if(t==1){
                  sel = (sel+1)%items;
                  vibration_confirm(30);
            } else if(t==2){
                  vibration_confirm(200);
                  exit = true;
            }
      }
      
      switch(sel){
            case 0: otaIDEmode(); break;
            case 1: firmwareUpgrade(); break;
      }
}


// ============================================================
// OTA IDE — ArduinoOTA puro
// ============================================================
void otaIDEmode(){
      check_trigger_off();
      
      // Connetti a WiFi
      display.setPartialWindow(0, 0, 200, 200);
      display.firstPage();
      do{
            display.setTextSize(2);
            display.setCursor(20,20);
            display.println("Connecting");
            display.setCursor(25,45);
            display.println("to Wi-Fi...");
      } while(display.nextPage());
      
      WiFi.disconnect(true, true);
      delay(100);
      WiFi.mode(WIFI_OFF);
      delay(100);
      WiFi.mode(WIFI_STA);
      delay(100);
      WiFi.setTxPower(WIFI_POWER_8_5dBm);
      WiFi.begin(ssidService, passwordService);
      
      unsigned long start = millis();
      while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
            delay(200);
            yield();
      }
      
      if(WiFi.status() != WL_CONNECTED){
            do{
                  display.setCursor(20,80);
                  display.println("WiFi FAIL!");
            } while(display.nextPage());
            delay(2000);
            return;
      }
      
      // Mostra IP
      char ipStr[16];
      sprintf(ipStr, "%d.%d.%d.%d", WiFi.localIP()[0], WiFi.localIP()[1], WiFi.localIP()[2], WiFi.localIP()[3]);
      
      // Hostname univoco con boardCode
      char hostname[32];
      sprintf(hostname, "Tecnofly-TX-%04X", boardCode);
      ArduinoOTA.setHostname(hostname);
      
      ArduinoOTA.onStart([]() {Serial.println("OTA Started");});
      ArduinoOTA.onEnd([]() {Serial.println("\nOTA End");});
      ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
            int pct = progress * 100 / total;
            display.setPartialWindow(0, 0, 200, 200);
            display.firstPage();
            do{
                  display.setTextSize(2);
                  display.setCursor(5,10);
                  display.println("OTA IDE");
                  display.drawRect(10, 150, 182, 40, GxEPD_BLACK);
                  display.setTextSize(3);
                  display.setCursor(75,95);
                  display.print(pct);
                  display.print("%");
                  display.fillRect(10, 150, (pct*182)/100, 40, GxEPD_BLACK);
            } while(display.nextPage());
      });
      ArduinoOTA.onError([](ota_error_t error) {Serial.printf("OTA Error: %u\n", error);});
      ArduinoOTA.begin();
      
      // Schermata attesa
      display.setPartialWindow(0, 0, 200, 200);
      display.firstPage();
      do{
            display.setTextSize(2);
            display.setCursor(5,5);
            display.println("OTA IDE READY");
            display.setCursor(5,30);
            display.println("Hostname:");
            display.setCursor(5,50);
            display.print(hostname);
            display.setCursor(5,75);
            display.println("IP:");
            display.setCursor(5,95);
            display.print(ipStr);
            display.setTextSize(2);
            display.setCursor(5,150);
            display.print("Timeout: ");
            display.print("120s");
      } while(display.nextPage());
      
      // Attendi 120 secondi
      unsigned long timeout = millis() + 120000;
      while(millis() < timeout){
            ArduinoOTA.handle();
            delay(10);
            // Se arriva OTA, handle() fa il lavoro e restart
      }
      
      display.setPartialWindow(0, 0, 200, 200);
      display.firstPage();
      do{
            display.setTextSize(2);
            display.setCursor(20,70);
            display.println("TIMEOUT");
            display.setCursor(5,100);
            display.println("No upload");
      } while(display.nextPage());
      delay(2000);
}





