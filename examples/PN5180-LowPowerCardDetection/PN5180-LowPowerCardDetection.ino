// NAME: LowPowerCardDetection.ino
//
// DESC: Example usage of the PN5180 library for the PN5180-NFC Module
//       from NXP Semiconductors.
//		 This example uses the PN5180 low power card detection mode (LPCD)
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
// IRQ,6          <---             IRQ 
//
// ESP-32      <--> PN5180 pin mapping:
// 3.3V        <--> 3.3V
// 5V          <--> 3.3V
// GND         <--> GND
// SCLK, 18     --> SCLK
// MISO, 19    <--  MISO
// MOSI, 23     --> MOSI
// SS, 16       --> NSS (=Not SS -> active LOW)
// BUSY, 5     <--  BUSY
// Reset, 17    --> RST
// GPIO_NUM_34 <--- IRQ
//


#if defined(ARDUINO_ARCH_ESP32)
  #include <WiFi.h>
#endif
#include <PN5180.h>
#include <PN5180ISO14443.h>
#include <PN5180ISO15693.h>

#if defined(ARDUINO_AVR_UNO) || defined(ARDUINO_AVR_MEGA2560) || defined(ARDUINO_AVR_NANO)

#define PN5180_NSS  10
#define PN5180_BUSY 9
#define PN5180_RST  7
#define PN5180_IRQ  6

#elif defined(ARDUINO_ARCH_ESP32)

#define PN5180_NSS  16   
#define PN5180_BUSY 5  
#define PN5180_RST  17
#define PN5180_IRQ  GPIO_NUM_15

#else
#error Please define your pinout here!
#endif

#if defined(ARDUINO_ARCH_ESP32)
/*
 * Method to print the reason by which ESP32 has been awaken from sleep
 */
void print_wakeup_reason(){
  esp_sleep_wakeup_cause_t wakeup_reason;

  wakeup_reason = esp_sleep_get_wakeup_cause();

  switch(wakeup_reason)
  {
    case ESP_SLEEP_WAKEUP_EXT0 : Serial.println("Wakeup caused by external signal using RTC_IO"); break;
    case ESP_SLEEP_WAKEUP_EXT1 : Serial.println("Wakeup caused by external signal using RTC_CNTL"); break;
    case ESP_SLEEP_WAKEUP_TIMER : Serial.println("Wakeup caused by timer"); break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD : Serial.println("Wakeup caused by touchpad"); break;
    case ESP_SLEEP_WAKEUP_ULP : Serial.println("Wakeup caused by ULP program"); break;
    default : Serial.printf("Wakeup was not caused by deep sleep: %d\n",wakeup_reason); break;
 }
}
#endif


PN5180ISO14443 nfc(PN5180_NSS, PN5180_BUSY, PN5180_RST);
PN5180ISO15693 nfc15693(PN5180_NSS, PN5180_BUSY, PN5180_RST);

uint16_t sleepTimeMS = 0x3FF;

void setup() {
  Serial.begin(115200);
  Serial.println(F("=================================="));
  Serial.println(F("Uploaded: " __DATE__ " " __TIME__));
  #if defined(ARDUINO_ARCH_ESP32)
    // save power
    WiFi.mode(WIFI_OFF);
    btStop();
    // configure wakeup pin for deep-sleep mode
    esp_sleep_enable_ext0_wakeup(PN5180_IRQ, 1); //1 = High, 0 = Low

    Serial.println(F("============================================"));
    Serial.println(F("LPCD detection demo with deep-sleep on ESP32"));
    print_wakeup_reason();
    Serial.println(F("============================================"));
  #else
    Serial.println(F("PN5180 LPCD Demo Sketch"));
  #endif
  Serial.println("");   
  pinMode(PN5180_IRQ, INPUT);
  nfc.begin();
  nfc15693.begin();

  Serial.println(F("----------------------------------"));
  Serial.println(F("PN5180 Hard-Reset..."));
  nfc.reset();

  Serial.println(F("----------------------------------"));
  Serial.println(F("Reading product version..."));
  uint8_t productVersion[2];
  nfc.readEEprom(PRODUCT_VERSION, productVersion, sizeof(productVersion));
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
  nfc.readEEprom(FIRMWARE_VERSION, firmwareVersion, sizeof(firmwareVersion));
  Serial.print(F("Firmware version="));
  Serial.print(firmwareVersion[1]);
  Serial.print(".");
  Serial.println(firmwareVersion[0]);

  if (firmwareVersion[1] < 4) {
    Serial.println("This LPCD demo might work only for firmware version 4.0!!!");
  };
  
  Serial.println(F("----------------------------------"));
  Serial.println(F("Reading EEPROM version..."));
  uint8_t eepromVersion[2];
  nfc.readEEprom(EEPROM_VERSION, eepromVersion, sizeof(eepromVersion));
  Serial.print(F("EEPROM version="));
  Serial.print(eepromVersion[1]);
  Serial.print(".");
  Serial.println(eepromVersion[0]);
  Serial.println(F("----------------------------------"));
  Serial.println(F("Reading IRQ-Pin..."));
  uint8_t irqPin[1];
  nfc.readEEprom(IRQ_PIN_CONFIG, irqPin, sizeof(irqPin));
  Serial.print(F("irqPin="));
  Serial.println(irqPin[0]);

  Serial.println(F("----------------------------------"));
  Serial.println(F("start LPCD..."));


  uint8_t data[255];
  uint8_t response[256];
  // LPCD_FIELD_ON_TIME (0x36)
  uint8_t fieldOn = 0xF0;
  data[0] = fieldOn;
  nfc.writeEEprom(0x36, data, 1);
  nfc.readEEprom(0x36, response, 1);
  fieldOn = response[0];
  Serial.print("LPCD-fieldOn time: ");
  Serial.println(fieldOn, HEX);

  // LPCD_THRESHOLD (0x37)
  uint8_t threshold = 0x04;
  data[0] = threshold;
  nfc.writeEEprom(0x37, data, 1);
  nfc.readEEprom(0x37, response, 1);
  threshold = response[0];
  Serial.print("LPCD-threshold: ");
  Serial.println(threshold, HEX);

  // LPCD_REFVAL_GPO_CONTROL (0x38)
  uint8_t lpcdMode = 0x01; // 1 = LPCD SELF CALIBRATION
  data[0] = lpcdMode;
  nfc.writeEEprom(0x38, data, 1);
  nfc.readEEprom(0x38, response, 1);
  lpcdMode = response[0];
  Serial.print("lpcdMode: ");
  Serial.println(lpcdMode, HEX);
  
  // LPCD_GPO_TOGGLE_BEFORE_FIELD_ON (0x39)
  uint8_t beforeFieldOn = 0xF0; 
  data[0] = beforeFieldOn;
  nfc.writeEEprom(0x39, data, 1);
  nfc.readEEprom(0x39, response, 1);
  beforeFieldOn = response[0];
  Serial.print("beforeFieldOn: ");
  Serial.println(beforeFieldOn, HEX);
  
  // LPCD_GPO_TOGGLE_AFTER_FIELD_ON (0x3A)
  uint8_t afterFieldOn = 0xF0; 
  data[0] = afterFieldOn;
  nfc.writeEEprom(0x3A, data, 1);
  nfc.readEEprom(0x3A, response, 1);
  afterFieldOn = response[0];
  Serial.print("afterFieldOn: ");
  Serial.println(afterFieldOn, HEX);
  delay(100);
      
  // turn on LPCD in self calibration mode
  if (nfc.switchToLPCD(sleepTimeMS)) {
    Serial.println("switchToLPCD success");
  } else {
    Serial.println("switchToLPCD failed");
  }
  // ++ go to sleep ++
}

uint32_t loopCnt = 0;


// read cards loop
void loop() {
  if (digitalRead(PN5180_IRQ) == HIGH) {
    // LPCD detection irq
    showIRQStatus(nfc.getIRQStatus());
    uint32_t u;
    nfc.readRegister(0x26, &u);
    Serial.print("LPCD_REFERENCE_VALUE: ");
    Serial.println(u, HEX);
    nfc.clearIRQStatus(0xffffffff);
    nfc.reset(); 
    // try to read the UID for an ISO-14443 card
    uint8_t uid[10];
    nfc.setupRF();
    if (nfc.isCardPresent()) {
      uint8_t uidLength = nfc.readCardSerial(uid);
      if (uidLength > 0) {
        Serial.print(F("ISO14443 card found, UID="));
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
    // check for ISO-15693 card
    nfc15693.reset();
    nfc15693.setupRF();
    // check for ICODE-SLIX2 password protected tag
    uint8_t password[] = {0x01, 0x02, 0x03, 0x04};  // set your privacy unlock password here
    ISO15693ErrorCode myrc = nfc15693.disablePrivacyMode(password);
    if (ISO15693_EC_OK == myrc) {
      Serial.println("disablePrivacyMode successful");
    }
    // try to read ISO15693 inventory
    ISO15693ErrorCode rc = nfc15693.getInventory(uid);
    if (rc == ISO15693_EC_OK) {
      Serial.print(F("ISO15693 card found, UID="));
      for (int i=0; i<8; i++) {
        Serial.print(uid[7-i] < 0x10 ? " 0" : " ");
        Serial.print(uid[7-i], HEX); // LSB is first
      }
      Serial.println();
      // lock password  
      ISO15693ErrorCode myrc = nfc15693.enablePrivacyMode(password);
      if (ISO15693_EC_OK == myrc) {
        Serial.println("enablePrivacyMode successful");
      }
      Serial.println();
      Serial.println(F("----------------------------------"));
      delay(1000); 
      return;
    }
	
    // turn on LPCD
    if (nfc.switchToLPCD(sleepTimeMS)) {
      Serial.println("switchToLPCD success");
    } else {
      Serial.println("switchToLPCD failed");
    }
  }
}


void showIRQStatus(uint32_t irqStatus) {
  Serial.print(F("IRQ-Status 0x"));
  Serial.print(irqStatus, HEX);
  Serial.print(": [ ");
  if (irqStatus & (1<< 0)) Serial.print(F("RQ "));
  if (irqStatus & (1<< 1)) Serial.print(F("TX "));
  if (irqStatus & (1<< 2)) Serial.print(F("IDLE "));
  if (irqStatus & (1<< 3)) Serial.print(F("MODE_DETECTED "));
  if (irqStatus & (1<< 4)) Serial.print(F("CARD_ACTIVATED "));
  if (irqStatus & (1<< 5)) Serial.print(F("STATE_CHANGE "));
  if (irqStatus & (1<< 6)) Serial.print(F("RFOFF_DET "));
  if (irqStatus & (1<< 7)) Serial.print(F("RFON_DET "));
  if (irqStatus & (1<< 8)) Serial.print(F("TX_RFOFF "));
  if (irqStatus & (1<< 9)) Serial.print(F("TX_RFON "));
  if (irqStatus & (1<<10)) Serial.print(F("RF_ACTIVE_ERROR "));
  if (irqStatus & (1<<11)) Serial.print(F("TIMER0 "));
  if (irqStatus & (1<<12)) Serial.print(F("TIMER1 "));
  if (irqStatus & (1<<13)) Serial.print(F("TIMER2 "));
  if (irqStatus & (1<<14)) Serial.print(F("RX_SOF_DET "));
  if (irqStatus & (1<<15)) Serial.print(F("RX_SC_DET "));
  if (irqStatus & (1<<16)) Serial.print(F("TEMPSENS_ERROR "));
  if (irqStatus & (1<<17)) Serial.print(F("GENERAL_ERROR "));
  if (irqStatus & (1<<18)) Serial.print(F("HV_ERROR "));
  if (irqStatus & (1<<19)) Serial.print(F("LPCD "));
  Serial.println("]");
}
