void setupMode(){ // UPGRADE FIRMWARE AND WEBSERVER 
    serviceMode = true;
    DBGLN("SETUP MODE");
    WiFi.mode(WIFI_STA); 
    delay(500);
    WiFi.begin(ssidService, passwordService);
  // Wait for connection
  int count = 0;
  DBGLN("Waiting for Wi-Fi connection");
  while ( count < 40 ) {
      if (WiFi.status() == WL_CONNECTED) {
          DBGPRINT("Connected to ");
          DBGLN(ssidService);
          DBGPRINT("IP address: ");
          DBGLN(WiFi.localIP());
          DBGLN("Check for upgrade on Web Server...");
          String payload;
          WiFiClient client;
          HTTPClient http;
          String serverPath = String(upgradeServer) + "foilAssistUpgrade.php?T=R&V=" + VERSION;
          DBGLN(serverPath);
          http.begin(client, serverPath.c_str());
          int httpResponseCode = http.GET();
          if (httpResponseCode>0) {
            DBGPRINT("HTTP Response code: ");
            DBGLN(httpResponseCode);
            payload = http.getString();
//            Serial.println(payload);
          }
          else {
            DBGPRINT("Error code: ");
            DBGLN(httpResponseCode);
          }
          http.end();
//          Serial.println("State setup OFF saved in flash memory");  
          EEPROM.write(4, 0);
          EEPROM.commit();
          if(payload == "true"){
                  downloadFile(); // upgrade files in little fs              
                  DBGLN("Firmware...");     
                  t_httpUpdate_return ret = ESPhttpUpdate.update(String(upgradeServer) + upgradeFile);
                  switch(ret) {
                      case HTTP_UPDATE_FAILED: DBG("HTTP_UPDATE_FAILD Error (%d): %s", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());break;
                      case HTTP_UPDATE_NO_UPDATES: DBGLN("HTTP_UPDATE_NO_UPDATES");break;
                      case HTTP_UPDATE_OK: DBGLN("HTTP_UPDATE_OK");break;
                  } 
          }else{        
          DBGLN("No available");
          } 
//          Serial.println("Reboot...");
          ESP.restart();
      }
  delay(500);
  DBGPRINT(".");
  count++;
  }
//  Serial.println("Setup OFF on flash memory");  
  EEPROM.write(4, 0);
  EEPROM.commit();  
  ESP.restart();  
}



void downloadFile() {
    const char *filenames[] = {
      "/index.html", "/setup.html", "/test.html", "/ride.html", "/rides.html",
      "/assist.css", "/favicon.ico", "/bootstrap.min.js", "/rideAnalysis.js",
      "/jquery.js", "/expert.html", "/dataviewer.html", "/tecnofly_logo.gif", 
      "/uplot.iife.min.js", "/uplot.min.css", "/fiber.jpg"
    };

    HTTPClient http;

    for (int i = 0; i < sizeof(filenames) / sizeof(filenames[0]); i++) {
      String url = String(serverBase) + String(filenames[i]);
//      Serial.print("Downloading: ");
      DBGPRINT(String(filenames[i]));
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
//          Serial.println(" -> Error opening file!");
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
//            Serial.println(" -> Timeout durante ricezione!");
            break;
          }
          delay(1);
        }
        file.close();
        DBG(" -> Saved OK! (%d byte)\n", total);
      }

      http.end();
    }

}


