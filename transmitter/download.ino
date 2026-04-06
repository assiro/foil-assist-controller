// OTA upgrade module

void firmwareUpgrade(){
            check_trigger_off();
            display.setPartialWindow(0, 0, 200, 200); 
            display.firstPage();
            do{
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
//            esp_wifi_set_ps(WIFI_PS_NONE); // power save OFF
//            delay(50);

/*
WIFI_POWER_19_5dBm   // 19.5 dBm 
WIFI_POWER_19dBm     // 19.0 dBm
WIFI_POWER_18_5dBm
WIFI_POWER_17dBm
WIFI_POWER_15dBm
WIFI_POWER_13dBm
WIFI_POWER_11dBm
WIFI_POWER_8_5dBm
WIFI_POWER_7dBm
WIFI_POWER_5dBm
WIFI_POWER_2dBm
WIFI_POWER_MINUS_1dBm
*/
            WiFi.setTxPower(WIFI_POWER_8_5dBm);

            WiFi.begin(ssidService, passwordService);
            unsigned long start = millis();
            while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
                  delay(200);
                  yield();
            }
            hallValue = 0;
            display.setTextSize(2);
            while (hallValue < valMiddle){  // throttle off 
                  delay(100);
                  hallValue = analogRead(hallPin);
                  static bool otaStarted = false;

                  if(WiFi.status() == WL_CONNECTED) {
      //                       if (!otaStarted) {
      //                             OTAstart();
      //                             otaStarted = true;
      //                       }
      //                       ArduinoOTA.handle();       
                              do{         
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
                              
                  }else{
                              do{ 
                                    display.setCursor(0,90);
                                    display.print("Searcing WiFi");                     
                              }while (display.nextPage());
                  }
            }
            check_trigger_off();
}


boolean checkUpgrade(){
//      uint attempt = 0;
      Serial.println("Check for upgrade...");
      delay(400);
      do{ 
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
            display.setCursor(0,150);
            display.println("Upgrade found");
            display.println("PLEASE WAIT");
            display.nextPage();
            performOTA();   

      }else{
            display.setCursor(0,150);
            display.println("NO NEW UPGRADE");
            display.println("CLICK TRIGGER");
            display.nextPage();     
            return false;    
      } 

}


void update_progress(int percent) {
      display.setPartialWindow(0, 0, 200, 200); 
      display.firstPage();
      do {
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
          int barWidth = map((int)percent, 0, 100, 0, 180);  // Barra larga 180px
          display.drawRect(10, 150, 182, 40, GxEPD_BLACK);   // Contorno barra
          display.fillRect(10, 150, barWidth, 40, GxEPD_BLACK);  // Parte piena
      } while (display.nextPage());
}

void OTAstart(){      //ARDUINO OTA
      ArduinoOTA.setHostname("Tecnofly-controller");
      ArduinoOTA.onStart([]() {Serial.println("OTA Started");});
      ArduinoOTA.onEnd([]() {
          Serial.println("\nEnd");
      });
      ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
          do {    
              display.setPartialWindow(0, 0, 200, 200); // Imposta la finestra da aggiornare
              display.firstPage();
              display.setTextSize(3);
              display.setCursor(5,10);
              display.println("On The Air");
              display.setCursor(30,45);      
              display.println("Upgrade"); 
          //    display.drawRect(12, 100, 102,20, WHITE);
              display.drawRect(10, 150, 182, 40, GxEPD_BLACK);
              display.setTextSize(3);
              display.setCursor(75,100);
              display.print(progress / (total / 100));
              display.print("%");
              for(int prog=0; prog<(progress / (total / 100));prog++){
    //                    display.drawRect(13, 100, prog,20, WHITE);    
                    display.fillRect(10, 150, prog, 40, GxEPD_BLACK);              
              }
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



void setChannel(uint8_t channel) {
  // forza il canale radio (richiede esp_wifi.h)
  esp_err_t err = esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
  Serial.printf("esp_wifi_set_channel(%d) => %d\n", channel, err);
}

// disabilita power-save per migliorare handshake
void disablePowerSave() {
  esp_err_t err = esp_wifi_set_ps(WIFI_PS_NONE);
  Serial.printf("esp_wifi_set_ps(NONE) => %d\n", err);
}

bool tryConnectWithParams(const uint8_t *bssid, int channel, unsigned long timeoutMs) {
  Serial.println("=== Tentativo di connessione mirata ===");
  if (channel > 0) {
    setChannel(channel);
    delay(50); // lascia stabilizzare il canale
  }
   disablePowerSave();

  // assicurati che non ci siano vecchie connessioni
  WiFi.disconnect(true);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(false); // non scrivere credenziali in flash inutilmente
  WiFi.setSleep(false);

  // se abbiamo BSSID, proviamo l'overload (se la API lo supporta)
  if (bssid != nullptr) {
    // WiFi.begin(ssid, password, channel, bssid) expects a pointer to 6-byte bssid
    Serial.println("Inizio WiFi.begin con BSSID e canale...");
    WiFi.begin(ssidService, passwordService, channel, bssid);
  } else {
    Serial.println("Inizio WiFi.begin normale...");
    WiFi.begin(ssidService, passwordService);
  }

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - start) < timeoutMs) {
    delay(200);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("Connesso! IP: %s RSSI: %d\n", WiFi.localIP().toString().c_str(), WiFi.RSSI());
    return true;
  } else {
    Serial.printf("Connessione FALLITA (status=%d)\n", WiFi.status());
    return false;
  }
}
