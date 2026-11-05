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
    }  
    request->send(LittleFS, "/setup.html", "text/html");   
});

    server.on("/fileNumber", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", fileNameChar);
    });

// Send a GET request generate random trasmission code and store in eeprom
    server.on("/boardCode", HTTP_GET, [] (AsyncWebServerRequest *request) {
            request->send(200, "text/plain", "Generating new board code");
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
            request->send(200, "text/plain", "VOLTAGE OFFSET setting to: " + inputMessage1 + "mV");
            int offset = inputMessage1.toInt();
            EEPROM.put(8, offset);
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
//        printFile(jsonFileName);
        uint rxNumber = EEPROM.read(0);
        char serialNumber[2]; 
        itoa(rxNumber, serialNumber, 10);
        request->send_P(200, "text/plain", serialNumber);
    });

    server.on("/readMessage", HTTP_GET, [](AsyncWebServerRequest *request){
        readFileMessage();
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
            request->send(500, "text/plain", "Error opening file");
            return;
        }
        request->send(file, fileName, "text/plain", false);
    });

    server.on("/vescbin", HTTP_GET, [](AsyncWebServerRequest *request){
        if (!request->hasParam("number")) {
            request->send(400, "text/plain", "Missing parameter");
            return;
        }
        String inputNumber = request->getParam("number")->value();
        String fileName = "/vescDat" + inputNumber + ".bin";
        File file = LittleFS.open(fileName);
        if (!file) {
            request->send(404, "text/plain", "File not found");
            return;
        }
        request->send(file, fileName, "application/octet-stream", false);
    });

// Endpoint per visualizzare i file della SPIFFS
    server.on("/list", HTTP_GET, [](AsyncWebServerRequest *request) {
        String json = "{\"files\":[";
        json.reserve(512);
        bool first = true;
        File root = LittleFS.open("/");
        File file = root.openNextFile();
        while (file) {
            if (!first) json += ",";
            first = false;
            json += "{\"name\":\"" + String(file.name()) + "\",\"size\":" + String(file.size()) + "}";
            file = root.openNextFile();
        }
        json += "]}";
        request->send(200, "application/json", json);
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

        // BATTERY ALARM THRESHOLDS (fix 13/10/2026): soglie allarme/reset per pacco 10s e 12s
        server.on("/battAlert", HTTP_GET, [] (AsyncWebServerRequest *request) {
                if (request->hasParam("alarm10")) battAlarm10s = request->getParam("alarm10")->value().toFloat();
                if (request->hasParam("alarm12")) battAlarm12s = request->getParam("alarm12")->value().toFloat();
                if (request->hasParam("reset10")) battReset10s = request->getParam("reset10")->value().toFloat();
                if (request->hasParam("reset12")) battReset12s = request->getParam("reset12")->value().toFloat();
                saveBattAlertConfig();
                char buf[96];
                snprintf(buf, sizeof(buf), "OK 10s %.1f/%.1f  12s %.1f/%.1f", battAlarm10s, battReset10s, battAlarm12s, battReset12s);
                request->send(200, "text/plain", buf);
        });

        server.on("/read_battAlert", HTTP_GET, [] (AsyncWebServerRequest *request) {
                char buf[180];
                snprintf(buf, sizeof(buf), "{\"alarm10\":%.1f,\"reset10\":%.1f,\"alarm12\":%.1f,\"reset12\":%.1f,\"pack\":\"%s\"}",
                         battAlarm10s, battReset10s, battAlarm12s, battReset12s, battIs12s ? "12s" : "10s");
                request->send(200, "application/json", buf);
        });

        // THROTTLE RESPONSE CURVE (fix 21/10/2026)
        server.on("/throttleCurve", HTTP_GET, [] (AsyncWebServerRequest *request) {
                if (request->hasParam("gamma")) throttleCurveGamma = request->getParam("gamma")->value().toFloat();
                if (request->hasParam("mix"))   throttleCurveMix   = request->getParam("mix")->value().toFloat();
                if (!(throttleCurveGamma >= 0.40f && throttleCurveGamma <= 3.00f)) throttleCurveGamma = 1.0f;
                if (!(throttleCurveMix >= 0.0f && throttleCurveMix <= 1.0f)) throttleCurveMix = 0.0f;
                saveThrottleCurveConfig();
                char buf[96];
                snprintf(buf, sizeof(buf), "OK gamma %.2f mix %.2f", throttleCurveGamma, throttleCurveMix);
                request->send(200, "text/plain", buf);
        });

        server.on("/read_throttleCurve", HTTP_GET, [] (AsyncWebServerRequest *request) {
                char buf[96];
                snprintf(buf, sizeof(buf), "{\"gamma\":%.2f,\"mix\":%.2f}", throttleCurveGamma, throttleCurveMix);
                request->send(200, "application/json", buf);
        });
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

        // DIRECT CUT SAFETY READ
        server.on("/readDirectCut", HTTP_GET, [] (AsyncWebServerRequest *request) {
                uint dcValue = EEPROM.read(3);
                char buf[4]; 
                itoa(dcValue, buf, 10);
                request->send_P(200, "text/plain", buf);
        });

        // DIRECT CUT SAFETY SET
        server.on("/directCut", HTTP_GET, [] (AsyncWebServerRequest *request) {
                String inputMessage;
                if (request->hasParam(PARAM_INPUT_1)) {
                        inputMessage = request->getParam(PARAM_INPUT_1)->value();
                        request->send(200, "text/plain", "DIRECT CUT angle: " + inputMessage + "°");
                        directCutDegrees = inputMessage.toInt();
                        EEPROM.write(3, directCutDegrees);
                        EEPROM.commit();
                        if(directCutDegrees == 0){
                                directCutEnabled = false;
                        } else {
                                directCutEnabled = true;
                        }
                }
        });

        // EVENT LOG DOWNLOAD
        server.on("/eventlog", HTTP_GET, [] (AsyncWebServerRequest *request) {
                if (LittleFS.exists(EVENT_LOG)) {
                        request->send(LittleFS, EVENT_LOG, "text/plain");
                } else {
                        request->send(200, "text/plain", "No events logged yet.");
                }
        });

        // EVENT LOG CLEAR
        server.on("/eventclear", HTTP_GET, [] (AsyncWebServerRequest *request) {
                LittleFS.remove(EVENT_LOG);
                LittleFS.remove("/events.old");
                request->send(200, "text/plain", "Event log cleared.");
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
                        EEPROM.write(0, thresholdDegrees);
                        EEPROM.commit();
                        if(thresholdDegrees == 0){
                                checkPositionEnable = false;
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
            EEPROM.get(12, lifeCounter);   // legge 4 byte dall'indirizzo 12
            char lifeValue[12];            // buffer per il numero in stringa
            ultoa(lifeCounter, lifeValue, 10); // converte in stringa base 10
            request->send(200, "text/plain", lifeValue);
        });

        server.on("/lifeReset", HTTP_GET, [] (AsyncWebServerRequest *request) {
            EEPROM.put(12, 0);
            EEPROM.commit();
            request->send(200, "text/plain", "Life reset to zero");
        });

        server.on("/analysis", HTTP_GET, [](AsyncWebServerRequest *request){
            if(accelerometerConnected){
                    rideAnalysis();
            }else{
                    rideAnalysisNoADXL();
            }
            request->send(200, "text/plain", "Start analysis");
        });


    server.on("/checkAdxl", HTTP_GET, [](AsyncWebServerRequest *request){
            if(accelerometerConnected){
                request->send(200, "text/plain", "present");
            }else{
                request->send(200, "text/plain", "NOT present");
            }
    });

    server.on("/xyzCalib", HTTP_GET, [](AsyncWebServerRequest *request){
            request->send(200, "text/plain", "ok");
            calibrateAccelerometer();
    });  

    server.on("/rssiReset", HTTP_GET, [](AsyncWebServerRequest *request){
            request->send(200, "text/plain", "ok");
            portENTER_CRITICAL(&rxStateMux);
            packetCount = 0;
            rssiAvg = 0;  
            packetLost = 0;
            lossPercent = 0;
            portEXIT_CRITICAL(&rxStateMux);
    }); 

    //UCCIO: endpoint per leggere il record pump & glide
    server.on("/readRecord", HTTP_GET, [](AsyncWebServerRequest *request){
            char buf[12];
            sprintf(buf, "%u", record);
            request->send(200, "text/plain", buf);
    });

    //UCCIO: endpoint per resettare il record
    server.on("/recordReset", HTTP_GET, [](AsyncWebServerRequest *request){
            record = 0;
            saveRecord(0);
            request->send(200, "text/plain", "Record reset to 0");
    }); 

 
    server.on("/channel", HTTP_GET, [](AsyncWebServerRequest *request){
            uint8_t channel;
            wifi_second_chan_t second;
            esp_wifi_get_channel(&channel, &second);
            char json[128];
            snprintf(json, sizeof(json),"{\"channel\":%d}", channel);
        request->send(200, "application/json", json);
    });  








    // ── FLASH MCCONF: invia mcconf.bin al VESC (leggendo da LittleFS) ──
    server.on("/flashMcconf", HTTP_GET, [](AsyncWebServerRequest *request) {
        bool ok = flashMcconfToVesc();
        if (ok) {
            request->send(200, "text/plain", "MCCONF flashed to VESC");
        } else {
            request->send(500, "text/plain", "MCCONF flash failed (check serial, file)");
        }
    });

    // ── READ MCCONF: aggiorna i valori dal file su LittleFS ──
    server.on("/readMcconf", HTTP_GET, [](AsyncWebServerRequest *request) {
        bool ok = loadMcconfFromFile();
        if (ok) {
            request->send(200, "text/plain", "MCCONF loaded from LittleFS");
        } else {
            request->send(500, "text/plain", "Failed to read MCCONF");
        }
    });

    // ── Legge i parametri come testo (usato da expert.html per popolare i campi) ──
    server.on("/vescParam", HTTP_GET, [](AsyncWebServerRequest *request) {
        String text = readVescParamsAsText();
        if (text.startsWith("ERRORE")) {
            request->send(500, "text/plain", text);
        } else {
            request->send(200, "text/plain", text);
        }
    });

    // ── Scrive parametri al VESC (usato da expert.html -> Write to VESC) ──
    // /vescParams?l_current_max=80&l_current_min=0&l_in_current_max=81...
    server.on("/vescParams", HTTP_GET, [](AsyncWebServerRequest *request) {
        int nParams = request->params();
        if (nParams == 0) {
            request->send(400, "text/plain", "Missing parameters");
            return;
        }
        // Ricostruisce la query string dai parametri individuali
        String qs = "";
        for (int i = 0; i < nParams; i++) {
            const AsyncWebParameter *p = request->getParam(i);
            if (i > 0) qs += "&";
            qs += p->name() + "=" + p->value();
        }
        bool ok = setVescParamsFromQuery(qs, portMAX_DELAY);
        if (ok) {
            request->send(200, "text/plain", "OK scritto");
        } else {
            request->send(500, "text/plain", "FAILED (no mcconf.bin? VESC offline?)");
        }
    });

    // ── DOWNLOAD mcconf.bin ──
    server.on("/mcconf.bin", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (LittleFS.exists("/mcconf.bin")) {
            request->send(LittleFS, "/mcconf.bin", "application/octet-stream");
        } else {
            request->send(404, "text/plain", "mcconf.bin not found");
        }
    });

    server.on("/dumpConfig", HTTP_GET,[](AsyncWebServerRequest *request) {
        dumpVescConfiguration();
        request->send(200, "text/plain", "Configuration dumped on Serial");
    });

    // ── DUMP & SAVE VESC CONFIG → salva su LittleFS ──
    server.on("/dumpAndSaveVesc", HTTP_GET,[](AsyncWebServerRequest *request) {
        int n = dumpAndSaveVescConfig();
        if (n > 0) {
            request->send(200, "text/plain",
                "✅ Config dumpata e salvata su /mcconf.bin (" + String(n) + " byte)");
        } else {
            request->send(500, "text/plain", "❌ Dump fallito (VESC non risponde?)");
        }
    });

    // ── Legge/scrive la potenza max (slider expert.html) ──
    // ── Legge versione firmware VESC ──
    server.on("/vescFwVersion", HTTP_GET, [](AsyncWebServerRequest *request) {
        String ver = getCachedVescFwVersion();
        request->send(200, "text/plain", ver);
    });

    server.on("/vescPower", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (request->hasParam("w")) {
            float watt = request->getParam("w")->value().toFloat();
            String qs = "l_watt_max=" + String(watt, 0);
            bool ok = setVescParamsFromQuery(qs, portMAX_DELAY);
            request->send(ok ? 200 : 500, "text/plain", ok ? String(watt, 0) : "FAILED");
        } else {
            float w = readVescWattMax();
            request->send(200, "text/plain", String(w, 0));
        }
    });

    server.on("/setPower", HTTP_GET,[](AsyncWebServerRequest *request) {
        if (!request->hasParam("motor") ||
            !request->hasParam("battery")) {

            request->send(400,
                        "text/plain",
                        "use ?motor=70&battery=71");
            return;
        }
        float motor = request->getParam("motor")->value().toFloat();
        float battery = request->getParam("battery")->value().toFloat();
        bool ok = setCurrentLimits(motor, battery);

        request->send(ok ? 200 : 500,
                    "text/plain",
                    ok ? "OK" : "FAILED");
});


}
