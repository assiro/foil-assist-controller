// ======================================================
// vescConfigDump.ino — DUMP + SALVATAGGIO CONFIG VESC
// Invia COMM_GET_MCCONF, riceve la risposta, la stampa
// su seriale E la salva su LittleFS come mcconf.bin
// ======================================================

#include <LittleFS.h>

#define COMM_GET_MCCONF 14
#define COMM_GET_FW_VERSION 0

// CRC16-CCITT (usato dal protocollo VESC)
uint16_t crc16_vesc(const uint8_t *buf, uint16_t len) {
  uint16_t crc = 0;
  for (uint16_t i = 0; i < len; i++) {
    crc ^= (uint16_t)buf[i] << 8;
    for (uint8_t j = 0; j < 8; j++) {
      if (crc & 0x8000) crc = (crc << 1) ^ 0x1021;
      else crc <<= 1;
    }
  }
  return crc;
}

// ── Legge la config dal VESC e la salva su LittleFS ──
// Restituisce il numero di byte della mc_configuration (di solito 483),
// oppure 0 se fallisce.
int dumpAndSaveVescConfig() {
  McconfGuard configGuard;
  if (!configGuard.ownsLock()) return 0;
  VescUartGuard uartGuard;
  if (!uartGuard.ownsLock()) return 0;
  Serial.println();
  Serial.println(F("=== VESC DUMP ==="));

  // Svuota buffer UART
  while (Serial2.available()) Serial2.read();

  // Invia COMM_GET_MCCONF come pacchetto corto
  uint8_t payload[1] = {COMM_GET_MCCONF};
  uint16_t crc = crc16_vesc(payload, 1);
  uint8_t packet[6];
  packet[0] = 0x02;           // short packet
  packet[1] = 1;              // payload length
  packet[2] = payload[0];     // COMM_GET_MCCONF
  packet[3] = (crc >> 8) & 0xFF;
  packet[4] = crc & 0xFF;
  packet[5] = 0x03;           // end byte
  Serial2.write(packet, sizeof(packet));

  // Ricevi risposta (max 3 secondi)
  uint8_t rx[600];
  int rxLen = 0;
  unsigned long timeout = millis() + 3000;
  while (millis() < timeout) {
    while (Serial2.available() && rxLen < 600) {
      rx[rxLen++] = Serial2.read();
    }
    if (rxLen > 5 && rx[rxLen - 1] == 0x03) break;
  }

  if (rxLen == 0) {
    Serial.println(F("No response from VESC"));
    return 0;
  }

  Serial.printf("DUMP: received %d bytes\n", rxLen);

  // ── Estrai mc_configuration dal pacchetto VESC ──
  // Formato: start_byte(1) + len(2) + payload(484) + crc(2) + end(1)
  // payload[0] = COMM_GET_MCCONF echo (0x0E)
  // payload[1..483] = mc_configuration (483 byte)
  // Quindi la config inizia a rx[4] ed e' 483 byte
  
  // Verifica che sia un pacchetto valido
  if (rx[0] != 0x03) {
    Serial.println(F("DUMP: not a long packet (expected 0x03 start)"));
    return 0;
  }
  
  // Salta header pacchetto (3 byte) + comando echo (1 byte) = 4 byte
  int dataStart = 4;
  int dataLen = rxLen - dataStart - 3;  // -3 per CRC(2) + end(1)
  
  if (dataLen <= 0) {
    Serial.println(F("DUMP: invalid data length"));
    return 0;
  }
  
  Serial.printf("DUMP: mc_configuration = %d bytes\n", dataLen);

  // ── Salva su LittleFS come mcconf.bin ──
  // Crea un buffer da 1024 byte (padding con zero per compatibilita')
  uint8_t mcconfSave[1024];
  memset(mcconfSave, 0, sizeof(mcconfSave));
  
  int copyLen = min(dataLen, 1024);
  memcpy(mcconfSave, rx + dataStart, copyLen);

  LittleFS.remove("/mcconf.bin");
  File f = LittleFS.open("/mcconf.bin", FILE_WRITE);
  if (!f) {
    Serial.println(F("DUMP: cannot write mcconf.bin"));
    return 0;
  }
  f.write(mcconfSave, 1024);
  f.close();
  Serial.printf("DUMP: saved mcconf.bin (%d + padding 1024 bytes)\n", dataLen);
  invalidateWattMaxCache();   // fix 20/08/2026: il dump riscrive il file, la cache l_watt_max va invalidata

  Serial.println(F("=== DUMP END ==="));
  return dataLen;
}

// ── Legge versione firmware VESC (COMM_GET_FW_VERSION) ──
// Restituisce una stringa "major.minor" (es. "6.06") oppure "0.00" se fallisce.
String getVescFirmwareVersion() {
  VescUartGuard uartGuard;
  if (!uartGuard.ownsLock()) return "0.00";
  Serial.println();
  Serial.println(F("=== VESC FW VERSION ==="));

  // Svuota buffer UART
  while (Serial2.available()) Serial2.read();

  // Invia COMM_GET_FW_VERSION come pacchetto corto
  uint8_t payload[1] = {COMM_GET_FW_VERSION};
  uint16_t crc = crc16_vesc(payload, 1);
  uint8_t packet[6];
  packet[0] = 0x02;           // short packet
  packet[1] = 1;              // payload length
  packet[2] = payload[0];     // COMM_GET_FW_VERSION
  packet[3] = (crc >> 8) & 0xFF;
  packet[4] = crc & 0xFF;
  packet[5] = 0x03;           // end byte
  Serial2.write(packet, sizeof(packet));

  // Ricevi risposta (max 1 secondo)
  uint8_t rx[32];
  int rxLen = 0;
  unsigned long timeout = millis() + 1000;
  while (millis() < timeout) {
    while (Serial2.available() && rxLen < 32) {
      rx[rxLen++] = Serial2.read();
    }
    if (rxLen >= 5 && rx[rxLen - 1] == 0x03) break;
    delay(1);
  }

  if (rxLen < 6) {
    Serial.println(F("FW VERSION: no response from VESC"));
    return "0.00";
  }

  uint8_t major, minor;
  // Determina tipo pacchetto e trova i byte major/minor
  // Il VESC risponde con echo del comando (0x00) come primo byte payload
  // Short packet:  02 LEN ECHO MAJ MIN CRC_H CRC_L 03  -> MAJ a rx[3], MIN a rx[4]
  // Long packet:   03 00 LEN ECHO MAJ MIN CRC_H CRC_L 03 -> MAJ a rx[4], MIN a rx[5]
  if (rx[0] == 0x02) {
    // Short packet: salta start(1) + len(1) + echo(1) = 3
    major = rx[3];
    minor = rx[4];
  } else if (rx[0] == 0x03) {
    // Long packet: salta start(1) + len(2) + echo(1) = 4
    major = rx[4];
    minor = rx[5];
  } else {
    Serial.println(F("FW VERSION: unknown packet start"));
    return "0.00";
  }

  char buf[16];
  snprintf(buf, sizeof(buf), "%d.%02d", major, minor);
  Serial.printf("FW VERSION: %s (major=%d, minor=%d)\n", buf, major, minor);
  Serial.println(F("=== FW VERSION END ==="));
  return String(buf);
}

// ── Versione legacy: solo dump su seriale (chiamata da /dumpConfig) ──
void dumpVescConfiguration() {
  dumpAndSaveVescConfig();
}


