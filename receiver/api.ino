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
          uint rxNumber = EEPROM.read(0);
          char serialNumber[2]; 
          itoa(rxNumber, serialNumber, 10);
          request->send_P(200, "text/plain", serialNumber);
      });

      server.on("/Wmessage", HTTP_GET, [](AsyncWebServerRequest *request){
            String wmessage1;
            String wmessage2;
            String wmessage3;        
            if (request->hasParam("input1")) {
              wmessage1 = request->getParam("input1")->value();
    //          Serial.print("Welcome message 1: ");
    //          Serial.println(wmessage1);      
            }    
            if (request->hasParam("input2")) {
              wmessage2 = request->getParam("input2")->value();
    //          Serial.print("Welcome message 2: ");
    //          Serial.println(wmessage2);
            }
            if (request->hasParam("input3")) {
              wmessage3 = request->getParam("input3")->value();
    //          Serial.print("Welcome message 3: ");
    //          Serial.println(wmessage3);
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

    // Send a GET request to <ESP_IP>/serial?number=5>
      server.on("/serial", HTTP_GET, [] (AsyncWebServerRequest *request) {
            String inputMessage1;     
            if (request->hasParam(PARAM_INPUT_1)) {
              inputMessage1 = request->getParam(PARAM_INPUT_1)->value();
              request->send(200, "text/plain", "Serial Number setting to: " + inputMessage1);
              byte newSerialNumber = inputMessage1.toInt();
              Serial.print("Writing new coding number: ");
              Serial.println(newSerialNumber);
              EEPROM.write(0, newSerialNumber);
              EEPROM.commit();
              delay(2000);
              Serial.print("Rebooting...");
              ESP.restart();
            }else {
              inputMessage1 = "GET ERROR";
            }
    //        Serial.println(inputMessage1);
      });
// offset for Vesc battery reading voltage
      server.on("/offset", HTTP_GET, [] (AsyncWebServerRequest *request) {
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
            char messagex[25];
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


/*
      server.on("/spin1", HTTP_GET, [](AsyncWebServerRequest *request){
            float rpm = -100000;  
              UART.setRPM(rpm);
              request->send(200, "text/plain", "ATTEMPT1 to change Spin to right...");
      });

      server.on("/spin2", HTTP_GET, [](AsyncWebServerRequest *request){         

bool invertDirection = false;
  // Pacchetto base di configurazione (esempio semplificato)
  uint8_t payload[3];
  // Comando: COMM_WRITE_CONF
  payload[0] = 0x01;  // Command ID per COMM_WRITE_CONF
  payload[1] = invertDirection ? 1 : 0;  // Flag per invertire direzione
  // Calcolo del checksum (semplice XOR di tutti i byte del payload)
  uint8_t checksum = payload[0] ^ payload[1];
  // Costruzione del pacchetto completo
  uint8_t packet[6];
  packet[0] = 0x02;  // Inizio del pacchetto
  packet[1] = sizeof(payload);  // Lunghezza del payload
  packet[2] = payload[0];
  packet[3] = payload[1];
  packet[4] = checksum;  // Checksum
  packet[5] = 0x03;  // Fine del pacchetto

  // Invio del pacchetto tramite seriale
  Serial2.write(packet, sizeof(packet));
  Serial.println("Pacchetto inviato per configurare la direzione del motore.");

          request->send(200, "text/plain", "ATTEMPT2 to change Spin to right...");
      });
*/
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

      server.on("/lora", HTTP_GET, [](AsyncWebServerRequest *request){
          String lora = String(LoRa.packetRssi()) + " " + String(LoRa.packetSnr()); 
          request->send_P(200, "text/plain", lora.c_str());
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

