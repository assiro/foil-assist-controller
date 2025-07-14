void rideAnalysis(){
    const int surfMinTime = 5; //seconds
    const int fileMinTime = 10; //minutes
//    recordEnable = false;

    uint count = 0;
    char rideFileName[20];
    memset(rideFileName, 0, sizeof(rideFileName));
    //takeOff
    uint takeOffTreshold = 2000;
    uint takeOff = 0;
    uint consecutiveHighPower = 0;  
    bool inTakeOff = false;  
    String startTakeOffTime;  
    bool falling = false;
    //waves
    uint wave = 0;
    uint waveTime = 0; 
    bool inWave = false;  
    String startWaveTime;  
    uint waveBestTime = 0;
    uint oldWaveBestTime = 0;

    oneTimeFlag = true;
//     for(uint dataFile=0; dataFile<10; dataFile++){   //file number 0 - 9
     uint dataFile=6; 
        wave = 0;
        waveBestTime = 0;
        takeOff = 0;
        count = 0;
        itoa(dataFile, fileNameChar,10);
        strcat(rideFileName, "/vescDat");
        strcat(rideFileName, fileNameChar);
        strcat(rideFileName, ".txt");
        Serial.print("Analysis of file: ");
        Serial.println(rideFileName);

        File file = SD.open(rideFileName);
        if (!file) {
            Serial.println("Errore apertura file");
            return;
        }

        while (file.available()) {
            String line = file.readStringUntil('\n');
            VESCData data;
            sscanf(line.c_str(), "%5s |R %d |V %f |I %f |M %f |W %d |TH %d%%", 
                  data.time, &data.rpm, &data.voltage, &data.current,&data.Mcurrent, &data.watts, &data.throttle);

            // Analisi Partenze - Controlla se la potenza supera 2000W
            if (data.watts > takeOffTreshold) {
                if (!inTakeOff) {
                    startTakeOffTime = data.time;  // Memorizza il tempo di inizio
                }
                consecutiveHighPower++;
                inTakeOff = true;
            } else {
                if (inTakeOff && consecutiveHighPower >= 3) {
                      takeOff++;
/*                      Serial.print("TakeOff #"); Serial.print(takeOff);
                      int diff = timeDifference(startTakeOffTime, data.time);
                      Serial.print(" - Durata: ");
                      Serial.print(diff); 
                      Serial.print(" - Inizio: "); Serial.print(startTakeOffTime);
                      Serial.print(" | Fine: "); Serial.println(data.time);
  */
                }
                consecutiveHighPower = 0;
                inTakeOff = false;
    //           if (falling){wave--;}
                falling = false;
            }

            // Analisi surfate - Controlla throttle a 0
            if (data.throttle == 0) {
                if (!inWave) {
                    startWaveTime = data.time;  // Memorizza il tempo di inizio
                }
                waveTime++;
                inWave = true;
            } else {
                if(inWave && waveTime > 5 && waveTime < 300) {  // limit to 5 min of possible pump or wave
                    wave++;
                    Serial.println("add wave more..");
                    if(wave > 99){wave = 99;}  // limit - ERROR in the calc
                    falling = true; 
                    if(waveTime > waveBestTime){
                          oldWaveBestTime = waveBestTime;
                          waveBestTime = waveTime;
                    }
/*                   Serial.print("Wave #"); Serial.print(wave);
                    int diff = timeDifference(startWaveTime, data.time);
                    Serial.print(" - Durata: ");
                    Serial.print(diff);                    
                    Serial.print(" - Inizio: "); Serial.print(startWaveTime);
                    Serial.print(" | Fine: "); Serial.println(data.time);
*/ 
                }
                waveTime = 0;
                inWave = false;
            }
            count++;       
    //        Serial.print("Tempo: "); Serial.print(data.time);
    //        Serial.print(" RPM: "); Serial.print(data.rpm);
    //        Serial.print(" V: "); Serial.print(data.voltage);
    //        Serial.print(" I: "); Serial.print(data.current);
    //        Serial.print(" W: "); Serial.print(data.watts);
    //        Serial.print(" TH: "); Serial.println(data.throttle);
        }
        file.close();
        if (count > 0) {
          //  wave = wave - takeOff;
/**/            Serial.print("Numero partenze: ");
            Serial.print(takeOff);    
            Serial.print(" - Numero onde: ");
            Serial.println(wave);         
            Serial.print("Miglior tempo su Onda: ");
            Serial.print(waveBestTime);       
            Serial.print(" secondi # ");
            Serial.print("Numero righe analizzate: ");
            Serial.println(count);    
            
            memset(rideFileName, 0, sizeof(rideFileName));
            data.takeOff = takeOff;
            data.wave = wave;
            data.waveBestTime = waveBestTime;
            data.rideNumber = dataFile+1;

        } else {
            Serial.println("Nessun dato disponibile per il calcolo della media.");
        }
        delay(100); 
   // }
    Serial.println("-------- FINE ANALISI DATI --------");
}

/**/
int timeDifference(String time1, String time2) {
    int min1, sec1, min2, sec2;
    sscanf(time1.c_str(), "%d:%d", &min1, &sec1);
    sscanf(time2.c_str(), "%d:%d", &min2, &sec2);
    int totalSec1 = min1 * 60 + sec1;
    int totalSec2 = min2 * 60 + sec2;
    return abs(totalSec1 - totalSec2);
}
