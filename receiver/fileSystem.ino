void downloadFile(){
    Serial.println("Downloading files...");
    const char *filenames[] = {
      "/index.html",
      "/setup.html",
      "/test.html",
      "/lora.html",
      "/ride.html"
    };
    char serverUrl[] = "https://www.eoloonline.it/firmware/foilAssist/webServer";
    char URL[75];
    HTTPClient http;
    for (int i = 0; i < sizeof(filenames) / sizeof(filenames[0]); i++) {
          URL[0] = '\0';
          strcat(URL, serverUrl);
          strcat(URL, filenames[i]);       
          http.begin(URL);
          int httpCode = http.GET();
          if (httpCode > 0) {
                if (httpCode == HTTP_CODE_OK) {
                      Serial.print(filenames[i]);
                      file = SD.open(filenames[i], FILE_WRITE);
                      if (file) {
                        String payload = http.getString();
                        file.print(payload);
                        file.close();
                        Serial.println(" ...OK!");
                      } else {
                        Serial.println(" ... error!");
                      }
                } else {
                  Serial.printf("Errore HTTP: %d\n", httpCode);
                }
          } else {
            Serial.println("Impossible to connect ");
          }
          http.end();
    }
}

