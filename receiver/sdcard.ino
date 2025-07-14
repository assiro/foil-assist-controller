/*
SD CARD FUNCTIONS
*/
void initSDCard(){
          if (!SD.begin(chipSelect)) {
              Serial.println("SD CARD initialization failed");
              sdcard = false;
          } else {
              Serial.println("Card is present.");
              sdcard = true;
          }
}

String readFile(String path) {
    File file = SD.open(path);
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

/*
void readFile(fs::FS &fs, const char * path){
  Serial.printf("Reading file: %s\n", path);
  File file = fs.open(path);
  if(!file){
    Serial.println("Failed to open file for reading");
    return;
  }
  Serial.print("Read from file: ");
  while(file.available()){
    Serial.write(file.read());
  }
  file.close();
}
*/
void writeFile(fs::FS &fs, const char * path, const char * message){
  Serial.printf("Writing file: %s\n", path);

  File file = fs.open(path, FILE_WRITE);
  if(!file){
    Serial.println("Failed to open file for writing");
    return;
  }
  if(file.print(message)){
    Serial.println("File written");
  } else {
    Serial.println("Write failed");
  }
  file.close();
}

void appendFile(fs::FS &fs, const char * path, const char * message){
  Serial.printf("Appending to file: %s\n", path);

  File file = fs.open(path, FILE_APPEND);
  if(!file){
    Serial.println("Failed to open file for appending");
    return;
  }
  if(file.print(message)){
      Serial.println("Message appended");
  } else {
    Serial.println("Append failed");
  }
  file.close();
}

void deleteFile(fs::FS &fs, const char * path){
  Serial.printf("Deleting file: %s\n", path);
  if(fs.remove(path)){
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
    if (!SD.exists(fileName)) {
        request->send(404, "text/plain", "Error: missing file!");
        return;
    }
    if (SD.remove(fileName)) {
        request->send(200, "text/plain", "File deleted: " + fileName);
    } else {
        request->send(500, "text/plain", "Error deleting file.");
    }
}

