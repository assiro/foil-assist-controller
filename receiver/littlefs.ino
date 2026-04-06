/*
LITTLEFS FUNCTIONS
*/

String readFile(String path) {
    File file = LittleFS.open(path);
    if (!file) {
        Serial.println("Error opening file");
        return "";
    }
    String content = "";
    while (file.available()) {
        content += char(file.read());
    }
    file.close();
    return content;
}


void writeFile(const char * path, const char * message) {
  Serial.printf("Writing file: %s\n", path);
  File file = LittleFS.open(path, FILE_WRITE);
  if (!file) {
    Serial.println("Failed to open file for writing");
    return;
  }
  if (file.print(message)) {
    Serial.println("File written");
  } else {
    Serial.println("Write failed");
  }
  file.close();
}

void appendFile(const char * path, const char * message) {
  Serial.printf("Appending to file: %s\n", path);
  File file = LittleFS.open(path, FILE_APPEND);
  if (!file) {
    Serial.println("Failed to open file for appending");
    return;
  }
  if (file.print(message)) {
    Serial.println("Message appended");
  } else {
    Serial.println("Append failed");
  }
  file.close();
}


void deleteFile(const char * path) {
  Serial.printf("Deleting file: %s\n", path);
  if (LittleFS.remove(path)) {
    Serial.println("File deleted");
  } else {
    Serial.println("Delete failed");
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
        if(file.size() == 0){
              Serial.println("File number empty!");
        }else{
              char inChar = file.read();
              Serial.print("Last Log file number is: ");
              Serial.println(inChar);     
              fileName = inChar - '0';
        } 
    }else{
        Serial.println("file number not present...");
    }
    file.close();
    itoa(fileName, fileNameChar,10);
}


void readMessage(){
  File file = LittleFS.open("/message.txt");
  if (!file) {
//      Serial.println("Errore nell'apertura del file.");
      return;
  }
  String str1 = file.readStringUntil('\n');
  str1.toCharArray(data.message1, 9);
  String str2 = file.readStringUntil('\n');
  str2.toCharArray(data.message2, 9);
  String str3 = file.readStringUntil('\n');
  str3.toCharArray(data.message3, 9);
  file.close();
  Serial.print(data.message1);
  Serial.print(" ");
  Serial.print(data.message2);
  Serial.print(" ");
  Serial.println(data.message3);  
}
/*
void format(){
//  Serial.println("Formatt LittleFS...");
  if (LittleFS.format()) {
    Serial.println("LittleFS formatted");
  } else {
    Serial.println("Error...");
  }
}*/