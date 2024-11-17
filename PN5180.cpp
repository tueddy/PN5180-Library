// NAME: PN5180.cpp
//
// DESC: Implementation of PN5180 class.
//
// Copyright (c) 2018 by Andreas Trappmann. All rights reserved.
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
//#define DEBUG 1
#define DEBUG_ERROR 1

#include <Arduino.h>
#include "PN5180.h"
#include "Debug.h"

// PN5180 1-Byte Direct Commands
// see 11.4.3.3 Host Interface Command List (https://www.nxp.com/docs/en/data-sheet/PN5180A0XX_C3_C4.pdf, page 20)
#define PN5180_WRITE_REGISTER                       (0x00)  // Write one 32bit register value
#define PN5180_WRITE_REGISTER_OR_MASK               (0x01)  // Sets one 32bit register value using a 32 bit OR mask
#define PN5180_WRITE_REGISTER_AND_MASK              (0x02)  // Sets one 32bit register value using a 32 bit AND mask
//#define PN5180_WRITE_REGISTER_MULTIPLE              (0x03)  // Processes an array of register addresses in random order and performs the defined action on these addresses.
#define PN5180_READ_REGISTER                        (0x04)  // Reads one 32bit register value
//#define PN5180_READ_REGISTER_MULTIPLE               (0x05)  // Reads from an array of max.18 register addresses in random order
#define PN5180_WRITE_EEPROM                         (0x06)  // Processes an array of EEPROM addresses in random order and writes the value to these addresses
#define PN5180_READ_EEPROM                          (0x07)  // Processes an array of EEPROM addresses from a start address and reads the values from these addresses
//#define PN5180_WRITE_TX_DATA                        (0x08)  // Write data into the transmission buffer
#define PN5180_SEND_DATA                            (0x09)  // Write data into the transmission buffer, the START_SEND bit is automatically set
#define PN5180_READ_DATA                            (0x0A)  // Read data from reception buffer, after successful reception
#define PN5180_SWITCH_MODE                          (0x0B)  // Switch the mode. It is only possible to switch from NormalMode to standby, LPCD or Autocoll
#define PN5180_MIFARE_AUTHENTICATE                  (0x0C)  // Perform a MIFARE Classic Authentication on an activated card
//#define PN5180_EPC_INVENTORY                        (0x0D)  // Perform an inventory of ISO18000-3M3 tags
//#define PN5180_EPC_RESUME_INVENTORY                 (0x0E)  // Resume the inventory algorithm in case it is paused
//#define PN5180_EPC_RETRIEVE_INVENTORY_RESULT_SIZE   (0x0F)  // Retrieve the size of the inventory result
//#define PN5180_EPC_RETRIEVE_INVENTORY_RESULT        (0x10)  // Retrieve the result of a preceding EPC_INVENTORY or EPC_RESUME_INVENTORY instruction
#define PN5180_LOAD_RF_CONFIG                       (0x11)  // Load the RF configuration from EEPROM into the configuration registers
//#define UPDATE_RF_CONFIG                            (0x12)  // Update the RF configuration within EEPROM.
//#define RETRIEVE_RF_CONFIG_SIZE                     (0x13)  // Retrieve the number of registers for a selected RF configuration
//#define RETRIEVE_RF_CONFIG                          (0x14)  // Read out an RF configuration. The register address-value-pairs are available in the response
//#define RFU                                         (0x15) 	// RFU (reserved for future use)
#define PN5180_RF_ON                                (0x16)  // Switch on the RF Field
#define PN5180_RF_OFF                               (0x17)  // Switch off the RF Field

#define SETRF_ON_TIMEOUT	(500)
#define SETRF_OFF_TIMEOUT	(500)

uint8_t PN5180::readBufferStatic16[16];

PN5180::PN5180(uint8_t SSpin, uint8_t BUSYpin, uint8_t RSTpin, SPIClass& spi) :
  PN5180_NSS(SSpin),
  PN5180_BUSY(BUSYpin),
  PN5180_RST(RSTpin),
  PN5180_SPI(spi),
  PN5180_SCK(-1), 
  PN5180_MISO(-1),
  PN5180_MOSI(-1)
{
  /*
   * 11.4.1 Physical Host Interface
   * The interface of the PN5180 to a host microcontroller is based on a SPI interface,
   * extended by signal line BUSY. The maximum SPI speed is 7 Mbps and fixed to CPOL
   * = 0 and CPHA = 0.
   */
  // Settings for PN5180: 7Mbps, MSB first, SPI_MODE0 (CPOL=0, CPHA=0)
  SPI_SETTINGS = SPISettings(7000000, MSBFIRST, SPI_MODE0);
}

PN5180::~PN5180() {
  if (readBufferDynamic508) {
    free(readBufferDynamic508);
  }
}

// If you specify ss parameter here it will override the SSpin specified in the class initialization
void PN5180::begin(int8_t sck, int8_t miso, int8_t mosi, int8_t ss) {
  PN5180DEBUG_PRINTF(F("PN5180::begin(sck=%d, miso=%d, mosi=%d, ss=%d)"), sck, miso, mosi, ss);
  PN5180DEBUG_PRINTLN();
  PN5180DEBUG_ENTER;
  PN5180_SCK  = sck;
  PN5180_MISO = miso;
  PN5180_MOSI = mosi;
  if (ss >= 0) PN5180_NSS = ss; // ss was specified so override any NSS from class initialization

  pinMode(PN5180_NSS, OUTPUT);
  pinMode(PN5180_BUSY, INPUT);
  pinMode(PN5180_RST, OUTPUT);

  digitalWrite(PN5180_NSS, HIGH); // disable
  digitalWrite(PN5180_RST, HIGH); // no reset

  if ((PN5180_SCK > 0) && (PN5180_MISO > 0) && (PN5180_MOSI > 0)) {
    // start SPI with custom pins
    PN5180_SPI.begin(PN5180_SCK, PN5180_MISO, PN5180_MOSI, PN5180_NSS);
    PN5180DEBUG(F("Custom SPI pinout: "));
    PN5180DEBUG(F("SS=")); PN5180DEBUG(PN5180_NSS);
    PN5180DEBUG(F(", MOSI=")); PN5180DEBUG(PN5180_MOSI);
    PN5180DEBUG(F(", MISO=")); PN5180DEBUG(PN5180_MISO);
    PN5180DEBUG(F(", SCK=")); PN5180DEBUG(PN5180_SCK);
  } else {
    // start SPI with default pINs
    PN5180_SPI.begin();
    PN5180DEBUG(F("Default SPI pinout: "));
    PN5180DEBUG(F("SS=")); PN5180DEBUG(SS);
    PN5180DEBUG(F(", MOSI=")); PN5180DEBUG(MOSI);
    PN5180DEBUG(F(", MISO=")); PN5180DEBUG(MISO);
    PN5180DEBUG(F(", SCK=")); PN5180DEBUG(SCK);
  }
  PN5180DEBUG_PRINTLN();
  PN5180DEBUG_EXIT;
}

void PN5180::end() {
  PN5180DEBUG_PRINTF(F("PN5180::end()"));
  PN5180DEBUG_PRINTLN();
  PN5180DEBUG_ENTER;
  digitalWrite(PN5180_NSS, HIGH); // disable
  PN5180_SPI.end();
  PN5180DEBUG_EXIT;
}

/*
 * WRITE_REGISTER - 0x00
 * This command is used to write a 32-bit value (little endian) to a configuration register.
 * The address of the register must exist. If the condition is not fulfilled, an exception is
 * raised.
 */
bool PN5180::writeRegister(uint8_t reg, uint32_t value) {
  PN5180DEBUG_PRINTF(F("PN5180::writeRegister(reg=%d, value=%d)"), reg, value);
  PN5180DEBUG_PRINTLN();
  PN5180DEBUG_ENTER;
  uint8_t *p = (uint8_t*)&value;

#ifdef DEBUG
  PN5180DEBUG(F("Write Register 0x"));
  PN5180DEBUG(formatHex(reg));
  PN5180DEBUG(F(", value (LSB first)=0x"));
  for (int i=0; i<4; i++) {
    PN5180DEBUG(formatHex(p[i]));
  }
  PN5180DEBUG_PRINTLN();
#endif

  /*
  For all 4 byte command parameter transfers (e.g. register values), the payload
  parameters passed follow the little endian approach (Least Significant Byte first).
   */
  uint8_t cmd[] = { PN5180_WRITE_REGISTER, reg, p[0], p[1], p[2], p[3] };

  //   transceiveCommand(cmd, sizeof(cmd));
  if (!transceiveCommand(cmd, sizeof(cmd))) {
    PN5180ERROR(F("writeRegister() failed at transceiveCommand()"));
    PN5180DEBUG_EXIT;
    return false;
  }
  
  PN5180DEBUG_EXIT;
  return true;
}

/*
 * WRITE_REGISTER_OR_MASK - 0x01
 * This command modifies the content of a register using a logical OR operation. The
 * content of the register is read and a logical OR operation is performed with the provided
 * mask. The modified content is written back to the register.
 * The address of the register must exist. If the condition is not fulfilled, an exception is
 * raised.
 */
bool PN5180::writeRegisterWithOrMask(uint8_t reg, uint32_t mask) {
  PN5180DEBUG_PRINTF(F("PN5180::writeRegisterWithOrMask(reg=%d, mask=%d)"), reg, mask);
  PN5180DEBUG_PRINTLN();
  PN5180DEBUG_ENTER;
  uint8_t *p = (uint8_t*)&mask;

#ifdef DEBUG
  PN5180DEBUG(F("Write Register 0x"));
  PN5180DEBUG(formatHex(reg));
  PN5180DEBUG(F(" with OR mask (LSB first)=0x"));
  for (int i=0; i<4; i++) {
    PN5180DEBUG(formatHex(p[i]));
  }
  PN5180DEBUG_PRINTLN();
#endif

  uint8_t cmd[] = { PN5180_WRITE_REGISTER_OR_MASK, reg, p[0], p[1], p[2], p[3] };

  //   transceiveCommand(cmd, sizeof(cmd));
  if (!transceiveCommand(cmd, sizeof(cmd))) {
    PN5180ERROR(F("writeRegisterWithOrMask() failed at transceiveCommand()"));
    PN5180DEBUG_EXIT;
    return false;
  }

  PN5180DEBUG_EXIT;
  return true;
}

/*
 * WRITE_REGISTER_AND_MASK - 0x02
 * This command modifies the content of a register using a logical AND operation. The
 * content of the register is read and a logical AND operation is performed with the provided
 * mask. The modified content is written back to the register.
 * The address of the register must exist. If the condition is not fulfilled, an exception is
 * raised.
 */
bool PN5180::writeRegisterWithAndMask(uint8_t reg, uint32_t mask) {
  PN5180DEBUG_PRINTF(F("PN5180::writeRegisterWithAndMask(reg=%d, mask=%d)"), reg, mask);
  PN5180DEBUG_PRINTLN();
  PN5180DEBUG_ENTER;
  uint8_t *p = (uint8_t*)&mask;

#ifdef DEBUG
  PN5180DEBUG(F("Write Register 0x"));
  PN5180DEBUG(formatHex(reg));
  PN5180DEBUG(F(" with AND mask (LSB first)=0x"));
  for (int i=0; i<4; i++) {
    PN5180DEBUG(formatHex(p[i]));
  }
  PN5180DEBUG_PRINTLN();
#endif

  uint8_t cmd[] = { PN5180_WRITE_REGISTER_AND_MASK, reg, p[0], p[1], p[2], p[3] };

  //   transceiveCommand(cmd, sizeof(cmd));
  if (!transceiveCommand(cmd, sizeof(cmd))) {
    PN5180ERROR(F("writeRegisterWithAndMask() failed at transceiveCommand()"));
    PN5180DEBUG_EXIT;
    return false;
  }

  PN5180DEBUG_EXIT;
  return true;
}

/*
 * READ_REGISTER - 0x04
 * This command is used to read the content of a configuration register. The content of the
 * register is returned in the 4 byte response.
 * The address of the register must exist. If the condition is not fulfilled, an exception is
 * raised.
 */
bool PN5180::readRegister(uint8_t reg, uint32_t *value) {
  PN5180DEBUG_PRINTF(F("PN5180::readRegister(reg=0x%s, *value)"), formatHex(reg));
  PN5180DEBUG_PRINTLN();
  PN5180DEBUG_ENTER;

  uint8_t cmd[] = { PN5180_READ_REGISTER, reg };

  //   transceiveCommand(cmd, sizeof(cmd), (uint8_t*)value, 4);
  if (!transceiveCommand(cmd, sizeof(cmd), (uint8_t*)value, 4)) {
    PN5180ERROR(F("readRegister() failed at transceiveCommand()"));
    PN5180DEBUG_EXIT;
    return false;
  }

  PN5180DEBUG(F("Register value=0x"));
  PN5180DEBUG(formatHex(*value));
  PN5180DEBUG_PRINTLN();

  PN5180DEBUG_EXIT;
  return true;
}

/*
 * WRITE_EEPROM - 0x06
 */
bool PN5180::writeEEprom(uint8_t addr, const uint8_t *buffer, uint8_t len) {
  PN5180DEBUG_PRINTF(F("PN5180::writeEEprom(addr=%s, *buffer, len=%d)"), formatHex(addr), len);
  PN5180DEBUG_PRINTLN();
  PN5180DEBUG_ENTER;

  uint8_t cmd[len + 2];
  cmd[0] = PN5180_WRITE_EEPROM;
  cmd[1] = addr;
  for (int i = 0; i < len; i++) cmd[2 + i] = buffer[i];

  //   transceiveCommand(cmd, len + 2);
  if (!transceiveCommand(cmd, len + 2)) {
    PN5180ERROR(F("writeEEprom() failed at transceiveCommand()"));
    PN5180DEBUG_EXIT;
    return false;
  }

  PN5180DEBUG_EXIT;
  return true;
}

/*
 * READ_EEPROM - 0x07
 * This command is used to read data from EEPROM memory area. The field 'Address'
 * indicates the start address of the read operation. The field Length indicates the number
 * of bytes to read. The response contains the data read from EEPROM (content of the
 * EEPROM); The data is read in sequentially increasing order starting with the given
 * address.
 * EEPROM Address must be in the range from 0 to 254, inclusive. Read operation must
 * not go beyond EEPROM address 254. If the condition is not fulfilled, an exception is
 * raised.
 */
bool PN5180::readEEprom(uint8_t addr, uint8_t *buffer, int len) {
  PN5180DEBUG_PRINTF(F("PN5180::readEEprom(addr=%s, *buffer, len=%d)"), formatHex(addr), len);
  PN5180DEBUG_PRINTLN();
  PN5180DEBUG_ENTER;
  
  if ((addr > 254) || ((addr+len) > 254)) {
    PN5180ERROR(F("readEEprom() failed: Reading beyond addr 254!"));
    PN5180DEBUG_EXIT;
    return false;
  }

  PN5180DEBUG(F("Reading EEPROM at 0x"));
  PN5180DEBUG(formatHex(addr));
  PN5180DEBUG(F(", size="));
  PN5180DEBUG(len);
  PN5180DEBUG_PRINTLN(F("..."));

  uint8_t cmd[] = { PN5180_READ_EEPROM, addr, uint8_t(len) };

  //   transceiveCommand(cmd, sizeof(cmd), buffer, len);
  if (!transceiveCommand(cmd, sizeof(cmd), buffer, len)) {
    PN5180ERROR(F("readEEprom() failed at transceiveCommand()"));
    PN5180DEBUG_EXIT;
    return false;
  }

#ifdef DEBUG
  PN5180DEBUG(F("EEPROM values: "));
  for (int i=0; i<len; i++) {
    PN5180DEBUG(formatHex(buffer[i]));
    PN5180DEBUG(" ");
  }
  PN5180DEBUG_PRINTLN();
#endif

  PN5180DEBUG_EXIT;
  return true;
}

/*
 * SEND_DATA - 0x09
 * This command writes data to the RF transmission buffer and starts the RF transmission.
 * The parameter ‘Number of valid bits in last Byte’ indicates the exact number of bits to be
 * transmitted for the last byte (for non-byte aligned frames).
 * Precondition: Host shall configure the Transceiver by setting the register
 * SYSTEM_CONFIG.COMMAND to 0x3 before using the SEND_DATA command, as
 * the command SEND_DATA is only writing data to the transmission buffer and starts the
 * transmission but does not perform any configuration.
 * The size of ‘Tx Data’ field must be in the range from 0 to 260, inclusive (the 0 byte length
 * allows a symbol only transmission when the TX_DATA_ENABLE is cleared).‘Number of
 * valid bits in last Byte’ field must be in the range from 0 to 7. The command must not be
 * called during an ongoing RF transmission. Transceiver must be in ‘WaitTransmit’ state
 * with ‘Transceive’ command set. If the condition is not fulfilled, an exception is raised.
 */
bool PN5180::sendData(const uint8_t *data, int len, uint8_t validBits) {
  PN5180DEBUG_PRINTF(F("PN5180::sendData(*data, len=%d, validBits=%d)"), len, validBits);
  PN5180DEBUG_PRINTLN();
  PN5180DEBUG_ENTER;
  
  if (len > 260) {
    PN5180ERROR(F("sendData() failed: more than 260 bytes is not supported!"));
    PN5180DEBUG_EXIT;
    return false;
  }

#ifdef DEBUG
  PN5180DEBUG(F("Send data (len="));
  PN5180DEBUG(len);
  PN5180DEBUG(F("):"));
  for (int i=0; i<len; i++) {
    PN5180DEBUG(" ");
    PN5180DEBUG(formatHex(data[i]));
  }
  PN5180DEBUG_PRINTLN();
#endif

  //uint8_t buffer[len+2];
  //buffer[0] = PN5180_SEND_DATA;
  //buffer[1] = validBits; // number of valid bits of last byte are transmitted (0 = all bits are transmitted)
  //for (int i=0; i<len; i++) {
  //  buffer[2+i] = data[i];
  //}
  //   writeRegisterWithAndMask(SYSTEM_CONFIG, 0xfffffff8);  // Idle/StopCom Command
  if (!writeRegisterWithAndMask(SYSTEM_CONFIG, 0xfffffff8)) {
    PN5180ERROR(F("sendData() failed at writeRegisterWithAndMask() Idle/StopCom Command"));
    PN5180DEBUG_EXIT;
    return false;
  }

  //   writeRegisterWithOrMask(SYSTEM_CONFIG, 0x00000003);   // Transceive Command
  if (!writeRegisterWithOrMask(SYSTEM_CONFIG, 0x00000003)) {
    PN5180ERROR(F("sendData() failed at writeRegisterWithOrMask() Transceive Command"));
    PN5180DEBUG_EXIT;
    return false;
  }

  /*
   * Transceive command; initiates a transceive cycle.
   * Note: Depending on the value of the Initiator bit, a
   * transmission is started or the receiver is enabled
   * Note: The transceive command does not finish
   * automatically. It stays in the transceive cycle until
   * stopped via the IDLE/StopCom command
   */

  PN5180TransceiveStat transceiveState = getTransceiveState();
  if (PN5180_TS_WaitTransmit != transceiveState) {
    PN5180ERROR(F("sendData() failed: Transceiver not in state WaitTransmit!?"));
    PN5180DEBUG_EXIT;
    return false;
  }

  //bool ret = transceiveCommand(buffer, len+2);
  if (!cmd_SendData(data, len, validBits)) {
    PN5180ERROR(F("sendData() failed at cmd_SendData()"));
    PN5180DEBUG_EXIT;
    return false;
  }
  
  PN5180DEBUG_EXIT;
  return true;
}

/*
 * READ_DATA - 0x0A
 * This command reads data from the RF reception buffer, after a successful reception.
 * The RX_STATUS register contains the information to verify if the reception had been
 * successful. The data is available within the response of the command. The host controls
 * the number of bytes to be read via the SPI interface.
 * The RF data had been successfully received. In case the instruction is executed without
 * preceding an RF data reception, no exception is raised but the data read back from the
 * reception buffer is invalid. If the condition is not fulfilled, an exception is raised.
 */
uint8_t * PN5180::readData(int len) {
  PN5180DEBUG_PRINTF(F("PN5180::readData(len=%d)"), len);
  PN5180DEBUG_PRINTLN();
  PN5180DEBUG_ENTER;
  
  if (len < 0 || len > 508) {
    Serial.println(F("readData() failed: Reading more than 508 bytes is not supported!"));
    PN5180DEBUG_EXIT;
    return 0L;
  }

  PN5180DEBUG(F("Reading Data (len="));
  PN5180DEBUG(len);
  PN5180DEBUG_PRINTLN(F(")..."));

  uint8_t cmd[] = { PN5180_READ_DATA, 0x00 };

  uint8_t *readBuffer;
  if (len <=16) {
    // use a smaller static buffer, e.g. if reading the uid only
    readBuffer = readBufferStatic16;
  } else {
    // allocate the max buffer length of 508 bytes
    if (!readBufferDynamic508) {
       readBufferDynamic508 = (uint8_t *) malloc(508);
       if (!readBufferDynamic508) {
        PN5180ERROR(F("readData() failed: Cannot allocate the read buffer of 508 Bytes!"));
        PN5180DEBUG_EXIT;
        return 0;
       }
    }
    readBuffer = readBufferDynamic508;
  }

  //   transceiveCommand(cmd, sizeof(cmd), readBuffer, len);
  if (!transceiveCommand(cmd, sizeof(cmd), readBuffer, len)) {
    PN5180ERROR(F("readData() failed at transceiveCommand()"));
    PN5180DEBUG_EXIT;
    return 0L;
  }

#ifdef DEBUG
  PN5180DEBUG(F("Data read: "));
  for (int i=0; i<len; i++) {
    PN5180DEBUG(formatHex(readBuffer[i]));
    PN5180DEBUG(" ");
  }
  PN5180DEBUG_PRINTLN();
#endif

  PN5180DEBUG_EXIT;
  return readBuffer;
}

bool PN5180::readData(int len, uint8_t *buffer) {
  PN5180DEBUG_PRINTF(F("PN5180::readData(len=%d, *buffer)"), len);
  PN5180DEBUG_PRINTLN();
  PN5180DEBUG_ENTER;
  
  if (len < 0 || len > 508) {
    PN5180DEBUG_EXIT;
    return false;
  }
  uint8_t cmd[] = { PN5180_READ_DATA, 0x00 };
  
  //bool ret = transceiveCommand(cmd, sizeof(cmd), buffer, len);
  if (!transceiveCommand(cmd, sizeof(cmd), buffer, len)) {
    PN5180ERROR(F("sendData() failed at writeRegisterWithAndMask() Idle/StopCom Command"));
    PN5180DEBUG_EXIT;
    return false;
  }

  PN5180DEBUG_EXIT;
  return true;
}

/* prepare LPCD registers (Low Power Card Detection) */
bool PN5180::prepareLPCD() {
  //=======================================LPCD CONFIG================================================================================
  PN5180DEBUG(F("----------------------------------"));
  PN5180DEBUG(F("prepare LPCD..."));

  uint8_t data[255];
  uint8_t response[256];
    //1. Set Fieldon time                                           LPCD_FIELD_ON_TIME (0x36)
  uint8_t fieldOn = 0xF0;//0x## -> ##(base 10) x 8μs + 62 μs
  data[0] = fieldOn;
  writeEEprom(0x36, data, 1);
  readEEprom(0x36, response, 1);
  fieldOn = response[0];
  PN5180DEBUG("LPCD-fieldOn time: ");
  PN5180DEBUG(formatHex(fieldOn));

    //2. Set threshold level                                         AGC_LPCD_THRESHOLD @ EEPROM 0x37
  uint8_t threshold = 0x03;
  data[0] = threshold;
  writeEEprom(0x37, data, 1);
  readEEprom(0x37, response, 1);
  threshold = response[0];
  PN5180DEBUG("LPCD-threshold: ");
  PN5180DEBUG(formatHex(threshold));

  //3. Select LPCD mode                                               LPCD_REFVAL_GPO_CONTROL (0x38)
  uint8_t lpcdMode = 0x01; // 1 = LPCD SELF CALIBRATION 
                           // 0 = LPCD AUTO CALIBRATION (this mode does not work, should look more into it, no reason why it shouldn't work)
  data[0] = lpcdMode;
  writeEEprom(0x38, data, 1);
  readEEprom(0x38, response, 1);
  lpcdMode = response[0];
  PN5180DEBUG("lpcdMode: ");
  PN5180DEBUG(formatHex(lpcdMode));
  
  // LPCD_GPO_TOGGLE_BEFORE_FIELD_ON (0x39)
  uint8_t beforeFieldOn = 0xF0; 
  data[0] = beforeFieldOn;
  writeEEprom(0x39, data, 1);
  readEEprom(0x39, response, 1);
  beforeFieldOn = response[0];
  PN5180DEBUG("beforeFieldOn: ");
  PN5180DEBUG(formatHex(beforeFieldOn));
  
  // LPCD_GPO_TOGGLE_AFTER_FIELD_ON (0x3A)
  uint8_t afterFieldOn = 0xF0; 
  data[0] = afterFieldOn;
  writeEEprom(0x3A, data, 1);
  readEEprom(0x3A, response, 1);
  afterFieldOn = response[0];
  PN5180DEBUG("afterFieldOn: ");
  PN5180DEBUG(formatHex(afterFieldOn));
  delay(100);
  return true;
}

/* switch the mode to LPCD (low power card detection)
 * Parameter 'wakeupCounterInMs' must be in the range from 0x0 - 0xA82
 * max. wake-up time is 2960 ms.
 */
bool PN5180::switchToLPCD(uint16_t wakeupCounterInMs) {
  // clear all IRQ flags
  clearIRQStatus(0xffffffff); 
  // enable only LPCD and general error IRQ
  writeRegister(IRQ_ENABLE, LPCD_IRQ_STAT | GENERAL_ERROR_IRQ_STAT);  
  // switch mode to LPCD 
  uint8_t cmd[] = { PN5180_SWITCH_MODE, 0x01, (uint8_t)(wakeupCounterInMs & 0xFF), (uint8_t)((wakeupCounterInMs >> 8U) & 0xFF) };
  return transceiveCommand(cmd, sizeof(cmd));
}

/*
 * MIFARE_AUTHENTICATE - 0x0C
 * This command is used to perform a MIFARE Classic Authentication on an activated card.
 * It takes the key, card UID and the key type to authenticate at a given block address. The
 * response contains 1 byte indicating the authentication status.
*/
int16_t PN5180::mifareAuthenticate(uint8_t blockNo, const uint8_t *key, uint8_t keyType, const uint8_t *uid) {
  if (keyType != 0x60 && keyType != 0x61){
    PN5180ERROR(F("invalid key type supplied!"));
    return -2;
  }

  uint8_t cmdBuffer[13];
  uint8_t rcvBuffer[1] = {0x02};
  cmdBuffer[0] = PN5180_MIFARE_AUTHENTICATE;  // PN5180 MF Authenticate command
  for (int i=0;i<6;i++){
    cmdBuffer[i+1] = key[i];
  }
  cmdBuffer[7] = keyType;
  cmdBuffer[8] = blockNo;
  for (int i=0;i<4;i++){
    cmdBuffer[9+i] = uid[i];
  }

  //bool retval = transceiveCommand(cmdBuffer, 13, rcvBuffer, 1);
  if (!transceiveCommand(cmdBuffer, 13, rcvBuffer, 1)) {
    PN5180ERROR(F("mifareAuthenticate() failed at transceiveCommand()"));
    PN5180DEBUG_EXIT;
    return -3;
  }
  
  return rcvBuffer[0];

}

/*
 * LOAD_RF_CONFIG - 0x11
 * Parameter 'Transmitter Configuration' must be in the range from 0x0 - 0x1C, inclusive. If
 * the transmitter parameter is 0xFF, transmitter configuration is not changed.
 * Field 'Receiver Configuration' must be in the range from 0x80 - 0x9C, inclusive. If the
 * receiver parameter is 0xFF, the receiver configuration is not changed. If the condition is
 * not fulfilled, an exception is raised.
 * The transmitter and receiver configuration shall always be configured for the same
 * transmission/reception speed. No error is returned in case this condition is not taken into
 * account.
 *
 * Transmitter: RF   Protocol          Speed     Receiver: RF    Protocol    Speed
 * configuration                       (kbit/s)  configuration               (kbit/s)
 * byte (hex)                                    byte (hex)
 * ----------------------------------------------------------------------------------------------
 * ->0D              ISO 15693 ASK100  26        8D              ISO 15693   26
 *   0E              ISO 15693 ASK10   26        8E              ISO 15693   53
 */
bool PN5180::loadRFConfig(uint8_t txConf, uint8_t rxConf) {
  PN5180DEBUG(F("Load RF-Config: txConf="));
  PN5180DEBUG(formatHex(txConf));
  PN5180DEBUG(F(", rxConf="));
  PN5180DEBUG(formatHex(rxConf));
  PN5180DEBUG_PRINTLN();
  PN5180DEBUG_ENTER;

  uint8_t cmd[] = { PN5180_LOAD_RF_CONFIG, txConf, rxConf };

  //  transceiveCommand(cmd, sizeof(cmd));
  if (!transceiveCommand(cmd, sizeof(cmd))) {
    PN5180ERROR(F("loadRFConfig() failed at transceiveCommand()"));
    PN5180DEBUG_EXIT;
    return false;
  }

  PN5180DEBUG_EXIT;
  return true;
}

/*
 * RF_ON - 0x16
 * This command is used to switch on the internal RF field. If enabled the TX_RFON_IRQ is
 * set after the field is switched on.
 */
bool PN5180::setRF_on() {
  PN5180DEBUG_PRINTLN(F("PN5180::setRF_on()"));
  PN5180DEBUG_ENTER;

  //uint8_t cmd[] = { PN5180_RF_ON, 0x00 };
  //transceiveCommand(cmd, sizeof(cmd));
  if (!cmd_RfOn(0)) {
    PN5180ERROR(F("setRF_on() failed at cmd_RfOn()"));
    PN5180DEBUG_EXIT;
    return false;
  }

  PN5180DEBUG_PRINTF(F("wait for RF field to set up (max %d ms)"), SETRF_ON_TIMEOUT);
  PN5180DEBUG_PRINTLN();
  PN5180DEBUG_OFF;
  //while (0 == (TX_RFON_IRQ_STAT & getIRQStatus())) {   // wait for RF field to set up (max 500ms)
  //  if (millis() - startedWaiting > SETRF_ON_TIMEOUT) {
  //    PN5180DEBUG_ON;
  //    PN5180ERROR(F("setRF_on() timeout waiting for TX_RFON_IRQ_STAT"));
  //    PN5180DEBUG_EXIT;
  //    return false;
  //  }
  //};
  unsigned long startedWaiting = millis();
  uint32_t irqStatus;
  while (1) {   // wait for RF field to shut down
    if (!readRegister(IRQ_STATUS, &irqStatus)) {
      PN5180DEBUG_ON;
      PN5180ERROR(F("setRF_off() failed at readRegister()"));
      PN5180DEBUG_EXIT;
      return false;
    }
	if (0 != (TX_RFON_IRQ_STAT & irqStatus)) break;
    if (millis() - startedWaiting > SETRF_ON_TIMEOUT) {
      PN5180DEBUG_ON;
      PN5180ERROR(F("setRF_on() timeout failed waiting for SETRF_ON_TIMEOUT"));
      PN5180DEBUG_EXIT;
      return false; 
    }
  }
  PN5180DEBUG_ON;
  
  //   clearIRQStatus(TX_RFON_IRQ_STAT);
  if (!clearIRQStatus(TX_RFON_IRQ_STAT)) {
    PN5180ERROR(F("setRF_on() failed at clearIRQStatus()"));
    PN5180DEBUG_EXIT;
    return false;
  }

  PN5180DEBUG_EXIT;
  return true;
}

/*
 * RF_OFF - 0x17
 * This command is used to switch off the internal RF field. If enabled, the TX_RFOFF_IRQ
 * is set after the field is switched off.
 */
bool PN5180::setRF_off() {
  PN5180DEBUG_PRINTLN(F("PN5180::setRF_off()"));
  PN5180DEBUG_ENTER;

  if (!cmd_RfOff(0)) {
    PN5180ERROR(F("setRF_off() failed at cmd_RfOff()"));
    PN5180DEBUG_EXIT;
    return false;
  }

  PN5180DEBUG_PRINTF(F("wait for RF field to shut down (max %d ms)"), SETRF_OFF_TIMEOUT);
  PN5180DEBUG_PRINTLN();
  PN5180DEBUG_OFF;
  unsigned long startedWaiting = millis();
  uint32_t irqStatus;
  while (1) {   // wait for RF field to shut down
    if (!readRegister(IRQ_STATUS, &irqStatus)) {
      PN5180DEBUG_ON;
      PN5180ERROR(F("setRF_off() failed at readRegister()"));
      PN5180DEBUG_EXIT;
      return false;
    }
	if (0 != (TX_RFOFF_IRQ_STAT & irqStatus)) break;
    if (millis() - startedWaiting > SETRF_OFF_TIMEOUT) {
      PN5180DEBUG_ON;
      PN5180ERROR(F("setRF_off() timeout failed waiting for TX_RFOFF_IRQ_STAT"));
      PN5180DEBUG_EXIT;
      return false; 
    }
  }

  //   clearIRQStatus(TX_RFOFF_IRQ_STAT);
  if (!clearIRQStatus(TX_RFOFF_IRQ_STAT)) {
    PN5180ERROR(F("setRF_on() failed at clearIRQStatus()"));
    PN5180DEBUG_EXIT;
    return false;
  }

  PN5180DEBUG_EXIT;
  return true;
}

//---------------------------------------------------------------------------------------------

/*
11.4.3.1 A Host Interface Command consists of either 1 or 2 SPI frames depending whether the
host wants to write or read data from the PN5180. An SPI Frame consists of multiple
bytes.

All commands are packed into one SPI Frame. An SPI Frame consists of multiple bytes.
No NSS toggles allowed during sending of an SPI frame.

For all 4 byte command parameter transfers (e.g. register values), the payload
parameters passed follow the little endian approach (Least Significant Byte first).

Direct Instructions are built of a command code (1 Byte) and the instruction parameters
(max. 260 bytes). The actual payload size depends on the instruction used.
Responses to direct instructions contain only a payload field (no header).
All instructions are bound to conditions. If at least one of the conditions is not fulfilled, an exception is
raised. In case of an exception, the IRQ line of PN5180 is asserted and corresponding interrupt
status register contain information on the exception.
*/

/*
 * A Host Interface Command consists of either 1 or 2 SPI frames depending whether the
 * host wants to write or read data from the PN5180. An SPI Frame consists of multiple
 * bytes.
 * All commands are packed into one SPI Frame. An SPI Frame consists of multiple bytes.
 * No NSS toggles allowed during sending of an SPI frame.
 * For all 4 byte command parameter transfers (e.g. register values), the payload
 * parameters passed follow the little endian approach (Least Significant Byte first).
 * The BUSY line is used to indicate that the system is BUSY and cannot receive any data
 * from a host. Recommendation for the BUSY line handling by the host:
 * 1. Assert NSS to Low
 * 2. Perform Data Exchange
 * 3. Wait until BUSY is high
 * 4. Deassert NSS
 * 5. Wait until BUSY is low
 * If there is a parameter error, the IRQ is set to ACTIVE and a GENERAL_ERROR_IRQ is set.
 */
bool PN5180::transceiveCommand(uint8_t *sendBuffer, size_t sendBufferLen, uint8_t *recvBuffer, size_t recvBufferLen) {
  PN5180DEBUG_PRINTF(F("PN5180::transceiveCommand(*sendBuffer, sendBufferLen=%d, *recvBuffer, recvBufferLen=%d)"), sendBufferLen, recvBufferLen);
  PN5180DEBUG_PRINTLN();
  PN5180DEBUG_ENTER;
  
  PN5180_SPI.beginTransaction(SPI_SETTINGS);
#ifdef DEBUG
  PN5180DEBUG(F("Sending SPI frame: '"));
  for (uint8_t i=0; i<sendBufferLen; i++) {
    if (i>0) { PN5180DEBUG(" "); }
    PN5180DEBUG(formatHex(sendBuffer[i]));
  }
  PN5180DEBUG_PRINTLN("'");
#endif

  // 0.
  unsigned long startedWaiting = millis();
  while (LOW != digitalRead(PN5180_BUSY)) {
    if (millis() - startedWaiting > commandTimeout) {
      PN5180ERROR("transceiveCommand() timeout waiting for BUSY=LOW (send/0)");
      PN5180_SPI.endTransaction();
      digitalWrite(PN5180_NSS, HIGH);
      PN5180DEBUG_EXIT;
      return false;
    };
  }; // wait until busy is low
  // 1.
  digitalWrite(PN5180_NSS, LOW); delay(1);
  // 2.
  PN5180_SPI.transfer((uint8_t*)sendBuffer, sendBufferLen);  
  // 3.
  startedWaiting = millis();
  while (HIGH != digitalRead(PN5180_BUSY)) {
    if (millis() - startedWaiting > commandTimeout) {
      PN5180ERROR("transceiveCommand() timeout waiting for BUSY=HIGH (send/3)");
      PN5180_SPI.endTransaction();
      digitalWrite(PN5180_NSS, HIGH);
      PN5180DEBUG_EXIT;
      return false;
    };
  }; // wait until busy is high
  // 4.
  digitalWrite(PN5180_NSS, HIGH); delay(1);
  // 5.
  startedWaiting = millis();
  while (LOW != digitalRead(PN5180_BUSY)) {
    if (millis() - startedWaiting > commandTimeout) {
      PN5180ERROR("transceiveCommand() timeout waiting for BUSY=LOW (send/5)");
      PN5180_SPI.endTransaction();
      digitalWrite(PN5180_NSS, HIGH);
      PN5180DEBUG_EXIT;
      return false;
    };
  }; // wait until busy is low

  // check, if write-only
  if ((0 == recvBuffer) || (0 == recvBufferLen)) {
    PN5180_SPI.endTransaction();
    digitalWrite(PN5180_NSS, HIGH);
    PN5180DEBUG_EXIT;
    return true;
  }
  PN5180DEBUG_PRINTLN(F("Receiving SPI frame..."));

  // 1.
  digitalWrite(PN5180_NSS, LOW); 
  // 2.
  memset(recvBuffer, 0xFF, recvBufferLen);
  PN5180_SPI.transfer(recvBuffer, recvBufferLen);
  // 3.
  startedWaiting = millis(); //delay(1);
  while (HIGH != digitalRead(PN5180_BUSY)) {
    if (millis() - startedWaiting > commandTimeout) {
      PN5180ERROR("transceiveCommand() timeout waiting for BUSY=HIGH (receive/3)");
      PN5180_SPI.endTransaction();
      digitalWrite(PN5180_NSS, HIGH);
      PN5180DEBUG_EXIT;
      return false;
    };
  }; // wait until busy is high
  // 4.
  digitalWrite(PN5180_NSS, HIGH); 
  // 5.
  startedWaiting = millis();
  while (LOW != digitalRead(PN5180_BUSY)) {
    if (millis() - startedWaiting > commandTimeout) {
      PN5180ERROR("transceiveCommand() timeout waiting for BUSY=LOW (receive/5)");
      PN5180_SPI.endTransaction();
      digitalWrite(PN5180_NSS, HIGH);
      PN5180DEBUG_EXIT;
      return false;
    };
  }; // wait until busy is low

#ifdef DEBUG
  PN5180DEBUG(F("Received: '"));
  for (uint8_t i=0; i<recvBufferLen; i++) {
    if (i > 0) PN5180DEBUG(" ");
    PN5180DEBUG(formatHex(recvBuffer[i]));
  }
  PN5180DEBUG_PRINTLN("'");
#endif
  PN5180_SPI.endTransaction();
  PN5180DEBUG_EXIT;
  return true;
}

/*
 * Reset NFC device
 */
void PN5180::reset() {
  PN5180DEBUG_PRINTLN(F("PN5180::reset()"));
  PN5180DEBUG_ENTER;
  digitalWrite(PN5180_RST, LOW);  // at least 10us required
  delay(1);
  digitalWrite(PN5180_RST, HIGH); // 2ms to ramp up required
  delay(5);

  unsigned long startedWaiting = millis();
  PN5180DEBUG_PRINTF(F("wait for system to start up (%d ms)"), commandTimeout);
  PN5180DEBUG_PRINTLN();
  PN5180DEBUG_OFF;
  while (0 == (IDLE_IRQ_STAT & getIRQStatus())) {   // wait for system to start up (with timeout)
    if (millis() - startedWaiting > commandTimeout) {
      PN5180DEBUG_ON;
      PN5180ERROR(F("reset() timeout waiting for IDLE_IRQ_STAT"));
      // try again with larger time
      digitalWrite(PN5180_RST, LOW);  
      delay(10);
      digitalWrite(PN5180_RST, HIGH); 
      delay(50);
      PN5180DEBUG_EXIT;
      return;
    }
  }
  PN5180DEBUG_ON;
  PN5180DEBUG_EXIT;
}

/**
 * @name  getInterrupt
 * @desc  read interrupt status register and clear interrupt status
 */
uint32_t PN5180::getIRQStatus() {
  PN5180DEBUG_PRINTLN(F("PN5180::getIRQStatus()"));
  PN5180DEBUG_ENTER;

  PN5180DEBUG_PRINTLN(F("Read IRQ-Status register..."));
  uint32_t irqStatus;

  //   readRegister(IRQ_STATUS, &irqStatus);
  if (!readRegister(IRQ_STATUS, &irqStatus)) {
    PN5180ERROR(F("getIRQStatus() failed at readRegister()"));
    PN5180DEBUG_EXIT;
    return 0L;
  }

  PN5180DEBUG(F("IRQ-Status=0x"));
  PN5180DEBUG(formatHex(irqStatus));
  PN5180DEBUG_PRINTLN();

  PN5180DEBUG_EXIT;
  return irqStatus;
}

bool PN5180::clearIRQStatus(uint32_t irqMask) {
  PN5180DEBUG_PRINTF(F("PN5180::clearIRQStatus(mask=%s)"),formatHex(irqMask));
  PN5180DEBUG_PRINTLN();
  PN5180DEBUG_ENTER;

  PN5180DEBUG_PRINTLN(F("Clear IRQ-Status with mask"));

  //bool ret = writeRegister(IRQ_CLEAR, irqMask);
  if (!writeRegister(IRQ_CLEAR, irqMask)) {
    PN5180ERROR(F("clearIRQStatus() failed at writeRegister()"));
    PN5180DEBUG_EXIT;
    return false;
  }

  PN5180DEBUG_EXIT;
  return true;
}

#ifdef DEBUG
extern void showIRQStatus(uint32_t);
#endif


/*
 * Get TRANSCEIVE_STATE from RF_STATUS register
 */
PN5180TransceiveStat PN5180::getTransceiveState() {
  PN5180DEBUG_PRINT(F("PN5180::getTransceiveState()"));
  PN5180DEBUG_PRINTLN();
  PN5180DEBUG_ENTER;

  PN5180DEBUG_PRINTLN(F("Get Transceive state..."));

  uint32_t rfStatus;
  PN5180TransceiveStat ret;
  if (!readRegister(RF_STATUS, &rfStatus)) {
#ifdef DEBUG
    showIRQStatus(getIRQStatus());
#endif
    PN5180ERROR(F("getTransceiveState() failed reading RF_STATUS register."));
    ret = PN5180TransceiveStat(0);
    PN5180DEBUG_EXIT;
    return ret;
  }

  /*
   * TRANSCEIVE_STATEs:
   *  0 - idle
   *  1 - wait transmit
   *  2 - transmitting
   *  3 - wait receive
   *  4 - wait for data
   *  5 - receiving
   *  6 - loopback
   *  7 - reserved
   */
  uint8_t state = ((rfStatus >> 24) & 0x07);
  PN5180DEBUG(F("TRANSCEIVE_STATE=0x"));
  PN5180DEBUG(formatHex(state));
  PN5180DEBUG_PRINTLN();

  ret = PN5180TransceiveStat(state);
  PN5180DEBUG_EXIT;
  return ret;
}

/*
 * SEND_DATA - 0x09
 * This instruction is used to write data into the transmission buffer, the START_SEND bit is automatically set.
 *
 * Payload       Length(byte)    Value/Description
 * Command code  1               0x09
 * Parameter     1               Number of valid bits in last Byte
 *               1-260           Array of up to 260 elements {Transmit data}
 *                               1 Byte Transmit Data
 * Response:     -               -
 *
 * This command writes data to the RF transmission buffer and starts the RF transmission.
 * The parameter ‘Number of valid bits in last Byte’ indicates the exact number of bits to be
 * transmitted for the last byte (for non-byte aligned frames).
 *
 * Precondition: Host shall configure the Transceiver by setting the register
 * SYSTEM_CONFIG.COMMAND to 0x3 before using the SEND_DATA command, as
 * the command SEND_DATA is only writing data to the transmission buffer and starts the
 * transmission but does not perform any configuration.
 *
 * Parameter: 'valid bits in last byte'
 *
 * Note: When the command terminates, the transmission might still be ongoing, i.e. the command starts the
 * transmission but does not wait for the end of transmission.
 *
 * Condition: The size of ‘Tx Data’ field must be in the range from 0 to 260, inclusive (the 0 byte length
 * allows a symbol only transmission when the TX_DATA_ENABLE is cleared).‘Number of
 * valid bits in last Byte’ field must be in the range from 0 to 7. The command must not be
 * called during an ongoing RF transmission. Transceiver must be in ‘WaitTransmit’ state
 * with ‘Transceive’ command set. If the condition is not fulfilled, an exception is raised.
 *
 * Returns: true on success
 */
bool PN5180::cmd_SendData(const uint8_t *data, int len, uint8_t validBits) {
  PN5180DEBUG_PRINTF(F("PN5180::cmd_sendData(*data, len=%d, validBits=%d)"), len, validBits);
  PN5180DEBUG_PRINTLN();
  
  uint8_t buffer[len+2];
  buffer[0] = PN5180_SEND_DATA;
  buffer[1] = validBits; // number of valid bits of last byte are transmitted (0 = all bits are transmitted)
  for (int i=0; i<len; i++) {
    buffer[2+i] = data[i];
  }

  if (!transceiveCommand(buffer, len+2)) {
    PN5180ERROR(F("sendData() failed at transceiveCommand()"));
    PN5180DEBUG_EXIT;
    return false;
  }
  
  PN5180DEBUG_EXIT;
  return true;
}

/*
 * RF_ON - 0x16
 * This instruction switch on the RF Field
 *
 * Payload       Length(byte)    Value/Description
 * Command code  1               0x16
 * Parameter     1               Bit0 == 1: disable collision avoidance according to ISO18092
 *                               Bit1 == 1: Use Active Communication mode according to ISO18092
 * Response:     -               -
 *
 * This command is used to switch on the internal RF field. If enabled the TX_RFON_IRQ is
 * set after the field is switched on.
 *
 * Returns: true on success
 */
bool PN5180::cmd_RfOn(uint8_t parameter) {
  PN5180DEBUG_PRINTLN(F("PN5180::cmd_RfOn()"));
  PN5180DEBUG_ENTER;

  uint8_t cmd[] = { PN5180_RF_ON, parameter };

  if (!transceiveCommand(cmd, sizeof(cmd))) {
    PN5180ERROR(F("cmd_RfOn() failed at transceiveCommand()"));
    PN5180DEBUG_EXIT;
    return false;
  }

  PN5180DEBUG_EXIT;
  return true;
}

/*
 * RF_OFF - 0x17
 * This instruction switch off the RF Field	
 *
 * Payload       Length(byte)    Value/Description
 * Command code  1               0x17
 * Parameter     1               dummy byte, any value accepted
 * Response:     -               -
 *
 * This command is used to switch off the internal RF field. If enabled, the TX_RFOFF_IRQ is set after the field is
 * switched off.
 *
 * Returns: true on success
 */
bool PN5180::cmd_RfOff(uint8_t parameter) {
  PN5180DEBUG_PRINTLN(F("PN5180::cmd_RfOff()"));
  PN5180DEBUG_ENTER;

  uint8_t cmd[] = { PN5180_RF_OFF, parameter };

  if (!transceiveCommand(cmd, sizeof(cmd))) {
    PN5180ERROR(F("cmd_RfOff() failed at transceiveCommand()"));
    PN5180DEBUG_EXIT;
    return false;
  }

  PN5180DEBUG_EXIT;
  return true;
}
