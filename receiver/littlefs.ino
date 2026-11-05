/*
LITTLEFS FUNCTIONS
*/

String readFile(String path) {
    File file = LittleFS.open(path);
    if (!file) {
        Serial.println("Error opening file");
        return "";
    }
    String content;
    const size_t fileSize = file.size();
    if (fileSize <= 8192) {
        content.reserve(fileSize);
    }
    uint8_t buffer[128];
    while (file.available()) {
        size_t count = file.read(buffer, sizeof(buffer));
        if (count == 0) break;
        content.concat((const char*)buffer, count);
    }
    file.close();
    return content;
}


void writeToFile(const char * path){
      file = LittleFS.open(path, FILE_WRITE);
      if (file){
            file.print("");
            file.close(); 
            Serial.println(" New data file created!");
      }else{ 
            Serial.println(" Error opening file");
      }
}


void fileNumber(const char * path, int fileNumber){
      file = LittleFS.open(path, FILE_WRITE);
      if (file){
            file.print(fileNumber);
            file.close(); 
      //      Serial.println("New file number created!");
      }else{ 
            Serial.println("Error creating file!");
      }
}


void handleDeleteFile(AsyncWebServerRequest *request) {
    if (!request->hasParam("file")) {
        request->send(400, "text/plain", "Error: missing file!");
        return;
    }
    String fileName = "/" + request->getParam("file")->value();
    if (!LittleFS.exists(fileName)) {
        request->send(404, "text/plain", "Error: missing file!");
        return;
    }
    if (LittleFS.remove(fileName)) {
        request->send(200, "text/plain", "File deleted: " + fileName);
    } else {
        request->send(500, "text/plain", "Error deleting file.");
    }
}

String getMemoryInfoJson() {
    size_t total = LittleFS.totalBytes();
    size_t used  = LittleFS.usedBytes();
    size_t free  = total - used;

    String json = "{";
    json.reserve(64);
    json += "\"total\":" + String(total) + ",";
    json += "\"used\":" + String(used) + ",";
    json += "\"free\":" + String(free);
    json += "}";

    return json;
}

void readFileNumber(){
    const char* path = "/fileNumb.txt";
    size_t len = 0;
    File file = LittleFS.open(path);
    fileName;
    if(file.available()){
            char inChar = file.read();
            Serial.print("Last file number is: ");
            Serial.println(inChar);     
            char buf[64];
            snprintf(buf, sizeof(buf), "Ride number %c", inChar);
            logEvent(buf);
            fileName = inChar - '0';
    }
    file.close();
    itoa(fileName, fileNameChar,10);
}

void readFileMessage(){
  File file = LittleFS.open("/message.txt");
  if (!file) {
//      Serial.println("Errore nell'apertura del file.");
      return;
  }
  auto readMessageLine = [&file](char* destination) {
    size_t length = file.readBytesUntil('\n', destination, 8);
    destination[length] = '\0';
    // Il delimitatore viene consumato automaticamente se la riga è più corta
    // del buffer. Se il buffer è pieno, scarta l'eventuale parte rimanente.
    if (length == 8) {
      while (file.available()) {
        int c = file.read();
        if (c < 0 || c == '\n') break;
      }
    }
  };
  readMessageLine(data.message1);
  readMessageLine(data.message2);
  readMessageLine(data.message3);
  file.close();
  Serial.print(data.message1);
  Serial.print(" ");
  Serial.print(data.message2);
  Serial.print(" ");
  Serial.println(data.message3);  
}

void readFileRecord(){
    const char* path = "/record.txt";
    File file = LittleFS.open(path);
    if(file && file.size() > 0){
        char buf[12];
        int len = file.readBytes(buf, sizeof(buf) - 1);
        buf[len] = '\0';
        record = atoi(buf);
        Serial.print("RECORD is: ");Serial.print(record);Serial.println("s");
        file.close();
    } else {
        if(file) file.close();
        // Crea file con record default se non esiste
        record = 10;
        saveRecord(record);
        Serial.printf("Record file created with default: %ds\n", record);
    }
}

void saveRecord(uint newRecord){
    File file = LittleFS.open("/record.txt", FILE_WRITE);
    if (file){
        file.print(newRecord);
        file.close();
    }
}

