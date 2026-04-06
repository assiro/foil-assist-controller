void setupMode(){ // UPGRADE FIRMWARE AND WEBSERVER 
    serviceMode = true;
    Serial.println("SETUP MODE");
    WiFi.mode(WIFI_STA); 
    delay(500);
    WiFi.begin(ssidService, passwordService);
  // Wait for connection
  int count = 0;
  Serial.println("Waiting for Wi-Fi connection");
  while ( count < 40 ) {
      if (WiFi.status() == WL_CONNECTED) {
          Serial.print("Connected to ");
          Serial.println(ssidService);
          Serial.print("IP address: ");
          Serial.println(WiFi.localIP());
          Serial.println("Check for upgrade on Web Server...");
          String payload;
          WiFiClient client;
          HTTPClient http;
          String serverPath = upgradeServer + "foilAssistUpgrade.php?T=R&V=" + VERSION;
          Serial.println(serverPath);
          http.begin(client, serverPath.c_str());
          int httpResponseCode = http.GET();
          if (httpResponseCode>0) {
            Serial.print("HTTP Response code: ");
            Serial.println(httpResponseCode);
            payload = http.getString();
//            Serial.println(payload);
          }
          else {
            Serial.print("Error code: ");
            Serial.println(httpResponseCode);
          }
          http.end();
          Serial.println("State setup OFF saved in flash memory");  
          EEPROM.write(4, 0);
          EEPROM.commit();
          if(payload == "true"){
                  downloadFile(); // upgrade files in little fs              
                  Serial.println("Downloading firmware...");     
                  t_httpUpdate_return ret = ESPhttpUpdate.update(upgradeServer + upgradeFile);
                  switch(ret) {
                      case HTTP_UPDATE_FAILED: Serial.printf("HTTP_UPDATE_FAILD Error (%d): %s", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());break;
                      case HTTP_UPDATE_NO_UPDATES: Serial.println("HTTP_UPDATE_NO_UPDATES");break;
                      case HTTP_UPDATE_OK: Serial.println("HTTP_UPDATE_OK");break;
                  } 
          }else{        
          Serial.println("No Upgrade available");
          } 
          Serial.println("Reboot...");
          ESP.restart();
      }
  delay(500);
  Serial.print(".");
  count++;
  }
  Serial.println("Setup OFF on flash memory");  
  EEPROM.write(4, 0);
  EEPROM.commit();  
  ESP.restart();  
}



void downloadFile() {
    Serial.println("Download files...");

    const char *filenames[] = {
      "/index.html", "/setup.html", "/test.html", "/ride.html",
      "/assist.css", "/favicon.ico", "/bootstrap.min.js", "/rideAnalysis.js",
      "/jquery.js", "/message.txt", "/expert.html",
      "/tecnofly_logo.gif", "/uplot.iife.min.js", "/uplot.min.css"
    };

    const char *serverBase = "https://www.eoloonline.it/firmware/foilAssist/webServer";

    HTTPClient http;

    for (int i = 0; i < sizeof(filenames) / sizeof(filenames[0]); i++) {
      String url = String(serverBase) + String(filenames[i]);
      Serial.print("Downloading: ");
      Serial.print(String(filenames[i]));

      http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
      http.setUserAgent("ESP32Downloader/1.0");
      http.setTimeout(20000);  // 20 secondi

      http.begin(url);

      int httpCode = http.GET();

      if (httpCode == HTTP_CODE_OK) {
        int len = http.getSize();
        WiFiClient *stream = http.getStreamPtr();

        File file = LittleFS.open(filenames[i], FILE_WRITE);
        if (!file) {
          Serial.println(" -> Error opening file!");
          http.end();
          continue;
        }

        uint8_t buff[256];  // buffer più grande
        int total = 0;
        unsigned long lastTime = millis();
        while (http.connected() && (len > 0 || len == -1)) {
          size_t size = stream->available();
          if (size) {
            int c = stream->readBytes(buff, min(size, sizeof(buff)));
            file.write(buff, c);
            total += c;
            if (len > 0) len -= c;
            lastTime = millis();  // reset timeout
          }

          if (millis() - lastTime > 15000) {
            Serial.println(" -> Timeout durante ricezione!");
            break;
          }

          delay(1);
        }
        file.close();
        Serial.printf(" -> Saved OK! (%d byte)\n", total);

      } else {
        Serial.printf(" -> Error HTTP: %d\n", httpCode);
      }

      http.end();
    }

}


