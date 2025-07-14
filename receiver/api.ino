void api(){
	// Route for root / web page
      server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(SD, "/index.html", "text/html");
        Serial.println("Client Connected");
      });

      server.onNotFound([](AsyncWebServerRequest *request){
        request->send(SD, "/index.html", "text/html");
      });    
  
      server.on("/ver", HTTP_GET, [](AsyncWebServerRequest *request){
          request->send_P(200, "text/plain", VERSION.c_str());
      });

      server.on("/rx_number", HTTP_GET, [](AsyncWebServerRequest *request){
            uint8_t lsb = EEPROM.read(1);
            uint8_t msb = EEPROM.read(2);

            uint16_t rxNumber = (msb << 8) | lsb;
            Serial.print("EEPROM serial number: ");
            Serial.println(rxNumber);
            char serialNumber[5]; 
            itoa(rxNumber, serialNumber, 10);
            request->send_P(200, "", serialNumber);
      });

      server.on("/Wmessage", HTTP_GET, [](AsyncWebServerRequest *request){
            String wmessage1;
            String wmessage2;
            String wmessage3;        
            if (request->hasParam("input1")) {
              wmessage1 = request->getParam("input1")->value();  
            }    
            if (request->hasParam("input2")) {
              wmessage2 = request->getParam("input2")->value();
            }
            if (request->hasParam("input3")) {
              wmessage3 = request->getParam("input3")->value();
            }
            const char* path = "/message.txt";
            file = SD.open(path, FILE_WRITE);
            if (file){
                file.println(wmessage1);
                file.println(wmessage2);
                file.print(wmessage3);            
                file.close(); 
                Serial.println("Message writed!");
            }else{ 
                Serial.println(" Error opening file");
            }  
            request->send(SD, "/setup.html", "text/html");   
      });

      server.on("/fileNumber", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send_P(200, "text/plain", fileNameChar);
      });

    // Send a GET request generate random trasmission code and store in eeprom
      server.on("/boardCode", HTTP_GET, [] (AsyncWebServerRequest *request) {
                request->send(200, "text/plain", "Generating new transmittion code for the board...");
                generateBoardCode();
      });

        server.on("/read_autolock", HTTP_GET, [] (AsyncWebServerRequest *request) {
                uint autoLock = EEPROM.read(0);
                char lockValue[2]; 
                itoa(autoLock, lockValue, 10);
                request->send_P(200, "text/plain", lockValue);
        });

    // Send a GET request to <ESP_IP>/autolock?number=1>   (1 or 0 to enable and disable)
      server.on("/autolock", HTTP_GET, [] (AsyncWebServerRequest *request) {
            String inputMessage1;     
            if (request->hasParam(PARAM_INPUT_1)) {
                inputMessage1 = request->getParam(PARAM_INPUT_1)->value();
                request->send(200, "text/plain", "AUTO LOCK setting to: " + inputMessage1);
                byte newautolock = inputMessage1.toInt();
                Serial.print("Writing AUTO LOCK to: ");
                Serial.println(newautolock);
                EEPROM.write(0, newautolock);
                EEPROM.commit();
                if(newautolock == 0){
                        gravityEnable = false;
                }else{  
                        gravityEnable = true;
                }
            }else {
                inputMessage1 = "GET ERROR";
            }
      });

// offset for Vesc battery reading voltage
      server.on("/offset", HTTP_GET, [] (AsyncWebServerRequest *request) { // /offset?number=0
            String inputMessage1;     
            if (request->hasParam(PARAM_INPUT_1)) {
              inputMessage1 = request->getParam(PARAM_INPUT_1)->value();    
              request->send(200, "text/plain", "VOLTAGE OFFSET setting to: " + inputMessage1 + "0mV");
              int offset = inputMessage1.toInt();
              EEPROM.write(8, offset);
              EEPROM.commit();
              offsetCalc();
            }       
      });

      server.on("/read_offset", HTTP_GET, [] (AsyncWebServerRequest *request) {
            offsetCalc();
            String offset = String(voltageOffset, 2);
            request->send_P(200, "text/plain", offset.c_str());   
      });

      server.on("/json", HTTP_GET, [](AsyncWebServerRequest *request){
          printFile(jsonFileName);
          uint rxNumber = EEPROM.read(0);
          char serialNumber[2]; 
          itoa(rxNumber, serialNumber, 10);
          request->send_P(200, "text/plain", serialNumber);
      });

      server.on("/upgrade", HTTP_GET, [](AsyncWebServerRequest *request){
          request->send_P(200, "text/plain", "Update from server...");
          EEPROM.write(4, 0x0A);
          EEPROM.commit();
          delay(2000);
          ESP.restart();// after reboot it will update from server if network tecnofly is present
      });

      server.on("/readMessage", HTTP_GET, [](AsyncWebServerRequest *request){
            readMessage();
            char messagex[25] = "";
            strcat(messagex, data.message1); 
            strcat(messagex, ",");
            strcat(messagex, data.message2); 
            strcat(messagex, ",");
            strcat(messagex, data.message3);             
            String message = String(messagex);
            request->send_P(200, "text/plain", message.c_str());  
      }); 

      server.on("/data", HTTP_GET, [](AsyncWebServerRequest *request){
          request->send_P(200, "text/plain", dataToJson().c_str());
      });

      server.on("/reset", HTTP_GET, [](AsyncWebServerRequest *request){
          request->send(200, "text/plain", "Rebooting...");
          ESP.restart();
      });

      server.on("/ride", HTTP_GET, [](AsyncWebServerRequest *request){ //ride?number=5
          String inputNumber;     
          inputNumber = request->getParam("number")->value();   
          String fileContent = readFile("/ride" + inputNumber + ".json"); 
          request->send(200, "text/plain", fileContent);
      });

      server.on("/vesc", HTTP_GET, [](AsyncWebServerRequest *request){ //vesc?number=5
            String inputNumber;     
            inputNumber = request->getParam("number")->value();   
            String fileName = "/vescDat" + inputNumber + ".txt"; 
            File file = SD.open(fileName);
            if (!file) {
                request->send(500, "text/plain", "Errore nell'apertura del file");
                return;
            }
            request->send(file, fileName, "text/plain", false);
      });

// Endpoint per visualizzare i file della SPIFFS
    server.on("/list", HTTP_GET, [](AsyncWebServerRequest *request) {
        StaticJsonDocument<1024> jsonDoc;
        JsonArray filesArray = jsonDoc.createNestedArray("files");

        File root = SD.open("/");
        File file = root.openNextFile();

        while (file) {
            JsonObject fileObj = filesArray.createNestedObject();
            fileObj["name"] = file.name();
            fileObj["size"] = file.size();
            file = root.openNextFile();
        }
        String jsonString;
        serializeJson(jsonDoc, jsonString);
        request->send(200, "application/json", jsonString);
    });

    // Endpoint per il caricamento dei file su SD card
    server.on("/upload", HTTP_POST, [](AsyncWebServerRequest *request) {
            request->send(200, "text/plain", "Upload done");
    },[](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
        static File file;  
        if (index == 0) {  // Inizio del caricamento
            Serial.printf("Upload Start: %s\n", filename.c_str());
            SD.remove(("/" + filename).c_str());  // Rimuove il vecchio file
            file = SD.open(("/" + filename).c_str(), FILE_WRITE);
            if (!file) {
                Serial.println("Error opening file!");
                return;
            }
        }

        if (file) {
            file.write(data, len);  // Scrive i dati ricevuti
        }

        if (final) {  // Fine del caricamento
            if (file) {
                file.close();
                Serial.printf("Upload done: %s (%u bytes)\n", filename.c_str(), index + len);
            } else {
                Serial.println("Errore: file non valido durante la chiusura!");
            }
        }
    }
);

server.on("/delete", HTTP_DELETE, handleDeleteFile);

////////////////////// API FOR DEVELOPING 
      server.on("/brake", HTTP_GET, [](AsyncWebServerRequest *request){
            float brake = -5;  
              UART.setBrakeCurrent(brake);
              request->send(200, "text/plain", "ATTEMPT to change Brake current...");
      });

      server.on("/volt", HTTP_GET, [](AsyncWebServerRequest *request){
          String battVoltage = String(voltage, 1);
          request->send_P(200, "text/plain", battVoltage.c_str());
      });

      server.on("/sd", HTTP_GET, [](AsyncWebServerRequest *request){
          String SDMessage;
          if (!SD.begin(chipSelect)) {
              SDMessage = "SD CARD initialization failed";
          } else {
              SDMessage = "Card is working";
          }
          request->send_P(200, "text/plain", SDMessage.c_str());
      });

}

