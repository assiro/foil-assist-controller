void api(){
	// Route for root / web page
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(LittleFS, "/index.html", "text/html");
    Serial.println("Client Connected");
    });

    server.onNotFound([](AsyncWebServerRequest *request){
    request->send(LittleFS, "/index.html", "text/html");
    });    

    server.on("/ver", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send_P(200, "text/plain", VERSION.c_str());
    });

    server.on("/rx_number", HTTP_GET, [](AsyncWebServerRequest *request){
        uint8_t lsb = EEPROM.read(1);
        uint8_t msb = EEPROM.read(2);
        uint16_t rxNumber = (msb << 8) | lsb;
//            Serial.print("EEPROM serial number: "); Serial.println(rxNumber);
        char serialNumber[5]; 
        itoa(rxNumber, serialNumber, 10);
        request->send_P(200, "", serialNumber);
    });

server.on("/Wmessage", HTTP_GET, [](AsyncWebServerRequest *request){
    String wmessage1, wmessage2, wmessage3;
    if (request->hasParam("input1")) wmessage1 = request->getParam("input1")->value();  
    if (request->hasParam("input2")) wmessage2 = request->getParam("input2")->value();
    if (request->hasParam("input3")) wmessage3 = request->getParam("input3")->value();

    File file = LittleFS.open("/message.txt", FILE_WRITE);
    if (file) {
    file.println(wmessage1);
    file.println(wmessage2);
    file.print(wmessage3);            
    file.close(); 
//        Serial.println("Message written!");
//        } else { 
//        Serial.println("Error opening file");
    }  
    request->send(LittleFS, "/setup.html", "text/html");   
});

    server.on("/fileNumber", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", fileNameChar);
    });

// Send a GET request generate random trasmission code and store in eeprom
    server.on("/boardCode", HTTP_GET, [] (AsyncWebServerRequest *request) {
            request->send(200, "text/plain", "Generating new transmittion code for the board...");
            generateBoardCode();
    });
// SET specific board code by api  
    server.on("/setCode", HTTP_GET, [] (AsyncWebServerRequest *request) { // /setCode?number=63733
        String inputMessage1;     
        if (request->hasParam(PARAM_INPUT_1)) {
            inputMessage1 = request->getParam(PARAM_INPUT_1)->value();    
            request->send(200, "text/plain", "BOARD CODE setting to: " + inputMessage1);
            uint16_t codeValue = inputMessage1.toInt();
            Serial.print("New board code: "); Serial.print(codeValue);
            byte value1 = (codeValue >> 8) & 0xFF;
            byte value2 = codeValue & 0xFF;
            EEPROM.write(1, value2);
            EEPROM.write(2, value1);
            EEPROM.commit();  
            delay(2000);
            ESP.restart();// after reboot it will update from server if network tecnofly is present
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
        if (!request->hasParam("number")) {
            request->send(400, "text/plain", "Missing parameter");
            return;
        }
        inputNumber = request->getParam("number")->value();   
        String fileContent = readFile("/ride" + inputNumber + ".json"); 
        request->send(200, "text/plain", fileContent);
    });

    server.on("/vesc", HTTP_GET, [](AsyncWebServerRequest *request){
        if (!request->hasParam("number")) {
            request->send(400, "text/plain", "Missing parameter");
            return;
        }
        String inputNumber = request->getParam("number")->value();   
        String fileName = "/vescDat" + inputNumber + ".txt"; 
        File file = LittleFS.open(fileName);
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

        File root = LittleFS.open("/");
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

        // Endpoint per il caricamento dei file su little FS
        server.on("/upload", HTTP_POST, [](AsyncWebServerRequest *request) {
                request->send(200, "text/plain", "Upload done");
        },[](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
            static File file;  
            if (index == 0) {  // Inizio del caricamento
                Serial.printf("Upload Start: %s\n", filename.c_str());
                LittleFS.remove(("/" + filename).c_str());  // Rimuove il vecchio file
                file = LittleFS.open(("/" + filename).c_str(), FILE_WRITE);
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
        });

        server.on("/delete", HTTP_DELETE, handleDeleteFile);

        server.on("/meminfo", HTTP_GET, [](AsyncWebServerRequest *request){
                    request->send(200, "application/json", getMemoryInfoJson());
        });

        server.on("/upgrade", HTTP_GET, [](AsyncWebServerRequest *request){
                request->send_P(200, "text/plain", "Upgrade from server...");
                EEPROM.write(4, 0x0A);
                EEPROM.commit();
                delay(2000);
                ESP.restart();// after reboot it will update from server if network tecnofly is present
        });

        // POSITION SENSOR ENABLE/DISABLE
        server.on("/readAutolock", HTTP_GET, [] (AsyncWebServerRequest *request) {
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
                        thresholdDegrees  = inputMessage1.toInt();
                        Serial.print("Writing AUTO LOCK to: ");
                        Serial.println(thresholdDegrees);
                        EEPROM.write(0, thresholdDegrees);
                        EEPROM.commit();
                        if(thresholdDegrees == 0){
                                checkPositionEnable = false;
    //                            Serial.println("✅ UNLOCK");
                                gravityAlarm = false;
                        }else{  
                                checkPositionEnable = true;
                        }
                }else {
                        inputMessage1 = "GET ERROR";
                }
        });

        server.on("/lifeCounter", HTTP_GET, [] (AsyncWebServerRequest *request) {
            uint32_t lifeCounter;
            EEPROM.get(12, lifeCounter);   // legge 4 byte dall’indirizzo 12
            char lifeValue[12];            // buffer per il numero in stringa
            ultoa(lifeCounter, lifeValue, 10); // converte in stringa base 10
            request->send(200, "text/plain", lifeValue);
        });

        server.on("/lifeReset", HTTP_GET, [] (AsyncWebServerRequest *request) {
            EEPROM.put(12, 0);
            EEPROM.commit();
            request->send(200, "text/plain", "Life reset to zero");
        });
        
////////////////////// API FOR DEVELOPING 

    server.on("/volt", HTTP_GET, [](AsyncWebServerRequest *request){
        String battVoltage = String(voltage, 1);
        request->send_P(200, "text/plain", battVoltage.c_str());
    });

    server.on("/analysis", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "text/plain", "Start Analysis...");
        rideAnalysis();
    });

    server.on("/format", HTTP_GET, [](AsyncWebServerRequest *request){
            request->send(200, "text/plain", "Formatting flash...");
            LittleFS.format();  //format();
            delay(1000);
            ESP.restart();
    });

    server.on("/apOff", HTTP_GET, [](AsyncWebServerRequest *request){
            request->send(200, "text/plain", "AP OFF");
            disableAP();
    });

//// IMU API 
    server.on("/getImu", HTTP_GET, [](AsyncWebServerRequest *request){
            String fileImu = "/imu.bin"; 
            File file = LittleFS.open(fileImu);
            if (!file) {
                request->send(500, "text/plain", "Errore nell'apertura del file");
                return;
            }
            request->send(file, fileImu, "text/plain", false);
    });

    server.on("/fileXYZ", HTTP_GET, [](AsyncWebServerRequest *request){
            request->send(200, "text/plain", "start IMU");
            fileCounter = 0;
            openImuFile();
    });

    server.on("/close", HTTP_GET, [](AsyncWebServerRequest *request){
            request->send(200, "text/plain", "stop IMU");
            if (imuFile) {
                imuFile.flush();
                imuFile.close();
                Serial.println("✅ imu.bin chiuso");
        }
    });

    server.on("/checkAdxl", HTTP_GET, [](AsyncWebServerRequest *request){
            if(accelerometerConnected){
                request->send(200, "text/plain", "present");
            }else{
                request->send(200, "text/plain", "NOT present");
            }
    });

 /* */
    server.on("/xyz", HTTP_GET, [](AsyncWebServerRequest *request) {
        sensors_event_t event;
        accel.getEvent(&event);
        float x = event.acceleration.y;
        float y = event.acceleration.z;
        float z = event.acceleration.x;
        float roll  = atan2(y, z) * 180.0 / PI;
        float pitch = atan2(-x, sqrt(y*y + z*z)) * 180.0 / PI;
        char json[128];
        snprintf(json, sizeof(json),
            "{\"x\":%.2f,\"y\":%.2f,\"z\":%.2f,\"roll\":%.1f,\"pitch\":%.1f}",
            x, y, z, roll, pitch
    );
    request->send(200, "application/json", json);
    });

    server.on("/xyzCalib", HTTP_GET, [](AsyncWebServerRequest *request){
            request->send(200, "text/plain", "ok");
            calibrateAccelerometer();
    });  

    server.on("/rssi", HTTP_GET, [](AsyncWebServerRequest *request) {
//    uint32_t total = packetCount + packetLost;
//    lossPercent = (total > 0) ? (100.0f * packetLost / total) : 0.0f;
    char json[160];
    snprintf(json, sizeof(json),
        "{\"rssi\":%d,\"rssiAvg\":%.1f,"
        "\"packets\":%lu,\"lost\":%lu,\"lossPercent\":%.1f}",
        (int)rssiLast, rssiAvg,
        packetCount, packetLost, lossPercent
    );
    request->send(200, "application/json", json);
    });

    server.on("/rssiReset", HTTP_GET, [](AsyncWebServerRequest *request){
            request->send(200, "text/plain", "ok");
            packetCount = 0;
            rssiAvg = 0;  
            rssiSum = 0;
            packetLost = 0;
            lossPercent = 0;
    }); 
}

