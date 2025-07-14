
void generateBoardCode(){
            Serial.println("Generate new code...");
            byte value1 = random(0, 256);
            byte value2 = random(0, 256);
            Serial.print("Value 1: ");
            Serial.println(value1, HEX);
            Serial.print("Value 2: ");
            Serial.println(value2, HEX);  
            EEPROM.write(1, value1);
            EEPROM.write(2, value2);
            EEPROM.commit();  
            delay(1000);
            Serial.println("Rebooting...");
            ESP.restart();
}