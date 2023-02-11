// NAME: PN5180-ReadUID.ino
//
// DESC: Example usage of the PN5180 library for the PN5180-NFC Module
//       from NXP Semiconductors.
//
// Copyright (c) 2018 by Andreas Trappmann. All rights reserved.
// Copyright (c) 2019 by Dirk Carstensen.
//
// This file is part of the PN5180 library for the Arduino environment.
//
// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public 
// License as published by the Free Software Foundation; either
// version 2.1 of the License, or (at your option) any later version.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
// Lesser General Public License for more details.
//
// BEWARE: SPI with an Arduino to a PN5180 module has to be at a level of 3.3V
// use of logic-level converters from 5V->3.3V is absolutly neccessary
// on most Arduinos for all input pins of PN5180!
// If used with an ESP-32, there is no need for a logic-level converter, since
// it operates on 3.3V already.
//
// Arduino <-> Level Converter <-> PN5180 pin mapping:
// 5V             <-->             5V
// 3.3V           <-->             3.3V
// GND            <-->             GND
// 5V      <-> HV
// GND     <-> GND (HV)
//             LV              <-> 3.3V
//             GND (LV)        <-> GND
// SCLK,13 <-> HV1 - LV1       --> SCLK
// MISO,12        <---         <-- MISO
// MOSI,11 <-> HV3 - LV3       --> MOSI
// SS,10   <-> HV4 - LV4       --> NSS (=Not SS -> active LOW)
// BUSY,9         <---             BUSY
// Reset,7 <-> HV2 - LV2       --> RST
//
// ESP-32    <--> PN5180 pin mapping:
// 3.3V      <--> 3.3V
// GND       <--> GND
// SCLK, 18   --> SCLK
// MISO, 19  <--  MISO
// MOSI, 23   --> MOSI
// SS, 16     --> NSS (=Not SS -> active LOW)
// BUSY, 5   <--  BUSY
// Reset, 17  --> RST
//

/*
 * Pins on ICODE2 Reader Writer:
 *
 *   ICODE2   |     PN5180
 * pin  label | pin  I/O  name
 * 1    +5V
 * 2    +3,3V
 * 3    RST     10   I    RESET_N (low active)
 * 4    NSS     1    I    SPI NSS
 * 5    MOSI    3    I    SPI MOSI
 * 6    MISO    5    O    SPI MISO
 * 7    SCK     7    I    SPI Clock
 * 8    BUSY    8    O    Busy Signal
 * 9    GND     9  Supply VSS - Ground
 * 10   GPIO    38   O    GPO1 - Control for external DC/DC
 * 11   IRQ     39   O    IRQ
 * 12   AUX     40   O    AUX1 - Analog/Digital test signal
 * 13   REQ     2?  I/O   AUX2 - Analog test bus or download
 *
 */


#include <PN5180.h>
#include <PN5180ISO14443.h>
#include <PN5180ISO15693.h>

#if defined(ARDUINO_AVR_UNO) || defined(ARDUINO_AVR_MEGA2560) || defined(ARDUINO_AVR_NANO)

#define PN5180_NSS  10
#define PN5180_BUSY 9
#define PN5180_RST  7

#elif defined(ARDUINO_ARCH_ESP32)

#define PN5180_NSS  16   
#define PN5180_BUSY 5  
#define PN5180_RST  17

#else
#error Please define your pinout here!
#endif

  
PN5180ISO14443 nfc14443(PN5180_NSS, PN5180_BUSY, PN5180_RST); 
PN5180ISO15693 nfc15693(PN5180_NSS, PN5180_BUSY, PN5180_RST);

void setup() {
  Serial.begin(115200);
  Serial.println(F("=================================="));
  Serial.println(F("Uploaded: " __DATE__ " " __TIME__));
  Serial.println(F("PN5180 ISO14443 Demo Sketch"));

  nfc14443.begin();

  Serial.println(F("----------------------------------"));
  Serial.println(F("PN5180 Hard-Reset..."));
  nfc14443.reset();

  Serial.println(F("----------------------------------"));
  Serial.println(F("Reading product version..."));
  uint8_t productVersion[2];
  nfc14443.readEEprom(PRODUCT_VERSION, productVersion, sizeof(productVersion));
  Serial.print(F("Product version="));
  Serial.print(productVersion[1]);
  Serial.print(".");
  Serial.println(productVersion[0]);

  if (0xff == productVersion[1]) { // if product version 255, the initialization failed
    Serial.println(F("Initialization failed!?"));
    Serial.println(F("Press reset to restart..."));
    Serial.flush();
    exit(-1); // halt
  }
  
  Serial.println(F("----------------------------------"));
  Serial.println(F("Reading firmware version..."));
  uint8_t firmwareVersion[2];
  nfc14443.readEEprom(FIRMWARE_VERSION, firmwareVersion, sizeof(firmwareVersion));
  Serial.print(F("Firmware version="));
  Serial.print(firmwareVersion[1]);
  Serial.print(".");
  Serial.println(firmwareVersion[0]);

  Serial.println(F("----------------------------------"));
  Serial.println(F("Reading EEPROM version..."));
  uint8_t eepromVersion[2];
  nfc14443.readEEprom(EEPROM_VERSION, eepromVersion, sizeof(eepromVersion));
  Serial.print(F("EEPROM version="));
  Serial.print(eepromVersion[1]);
  Serial.print(".");
  Serial.println(eepromVersion[0]);

  Serial.println(F("----------------------------------"));
  Serial.println(F("Enable RF field..."));
  nfc14443.setupRF();
}

uint32_t loopCnt = 0;


// read cards loop
void loop() {
  Serial.println(F("----------------------------------"));
  Serial.print(F("Loop #"));
  Serial.println(loopCnt++);
  #if defined(ARDUINO_ARCH_ESP32)  
    Serial.println("Free heap: " + String(ESP.getFreeHeap())); 
  #endif
  uint8_t uid[10];
  unsigned long StartTime, ElapsedTime;
  // check for ISO-14443 card
  StartTime = millis();
  nfc14443.reset();
  nfc14443.setupRF();
  if (nfc14443.isCardPresent()) {
    int8_t uidLength = nfc14443.readCardSerial(uid);
    if (uidLength > 0) {
      Serial.print(F("ISO-14443 card found, UID="));
      for (int i=0; i<uidLength; i++) {
        Serial.print(uid[i] < 0x10 ? " 0" : " ");
        Serial.print(uid[i], HEX);
      }
      Serial.println();
      Serial.println(F("----------------------------------"));
      delay(1000); 
      return;
    }
  } 
  ElapsedTime = (millis() - StartTime);
  Serial.print("time for reading ISO-14443: ");
  Serial.print(ElapsedTime);
  Serial.println(" mSec.");
  // check for ISO-15693 card
  StartTime = millis();
  nfc15693.reset();
  nfc15693.setupRF();
  // check for ICODE-SLIX2 password protected tag
  uint8_t password[] = {0x01, 0x02, 0x03, 0x04}; // put your privacy password here
  ISO15693ErrorCode myrc = nfc15693.disablePrivacyMode(password);
  if (ISO15693_EC_OK == myrc) {
    Serial.println("disablePrivacyMode successful");
  }
  // try to read ISO15693 inventory
  ISO15693ErrorCode rc = nfc15693.getInventory(uid);
  if (rc == ISO15693_EC_OK) {
    Serial.print(F("ISO-15693 card found, UID="));
    for (int i=0; i<8; i++) {
      Serial.print(uid[7-i] < 0x10 ? " 0" : " ");
      Serial.print(uid[7-i], HEX); // LSB is first
    }
    Serial.println();
    // enable privacy mode
    ISO15693ErrorCode myrc = nfc15693.enablePrivacyMode(password);
    if (ISO15693_EC_OK == myrc) {
      Serial.println("enablePrivacyMode successful");
    }
    Serial.println();
    Serial.println(F("----------------------------------"));
    delay(1000); 
    return;
  }
  ElapsedTime = (millis() - StartTime);
  Serial.print("time for reading ISO-15693: ");
  Serial.print(ElapsedTime);
  Serial.println(" mSec.");

  // no card detected
  Serial.println(F("*** No card detected!"));
}
