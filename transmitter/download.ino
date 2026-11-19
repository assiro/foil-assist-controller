// OTA upgrade module

void firmwareUpgrade(bool triggerBoard){
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

            // Il trigger alla board si invia SOLO se richiesto: aggiornare il
            // solo controller non deve mandare in setup la ricevente.
            if (triggerBoard) {
                  throttle = 200; // SETUP COMMAND by throttle
                  for (int i = 0; i <= 20; i++) {
                        esp_now_send(broadcastAddress, (uint8_t *) &throttle, sizeof(throttle));// Send data via ESP-NOW
                        delay(100);
                  }
                  throttle = 0;
                  delay(500);
            }

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


bool checkUpgrade() {
      Serial.println("Check for upgrade...");
      display.setPartialWindow(0, 0, 200, 200);
      display.firstPage();
      do {
            display.setTextColor(GxEPD_BLACK);
            display.setTextSize(2);
            display.setCursor(0, 130);
            display.print("Check Upgrade");
      } while (display.nextPage());

      WiFiClient client;
      HTTPClient http;
      http.setConnectTimeout(10000);
      http.setTimeout(5000);
      String serverPath = upgradeServer + "foilAssistUpgrade.php?T=C&V=" + VERSION;
      Serial.println(serverPath);
      if (!http.begin(client, serverPath)) {
            Serial.println("Unable to start upgrade check.");
            return false;
      }
      int httpResponseCode = http.GET();
      if (httpResponseCode != HTTP_CODE_OK) {
            Serial.printf("Upgrade check failed: HTTP %d\n", httpResponseCode);
            http.end();
            return false;
      }
      String payload = http.getString();
      payload.trim();
      http.end();

      if (payload != "true") {
            display.setPartialWindow(0, 0, 200, 200);
            display.firstPage();
            do {
                  display.setTextColor(GxEPD_BLACK);
                  display.setTextSize(2);
                  display.setCursor(0, 150);
                  display.println("NO NEW UPGRADE");
                  display.println("CLICK TRIGGER");
            } while (display.nextPage());
            return false;
      }

      display.setPartialWindow(0, 0, 200, 200);
      display.firstPage();
      do {
            display.setTextColor(GxEPD_BLACK);
            display.setTextSize(2);
            display.setCursor(0, 150);
            display.println("Upgrade found");
            display.println("PLEASE WAIT");
      } while (display.nextPage());
      bool updated = performOTA();
      if (!updated) {
            display.setPartialWindow(0, 0, 200, 200);
            display.firstPage();
            do {
                  display.fillScreen(GxEPD_WHITE);
                  display.setTextColor(GxEPD_BLACK);
                  display.setTextSize(3);
                  display.setCursor(35, 55);
                  display.println("UPDATE");
                  display.setCursor(35, 95);
                  display.println("FAILED");
            } while (display.nextPage());
            delay(3000);
      }
      return updated;
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
              unsigned int percent = (total > 0) ? (unsigned int)(((uint64_t)progress * 100U) / total) : 0;
              if (percent > 100) percent = 100;
              display.print(percent);
              display.print("%");
              display.fillRect(10, 150, (percent * 182U) / 100U, 40, GxEPD_BLACK);
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
      client.setTimeout(5000);
      HTTPClient http;
      http.setConnectTimeout(15000);
      http.setTimeout(5000);
      String firmwareUrl = upgradeServer + upgradeFile;
      if (!http.begin(client, firmwareUrl)) {
            Serial.println("[OTA] Unable to start HTTP request.");
            return false;
      }
      int httpCode = http.GET();
      if (httpCode != HTTP_CODE_OK) {
            Serial.printf("[OTA] HTTP error %d: %s\n", httpCode, http.errorToString(httpCode).c_str());
            http.end();
            return false;
      }

      int contentLength = http.getSize();
      if (contentLength <= 0) {
            Serial.printf("[OTA] Invalid firmware size: %d\n", contentLength);
            http.end();
            return false;
      }
      if (!Update.begin((size_t)contentLength)) {
            Serial.printf("[OTA] Update initialization failed: %s\n", Update.errorString());
            http.end();
            return false;
      }

      WiFiClient *stream = http.getStreamPtr();
      uint8_t buff[512];
      size_t written = 0;
      unsigned long lastDataAt = millis();
      unsigned long lastProgressAt = 0;
      const unsigned long idleTimeoutMs = 15000;
      Serial.printf("[OTA] Downloading %d bytes...\n", contentLength);

      while (written < (size_t)contentLength) {
            int available = stream->available();
            if (available > 0) {
                  size_t toRead = (size_t)available;
                  if (toRead > sizeof(buff)) toRead = sizeof(buff);
                  size_t remaining = (size_t)contentLength - written;
                  if (toRead > remaining) toRead = remaining;
                  size_t received = stream->readBytes(buff, toRead);
                  if (received == 0) {
                        if (millis() - lastDataAt >= idleTimeoutMs) break;
                        delay(10);
                        continue;
                  }
                  size_t stored = Update.write(buff, received);
                  if (stored != received) {
                        Serial.println("[OTA] Flash write failed.");
                        Update.abort();
                        http.end();
                        return false;
                  }
                  written += stored;
                  lastDataAt = millis();
            } else {
                  if (!http.connected() || millis() - lastDataAt >= idleTimeoutMs) break;
                  delay(10);
            }

            if (millis() - lastProgressAt >= 3000) {
                  int percent = (int)(((uint64_t)written * 100U) / (uint64_t)contentLength);
                  Serial.printf("[OTA] Progress: %d%%\n", percent);
                  update_progress(percent);
                  lastProgressAt = millis();
            }
      }

      if (written != (size_t)contentLength) {
 //           Serial.printf("[OTA] Incomplete download: %u/%d bytes.\n", (unsigned)written, contentLength);
            Update.abort();
            http.end();
            return false;
      }

      http.end();
      if (!Update.end() || !Update.isFinished()) {
            Serial.printf("[OTA] Finalization failed: %s\n", Update.errorString());
            Update.abort();
            return false;
      }
      Serial.println("[OTA] Upgrade done; rebooting.");
      delay(1500);
      ESP.restart();
      return true;
}


// ============================================================
// OTA LOCALE — SOTTOMENU FIRMWARE
// ============================================================

void boardUpdateTrigger(){
      check_trigger_off();
      display.setPartialWindow(0, 0, 200, 200);
      display.firstPage();
      do {
            display.setTextColor(GxEPD_BLACK);
            display.setTextSize(3);
            display.setCursor(25, 25);
            display.println("BOARD");
            display.setCursor(25, 60);
            display.println("UPDATE");
            display.setTextSize(2);
            display.setCursor(10, 130);
            display.println("sending trigger");
      } while (display.nextPage());

      // Livello 200 sul canale throttle: la ricevente lo riconosce come
      // "predisponiti per l'aggiornamento" e riavvia in setup mode.
      throttle = 200;
      for (int i = 0; i < 20; i++) {
            esp_now_send(broadcastAddress, (uint8_t *) &throttle, sizeof(throttle));
            delay(100);
      }
      // Fondamentale: senza questo il controller continuerebbe a comandare
      // 200 in ogni pacchetto e la ricevente resterebbe in setup.
      throttle = 0;
      delay(300);

      display.setPartialWindow(0, 0, 200, 200);
      display.firstPage();
      do {
            display.setTextColor(GxEPD_BLACK);
            display.setTextSize(3);
            display.setCursor(20, 30);
            display.println("BOARD");
            display.setCursor(20, 70);
            display.println("ARMED");
            display.setTextSize(2);
            display.setCursor(10, 145);
            display.println("release = exit");
      } while (display.nextPage());
      vibration_confirm(400);
      delay(2000);
}

// Aggiornamento del SOLO controller: nessun comando alla board.
void firmwareControllerMenu(){
      check_trigger_off();
      const int items = 2;
      const char* opts[items] = {
            "Update by Server",
            "Update by OTAIDE"
      };
      int sel = 0;
      bool exit = false;
      while(!exit){
            display.setPartialWindow(0, 0, 200, 200);
            display.firstPage();
            do {
                  display.setTextColor(GxEPD_BLACK);
                  display.setTextSize(2);
                  display.setCursor(0, 0);
                  display.println("CONTROLLER");
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
            case 0: firmwareUpgrade(false); break;   // niente trigger alla board
            case 1: otaIDEmode(); break;
      }
}

void firmwareMenu(){
      check_trigger_off();
      const int items = 3;
      const char* opts[items] = {
            "Controller",
            "Board",
            "Back"
      };
      int sel = 0;
      bool exit = false;
      while(!exit){
            display.setPartialWindow(0, 0, 200, 200);
            display.firstPage();
            do {
                  display.setTextColor(GxEPD_BLACK);
                  display.setTextSize(3);
                  display.setCursor(0, 0);
                  display.println("FIRMWARE");
                  for(int i=0; i<items; i++){
                        int y = 45 + i*50;
                        if(i==sel){
                              display.fillRect(0, y-3, 200, 44, GxEPD_BLACK);
                              display.setTextColor(GxEPD_WHITE);
                        } else {
                              display.setTextColor(GxEPD_BLACK);
                        }
                        display.setTextSize(3);
                        display.setCursor(5, y+6);
                        display.print(opts[i]);
                  }
            } while(display.nextPage());
            int t = check_trigger();
            if(t==1){
                  sel = (sel+1)%items;
                  vibration_confirm(30);
            } else if(t==2){
                  vibration_confirm(200);
                  if(sel==0){ firmwareControllerMenu(); }
                  else if(sel==1){ boardUpdateTrigger(); }
                  else { exit = true; }
            }
      }
}

// ============================================================
// OTA IDE — ArduinoOTA puro
// ============================================================
void otaIDEmode(){
      check_trigger_off();
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
 //           display.setCursor(5,95);
            display.print(ipStr);
            display.setTextSize(2);
            display.setCursor(5,150);
            display.print("Timeout: ");
            display.print("5 min");
      } while(display.nextPage());
      
      // Attendi 5 minuti
      unsigned long timeout = millis() + 300000;
      while(millis() < timeout){
            ArduinoOTA.handle();
            delay(10);
            // Se arriva OTA, handle() fa il lavoro e restart
      }

}





