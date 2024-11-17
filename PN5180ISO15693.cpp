// NAME: PN5180ISO15693.h
//
// DESC: ISO15693 protocol on NXP Semiconductors PN5180 module for Arduino.
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
#include "PN5180ISO15693.h"
#include "Debug.h"

// ISO15693-3 (1999) Request Flags Table 3 (http://olmicrowaves.com/menucontents/designsupport/rfid/ISO15693-3.pdf, page 9)
#define ISO15693_REQ_FLAG_SUBCARRIER	(1<<0)	// Subcarrier (0:ASK, 1:FSK)
#define ISO15693_REQ_FLAG_DATARATE		(1<<1)	// Uplink data rate (0:Low, 1:High)
#define ISO15693_REQ_FLAG_INVENTORY		(1<<2)	// Inventory (0: Bits 5-8 are table 4 flags, 1: Bits 5-8 are table 5 (inventory) flags)
#define ISO15693_REQ_FLAG_PROTOCOL		(1<<3)	// Protocol extention (always 0)

// ISO15693-3 (1999) Request Flags Table 4 (http://olmicrowaves.com/menucontents/designsupport/rfid/ISO15693-3.pdf, page 10)
#define ISO15693_REQ_FLAG_SELECT		(1<<4)	// Select (0: if Address flag set, use address mode, 1: Use select mode)
#define ISO15693_REQ_FLAG_ADDRESS		(1<<5)	// Address (0: Don't use address more, 1: Use address mode)
#define ISO15693_REQ_FLAG_OPTION		(1<<6)	// Option (0: Default value, 1: Must be set for write operations)
#define ISO15693_REQ_FLAG_RFU			(1<<7)	// RFU (always 0)

// ISO15693-3 (1999) Request Flags Table 5 (http://olmicrowaves.com/menucontents/designsupport/rfid/ISO15693-3.pdf, page 10)
#define ISO15693_REQ_FLAG_AFI			(1<<4)	// Inventory AFI (0: Don't use AFI, 1: Use AFI)
#define ISO15693_REQ_FLAG_NBSLOTS		(1<<5)	// Inventory Slots (0: 16-slots, 1: 1-slot)
#define ISO15693_REQ_FLAG_OPTION		(1<<6)	// Inventory Option (0: Default value, 1: Must be set for write operations)
#define ISO15693_REQ_FLAG_RFU			(1<<7)	// Inventory RFU (always 0)

// ISO15693-3 (1999) Response Flags Table 6 (http://olmicrowaves.com/menucontents/designsupport/rfid/ISO15693-3.pdf, page 11)
#define ISO15693_RESP_FLAG_ERROR		(1<<0)	// Error: (0: no error, 1: error)
#define ISO15693_RESP_FLAG_EXTENSION	(1<<3)	// Extension: (0: no protocol format extension, 1: protocol format is extended)

// ISO15693-3 (1999) Response Error Codes Table 7 (http://olmicrowaves.com/menucontents/designsupport/rfid/ISO15693-3.pdf, page 12)
// (See enum ISO15693ErrorCode in PN5180ISO15693.h)

// ISO15693-3 (1999) Commands (http://olmicrowaves.com/menucontents/designsupport/rfid/ISO15693-3.pdf, pages 21)
#define ISO15693_CMD_INVENTORY							(0x01)
#define ISO15693_CMD_STAYQUIET							(0x02)
// 0x03 to 0x1F RFU
#define ISO15693_CMD_READSINGLEBLOCK					(0x20)
#define ISO15693_CMD_WRITESINGLEBLOCK					(0x21)
#define ISO15693_CMD_LOCKBLOCK							(0x22)
#define ISO15693_CMD_READMULTIPLEBLOCKS					(0x23)
#define ISO15693_CMD_WRITEMULTIPLEBLOCKS				(0x24)
#define ISO15693_CMD_SELECT								(0x25)
#define ISO15693_CMD_RESETTOREADY						(0x26)
#define ISO15693_CMD_WRITEAFI							(0x27)
#define ISO15693_CMD_LOCKAFI							(0x28)
#define ISO15693_CMD_WRITEDSFID							(0x29)
#define ISO15693_CMD_LOCKDSFID							(0x2A)
#define ISO15693_CMD_GETSYSTEMINFORMATION				(0x2B)
#define ISO15693_CMD_GETMULTIPLEBLOCKSECURITYSTATUS		(0x2C)

PN5180ISO15693::PN5180ISO15693(uint8_t SSpin, uint8_t BUSYpin, uint8_t RSTpin, SPIClass& spi) 
              : PN5180(SSpin, BUSYpin, RSTpin, spi) {
}

/*
 * Inventory, code=01
 *
 * Inventory with flag for 16 time slots, code=01
 * https://www.nxp.com.cn/docs/en/application-note/AN12650.pdf
 * Request format: SOF, Req.Flags, Inventory, AFI (opt.), Mask len, Mask value, CRC16, EOF
 *                      |          |                      \_ Mask Length  (defined by ISO 15693)
 *                      |          \_ 0x01: Inventory command (defined by ISO 15693)
 *                      \_ 0x26: 0x02 high data rate | 0x04 inventory flags | 0x20 1 time slot (defined by ISO 15693)
 * Response format: SOF, Resp.Flags, DSFID, UID, CRC16, EOF
 */
ISO15693ErrorCode PN5180ISO15693::getInventory(uint8_t *uid) {
  PN5180DEBUG_PRINTF("PN5180ISO15693::getInventory()");
  PN5180DEBUG_PRINTLN();
  PN5180DEBUG_ENTER;

  uint8_t cmd = ISO15693_CMD_INVENTORY;
  // flags = 1 slot, only one card, no AFI field present, inventory flag + high data rate
  uint8_t flags = ISO15693_REQ_FLAG_NBSLOTS | ISO15693_REQ_FLAG_INVENTORY | ISO15693_REQ_FLAG_DATARATE;
  uint8_t maskLen = 0x00;
  uint8_t inventory[] = { cmd, flags, maskLen };

  PN5180DEBUG(F("Get Inventory..."));
  PN5180DEBUG_PRINTLN();
  for (int i=0; i<8; i++) {
    uid[i] = 0;  
  }
  
  uint8_t *readBuffer;
  ISO15693ErrorCode rc = issueISO15693Command(inventory, sizeof(inventory), &readBuffer);
  if (ISO15693_EC_OK != rc) {
    PN5180ERROR(F("getInventory() failed at issueISO15693Command()"));
    PN5180DEBUG_EXIT;
    return rc;
  }
  for (int i=0; i<8; i++) {
    uid[i] = readBuffer[2+i];
  }

#ifdef DEBUG
  PN5180DEBUG(F("Response flags: "));
  PN5180DEBUG(formatHex(readBuffer[0]));
  PN5180DEBUG(F(", Data Storage Format ID: "));
  PN5180DEBUG(formatHex(readBuffer[1]));
  PN5180DEBUG(F(", UID: "));
  for (int i=0; i<8; i++) {
    PN5180DEBUG(formatHex(uid[7-i])); // LSB comes first
    if (i<2) PN5180DEBUG(":");
  PN5180DEBUG_PRINTLN();
#endif

  PN5180DEBUG_EXIT;
  return ISO15693_EC_OK;
}

/*
 * Inventory with flag set for 16 time slots, code=01
 * https://www.nxp.com.cn/docs/en/application-note/AN12650.pdf
 * Request format: SOF, Req.Flags, Inventory, AFI (opt.), Mask len, Mask value, CRC16, EOF
 *                      |          |                      \- ? Mask Length  (defined by ISO 15693)
 *                      |          \- 0x01: Inventory command (defined by ISO 15693)
 *                      \- 0x26: 0x02 high data rate | 0x04 inventory flags | !(0x20) 16 time slots (defined by ISO 15693)
 * Response format: SOF, Resp.Flags, DSFID, UID, CRC16, EOF
 */
ISO15693ErrorCode PN5180ISO15693::getInventoryMultiple(uint8_t *uid, uint8_t maxTags, uint8_t *numCard) {
  PN5180DEBUG_PRINTF("PN5180ISO15693::getInventoryMultiple(maxTags=%d, numCard=%d)", maxTags, *numCard);
  PN5180DEBUG_PRINTLN();
  PN5180DEBUG_ENTER;

  uint16_t collision[maxTags];
  *numCard = 0;
  uint8_t numCol = 0;

  if (ISO15693_EC_OK != inventoryPoll(uid, maxTags, numCard, &numCol, collision)) {
    PN5180ERROR(F("getInventoryMultiple() failed at first inventoryPoll()"));
    PN5180DEBUG_EXIT;
    return ISO15693_EC_UNKNOWN_ERROR;
  }

  PN5180DEBUG_PRINTF("Number of collisions=%d", numCol);
  PN5180DEBUG_PRINTLN();

  while(numCol){                                                 // 5+ Continue until no collisions detected
#ifdef DEBUG
    PN5180DEBUG(F("Polling with mask=0x"));
    PN5180DEBUG(formatHex(collision[0]));
    PN5180DEBUG_PRINTLN();
#endif

    //                    inventoryPoll(uid, maxTags, numCard, &numCol, collision);
    if (ISO15693_EC_OK != inventoryPoll(uid, maxTags, numCard, &numCol, collision)) {
      PN5180ERROR(F("getInventoryMultiple() failed at collision resolution inventoryPoll()"));
      PN5180DEBUG_EXIT;
      return ISO15693_EC_UNKNOWN_ERROR;
    }

    numCol--;
    for(int i=0; i<numCol; i++){
      collision[i] = collision[i+1];
    }
  }
  
  PN5180DEBUG_EXIT;
  return ISO15693_EC_OK;
}

// 4. Send inventory request with mask length and mask value.
ISO15693ErrorCode PN5180ISO15693::inventoryPoll(uint8_t *uid, uint8_t maxTags, uint8_t *numCard, uint8_t *numCol, uint16_t *collision){
  PN5180DEBUG_PRINTF("PN5180ISO15693::inventoryPoll(maxTags=%d, numCard=%d, numCol=%d)", maxTags, *numCard, *numCol);
  PN5180DEBUG_PRINTLN();
  PN5180DEBUG_ENTER;
  
  uint8_t maskLen = 0;
  if(*numCol > 0){
    uint32_t mask = collision[0];
    do{
      mask >>= 4L;
      maskLen++;
    }while(mask > 0);
  } 
  uint8_t *p = (uint8_t*)&(collision[0]);
  // flags = 16 slots, up to 16 cards, no AFI field present, inventory flag + high data rate
  uint8_t flags = ISO15693_REQ_FLAG_DATARATE | ISO15693_REQ_FLAG_INVENTORY;
  const uint8_t inventory[7] = { flags, ISO15693_CMD_INVENTORY, uint8_t(maskLen*4), p[0], p[1], p[2], p[3] };
  // TODO: can p[] containing the collision mask bytes be more than 4 bytes long?
  // TODO: Why is collision[] initialized to a length of maxTags, but only 4 p[] values in this call?
  uint8_t cmdLen = 3 + (maskLen/2) + (maskLen%2);
#ifdef DEBUG
  PN5180DEBUG_PRINTF(F("mask=%d, maskLen=%d, cmdLen=%d"), p[0], maskLen, cmdLen);
  PN5180DEBUG_PRINTLN();
#endif

  // 3. Clear all IRQ_STATUS flags
  //   clearIRQStatus(0x000FFFFF);                                      
  if (!clearIRQStatus(0x000FFFFF)) {
    PN5180ERROR(F("inventoryPoll() failed at step 3. clearIRQStatus()"));
    PN5180DEBUG_EXIT;
    return ISO15693_EC_UNKNOWN_ERROR;
  }

  uint8_t *readBuffer;

  // 4. 5. 6. Idle/StopCom Command, Transceive Command, Inventory command
  //   sendData(inventory, cmdLen, 0);                                  
  if (!sendData(inventory, cmdLen, 0)) {
    PN5180ERROR(F("inventoryPoll() failed at step 4.5.6. sendData()"));
    PN5180DEBUG_EXIT;
    return ISO15693_EC_UNKNOWN_ERROR;
  }
  
  // 7. Loop to check 16 time slots for data
  for(uint8_t slot=0; slot<16; slot++){
    uint32_t rxStatus;
    uint32_t irqStatus = getIRQStatus();

    //   readRegister(RX_STATUS, &rxStatus);
    if (!readRegister(RX_STATUS, &rxStatus)) {
      PN5180ERROR(F("inventoryPoll() failed at step 7. readRegister() for slot %d"), slot);
      PN5180DEBUG_EXIT;
      return ISO15693_EC_UNKNOWN_ERROR;
    }

    PN5180DEBUG(F("slot="));
    PN5180DEBUG(formatHex(slot));
    PN5180DEBUG(F(": "));
    PN5180DEBUG(F("irqStatus="));
    PN5180DEBUG(formatHex(irqStatus));
    PN5180DEBUG(F(", RX_STATUS="));
    PN5180DEBUG(formatHex(rxStatus));
    PN5180DEBUG(F(": "));

    // 7+ Determine if a collision occurred
    uint16_t len = (uint16_t)(rxStatus & 0x000001ff);
    if((rxStatus >> 18) & 0x01 && *numCol < maxTags){
      if(maskLen > 0) collision[*numCol] = collision[0] | (slot << (maskLen * 4));
      else collision[*numCol] = slot << (maskLen * 4); // Yes, store position of collision
      *numCol = *numCol + 1;
#ifdef DEBUG
      PN5180DEBUG_PRINTF("Collision detected for UIDs matching %X starting at LSB", collision[*numCol-1]);
      PN5180DEBUG_PRINTLN();
#endif
    }
    else
		// 8. Check if a card has responded
		if(!(irqStatus & RX_IRQ_STAT) && !len){                   
      PN5180DEBUG(F("No card in this time slot."));
      PN5180DEBUG_PRINTLN();
    }
    else{
#ifdef DEBUG
      PN5180DEBUG_PRINTF("slot=%d, irqStatus: %ld, RX_STATUS: %ld, Response length=%d, ", slot, irqStatus, rxStatus, len);
#endif
      uint8_t *readBuffer;

      // 9. Read reception buffer
      readBuffer = readData(len+1);
      if(0L == readBuffer){
        PN5180ERROR(F("inventoryPoll() failed at step 9. readBuffer() for slot %d"), slot);
		PN5180DEBUG_EXIT;
        return ISO15693_EC_UNKNOWN_ERROR;
      }

#ifdef DEBUG
      PN5180DEBUG(F("readBuffer="));
      for(int i=0; i<len+1; i++){
        PN5180DEBUG(formatHex(readBuffer[i]));
        if (i<len) PN5180DEBUG(" ");
      }
      PN5180DEBUG_PRINTLN();
#endif

      // 10. Record all data to Inventory struct
      // Record raw UID data                                       
      for (int i=0; i<8; i++) {
        uint8_t startAddr = (*numCard * 8) + i;
        uid[startAddr] = readBuffer[2+i];
      }
      *numCard = *numCard + 1;

#ifdef DEBUG
      //PN5180DEBUG_PRINTF("getInventoryMultiple: Response flags: 0x%X, Data Storage Format ID: 0x%X", readBuffer[0], readBuffer[1]);
      //PN5180DEBUG_PRINTLN();
      PN5180DEBUG_PRINTF("numCard=%d", *numCard);
      PN5180DEBUG_PRINTLN();
#endif
    }
    
    if(slot+1 < 16){ // If we have more cards to poll for...
      // 11. Next SEND_DATA will only include EOF
      //   writeRegisterWithAndMask(TX_CONFIG, 0xFFFFFB3F);             
      if (!writeRegisterWithAndMask(TX_CONFIG, 0xFFFFFB3F)) {
        PN5180ERROR(F("inventoryPoll() failed at step 11. writeRegisterWithAndMask() for slot %d"), slot);
        PN5180DEBUG_EXIT;
        return ISO15693_EC_UNKNOWN_ERROR;
      }
	  delay(5);
	  // 14. Clear all IRQ_STATUS flags
      if (!clearIRQStatus(0x000FFFFF)) {
        PN5180ERROR(F("inventoryPoll() failed at step 14. clearIRQStatus() for slot %d"), slot);
        PN5180DEBUG_EXIT;
        return ISO15693_EC_UNKNOWN_ERROR;
      }
	  // 12. 13. 15. Idle/StopCom Command, Transceive Command, Send EOF
      //   sendData(inventory, 0, 0);                                 
      if (!sendData(inventory, 0, 0)) {
        PN5180ERROR(F("inventoryPoll() failed at step 12.13.15. clearIRQStatus() for slot %d"), slot);
        PN5180DEBUG_EXIT;
        return ISO15693_EC_UNKNOWN_ERROR;
      }
    }
  }

  // 16. Switch off RF field
  //   setRF_off();                                                     
  if (!setRF_off()) {
    PN5180ERROR(F("inventoryPoll() failed at step 16. setRF_off()"));
    PN5180DEBUG_EXIT;
    return ISO15693_EC_UNKNOWN_ERROR;
  }

  // 1. 2. Load ISO15693 config, RF on
  if (!setupRF()) {
    PN5180ERROR(F("inventoryPoll() failed at step 1.2. setupRF()"));
    PN5180DEBUG_EXIT;
    return ISO15693_EC_UNKNOWN_ERROR;
  }

  PN5180DEBUG_EXIT;
  return ISO15693_EC_OK;
}

/*
 * Read single block, code=20
 *
 * Request format: SOF, Req.Flags, ReadSingleBlock, UID (opt.), BlockNumber, CRC16, EOF
 * Response format:
 *  when ERROR flag is set:
 *    SOF, Resp.Flags, ErrorCode, CRC16, EOF
 *
 *     Response Flags:
 *    xxxx.3xx0
 *         |||\_ Error flag: 0=no error, 1=error detected, see error field
 *         \____ Extension flag: 0=no extension, 1=protocol format is extended
 *
 *  If Error flag is set, the following error codes are defined:
 *    01 = The command is not supported, i.e. the request code is not recognized.
 *    02 = The command is not recognized, i.e. a format error occurred.
 *    03 = The option is not supported.
 *    0F = Unknown error.
 *    10 = The specific block is not available.
 *    11 = The specific block is already locked and cannot be locked again.
 *    12 = The specific block is locked and cannot be changed.
 *    13 = The specific block was not successfully programmed.
 *    14 = The specific block was not successfully locked.
 *    A0-DF = Custom command error codes
 *
 *  when ERROR flag is NOT set:
 *    SOF, Flags, BlockData (len=blockLength), CRC16, EOF
 */
ISO15693ErrorCode PN5180ISO15693::readSingleBlock(const uint8_t *uid, uint8_t blockNo, uint8_t *blockData, uint8_t blockSize) {
  //                            flags, cmd, uid,             blockNo
  uint8_t readSingleBlock[] = { 0x22, 0x20, 1,2,3,4,5,6,7,8, blockNo }; // UID has LSB first!
  //                              |\- high data rate
  //                              \-- no options, addressed by UID
  for (int i=0; i<8; i++) {
    readSingleBlock[2+i] = uid[i];
  }

#ifdef DEBUG
  PN5180DEBUG_PRINTF("Read Single Block #%d, size=%d:", blockNo, blockSize);
  for (int i=0; i<sizeof(readSingleBlock); i++) {
    PN5180DEBUG(" ");
    PN5180DEBUG(formatHex(readSingleBlock[i]));
  }
  PN5180DEBUG_PRINTLN();
#endif

  uint8_t *resultPtr;
  ISO15693ErrorCode rc = issueISO15693Command(readSingleBlock, sizeof(readSingleBlock), &resultPtr);
  if (ISO15693_EC_OK != rc) {
    return rc;
  }

  PN5180DEBUG("Value=");
  
  for (int i=0; i<blockSize; i++) {
    blockData[i] = resultPtr[1+i];
#ifdef DEBUG    
    PN5180DEBUG(formatHex(blockData[i]));
    PN5180DEBUG(" ");
#endif    
  }

#ifdef DEBUG
  PN5180DEBUG(" ");
  for (int i=0; i<blockSize; i++) {
    char c = blockData[i];
    if (isPrintable(c)) {
      PN5180DEBUG(c);
    }
    else PN5180DEBUG(".");
  }
  PN5180DEBUG_PRINTLN();
#endif

  return ISO15693_EC_OK;
}

/*
 * Write single block, code=21
 *
 * Request format: SOF, Requ.Flags, WriteSingleBlock, UID (opt.), BlockNumber, BlockData (len=blcokLength), CRC16, EOF
 * Response format:
 *  when ERROR flag is set:
 *    SOF, Resp.Flags, ErrorCode, CRC16, EOF
 *
 *     Response Flags:
  *    xxxx.3xx0
  *         |||\_ Error flag: 0=no error, 1=error detected, see error field
  *         \____ Extension flag: 0=no extension, 1=protocol format is extended
  *
  *  If Error flag is set, the following error codes are defined:
  *    01 = The command is not supported, i.e. the request code is not recognized.
  *    02 = The command is not recognized, i.e. a format error occurred.
  *    03 = The option is not supported.
  *    0F = Unknown error.
  *    10 = The specific block is not available.
  *    11 = The specific block is already locked and cannot be locked again.
  *    12 = The specific block is locked and cannot be changed.
  *    13 = The specific block was not successfully programmed.
  *    14 = The specific block was not successfully locked.
  *    A0-DF = Custom command error codes
 *
 *  when ERROR flag is NOT set:
 *    SOF, Resp.Flags, CRC16, EOF
 */
ISO15693ErrorCode PN5180ISO15693::writeSingleBlock(const uint8_t *uid, uint8_t blockNo, const uint8_t *blockData, uint8_t blockSize) {
  //                            flags, cmd, uid,             blockNo
  uint8_t writeSingleBlock[] = { 0x22, 0x21, 1,2,3,4,5,6,7,8, blockNo }; // UID has LSB first!
  //                               |\- high data rate
  //                               \-- no options, addressed by UID

  uint8_t writeCmdSize = sizeof(writeSingleBlock) + blockSize;
  uint8_t *writeCmd = (uint8_t*)malloc(writeCmdSize);
  uint8_t pos = 0;
  writeCmd[pos++] = writeSingleBlock[0];
  writeCmd[pos++] = writeSingleBlock[1];
  for (int i=0; i<8; i++) {
    writeCmd[pos++] = uid[i];
  }
  writeCmd[pos++] = blockNo;
  for (int i=0; i<blockSize; i++) {
    writeCmd[pos++] = blockData[i];
  }

#ifdef DEBUG
  PN5180DEBUG("Write Single Block #");
  PN5180DEBUG(blockNo);
  PN5180DEBUG(", size=");
  PN5180DEBUG(blockSize);
  PN5180DEBUG(":");
  for (int i=0; i<writeCmdSize; i++) {
    PN5180DEBUG(" ");
    PN5180DEBUG(formatHex(writeCmd[i]));
  }
  PN5180DEBUG_PRINTLN();
#endif

  uint8_t *resultPtr;
  ISO15693ErrorCode rc =  issueISO15693Command(writeCmd, writeCmdSize, &resultPtr);
  if (ISO15693_EC_OK != rc) {
    free(writeCmd);
    return rc;
  }

  free(writeCmd);
  return ISO15693_EC_OK;
}

/*
 * Read multiple block, code=23
 *
 * Request format: SOF, Req.Flags, ReadMultipleBlock, UID (opt.), FirstBlockNumber, numBlocks, CRC16, EOF
 * Response format:
 *  when ERROR flag is set:
 *    SOF, Resp.Flags, ErrorCode, CRC16, EOF
 *
 *     Response Flags:
  *    xxxx.3xx0
  *         |||\_ Error flag: 0=no error, 1=error detected, see error field
  *         \____ Extension flag: 0=no extension, 1=protocol format is extended
  *
  *  If Error flag is set, the following error codes are defined:
  *    01 = The command is not supported, i.e. the request code is not recognized.
  *    02 = The command is not recognized, i.e. a format error occurred.
  *    03 = The option is not supported.
  *    0F = Unknown error.
  *    10 = The specific block is not available.
  *    11 = The specific block is already locked and cannot be locked again.
  *    12 = The specific block is locked and cannot be changed.
  *    13 = The specific block was not successfully programmed.
  *    14 = The specific block was not successfully locked.
  *    A0-DF = Custom command error codes
 *
 *  when ERROR flag is NOT set:
 *    SOF, Flags, BlockData (len=blockSize * numBlock), CRC16, EOF
 */
ISO15693ErrorCode PN5180ISO15693::readMultipleBlock(const uint8_t *uid, uint8_t blockNo, uint8_t numBlock, uint8_t *blockData, uint8_t blockSize) {
  if(blockNo > numBlock-1){ // Attempted to start at a block greater than the num blocks on the VICC
    PN5180DEBUG("Starting block exceeds length of data");
    return ISO15693_EC_BLOCK_NOT_AVAILABLE;
  }
  if( (blockNo + numBlock) > numBlock ){ // Will attempt to read a block greater than the num blocks on the VICC 
    PN5180DEBUG("End of block exceeds length of data");
    return ISO15693_EC_BLOCK_NOT_AVAILABLE;
  }
  
  //                              flags, cmd, uid,             1stBlock blocksToRead  
  uint8_t readMultipleCmd[12] = { 0x22, 0x23, 1,2,3,4,5,6,7,8, blockNo, uint8_t(numBlock-1) }; // UID has LSB first!
  //                                |\- high data rate
  //                                \-- no options, addressed by UID
  
  for (int i=0; i<8; i++) {
    readMultipleCmd[2+i] = uid[i];
  }

  PN5180DEBUG("readMultipleBlock: Read Block #");
  PN5180DEBUG(blockNo);
  PN5180DEBUG("-");
  PN5180DEBUG(blockNo+numBlock-1);
  PN5180DEBUG(", blockSize=");
  PN5180DEBUG(blockSize);
  PN5180DEBUG(", Cmd: ");
  for (int i=0; i<sizeof(readMultipleCmd); i++) {
    PN5180DEBUG(" ");
    PN5180DEBUG(formatHex(readMultipleCmd[i]));
  }

  uint8_t *resultPtr;
  ISO15693ErrorCode rc = issueISO15693Command(readMultipleCmd, sizeof(readMultipleCmd), &resultPtr);
  if (ISO15693_EC_OK != rc) return rc;

  PN5180DEBUG("readMultipleBlock: Value=");
  for (int i=0; i<numBlock * blockSize; i++) {
    blockData[i] = resultPtr[1+i];
#ifdef DEBUG    
    PN5180DEBUG(formatHex(blockData[i]));
    PN5180DEBUG(" ");
#endif 
  }

#ifdef DEBUG
  PN5180DEBUG(" ");
  for (int i=0; i<blockSize; i++) {
    char c = blockData[i];
    if (isPrintable(c)) {
      PN5180DEBUG(c);
    }
    else PN5180DEBUG(".");
  }
  PN5180DEBUG_PRINTLN();
#endif

  return ISO15693_EC_OK;
}

/*
 * Get System Information, code=2B
 *
 * Request format: SOF, Req.Flags, GetSysInfo, UID (opt.), CRC16, EOF
 * Response format:
 *  when ERROR flag is set:
 *    SOF, Resp.Flags, ErrorCode, CRC16, EOF
 *
 *     Response Flags:
  *    xxxx.3xx0
  *         |||\_ Error flag: 0=no error, 1=error detected, see error field
  *         \____ Extension flag: 0=no extension, 1=protocol format is extended
  *
  *  If Error flag is set, the following error codes are defined:
  *    01 = The command is not supported, i.e. the request code is not recognized.
  *    02 = The command is not recognized, i.e. a format error occurred.
  *    03 = The option is not supported.
  *    0F = Unknown error.
  *    10 = The specific block is not available.
  *    11 = The specific block is already locked and cannot be locked again.
  *    12 = The specific block is locked and cannot be changed.
  *    13 = The specific block was not successfully programmed.
  *    14 = The specific block was not successfully locked.
  *    A0-DF = Custom command error codes
  *
 *  when ERROR flag is NOT set:
 *    SOF, Flags, InfoFlags, UID, DSFID (opt.), AFI (opt.), Other fields (opt.), CRC16, EOF
 *
 *    InfoFlags:
 *    xxxx.3210
 *         |||\_ DSFID: 0=DSFID not supported, DSFID field NOT present; 1=DSFID supported, DSFID field present
 *         ||\__ AFI: 0=AFI not supported, AFI field not present; 1=AFI supported, AFI field present
 *         |\___ VICC memory size:
 *         |        0=Information on VICC memory size is not supported. Memory size field is present. ???
 *         |        1=Information on VICC memory size is supported. Memory size field is present.
 *         \____ IC reference:
 *                  0=Information on IC reference is not supported. IC reference field is not present.
 *                  1=Information on IC reference is supported. IC reference field is not present.
 *
 *    VICC memory size:
 *      xxxb.bbbb nnnn.nnnn
 *        bbbbb - Block size is expressed in number of bytes, on 5 bits, allowing to specify up to 32 bytes i.e. 256 bits.
 *        nnnn.nnnn - Number of blocks is on 8 bits, allowing to specify up to 256 blocks.
 *
 *    IC reference: The IC reference is on 8 bits and its meaning is defined by the IC manufacturer.
 */
ISO15693ErrorCode PN5180ISO15693::getSystemInfo(uint8_t *uid, uint8_t *blockSize, uint8_t *numBlocks) {
  uint8_t sysInfo[] = { 0x22, 0x2b, 1,2,3,4,5,6,7,8 };  // UID has LSB first!
  for (int i=0; i<8; i++) {
    sysInfo[2+i] = uid[i];
  }

#ifdef DEBUG
  PN5180DEBUG("Get System Information");
  for (int i=0; i<sizeof(sysInfo); i++) {
    PN5180DEBUG(" ");
    PN5180DEBUG(formatHex(sysInfo[i]));
  }
  PN5180DEBUG_PRINTLN();
#endif

  uint8_t *readBuffer;
  ISO15693ErrorCode rc = issueISO15693Command(sysInfo, sizeof(sysInfo), &readBuffer);
  if (ISO15693_EC_OK != rc) {
    return rc;
  }

  for (int i=0; i<8; i++) {
    uid[i] = readBuffer[2+i];
  }
  
#ifdef DEBUG
  PN5180DEBUG("UID=");
  for (int i=0; i<8; i++) {
    PN5180DEBUG(formatHex(readBuffer[9-i]));  // UID has LSB first!
    if (i<2) PN5180DEBUG(":");
  }
  PN5180DEBUG_PRINTLN();
#endif
  
  uint8_t *p = &readBuffer[10];

  uint8_t infoFlags = readBuffer[1];
  if (infoFlags & 0x01) { // DSFID flag
    uint8_t __attribute__((unused)) dsfid = *p++;
    PN5180DEBUG("DSFID=");  // Data storage format identifier
    PN5180DEBUG(formatHex(dsfid));
    PN5180DEBUG_PRINTLN();
  }
#ifdef DEBUG
  else PN5180DEBUG(F("No DSFID\n"));  
#endif
  
  if (infoFlags & 0x02) { // AFI flag
    uint8_t afi = *p++;
    PN5180DEBUG(F("AFI="));  // Application family identifier
    PN5180DEBUG(formatHex(afi));
    PN5180DEBUG(F(" - "));
    switch (afi >> 4) {
      case 0: PN5180DEBUG(F("All families")); break;
      case 1: PN5180DEBUG(F("Transport")); break;
      case 2: PN5180DEBUG(F("Financial")); break;
      case 3: PN5180DEBUG(F("Identification")); break;
      case 4: PN5180DEBUG(F("Telecommunication")); break;
      case 5: PN5180DEBUG(F("Medical")); break;
      case 6: PN5180DEBUG(F("Multimedia")); break;
      case 7: PN5180DEBUG(F("Gaming")); break;
      case 8: PN5180DEBUG(F("Data storage")); break;
      case 9: PN5180DEBUG(F("Item management")); break;
      case 10: PN5180DEBUG(F("Express parcels")); break;
      case 11: PN5180DEBUG(F("Postal services")); break;
      case 12: PN5180DEBUG(F("Airline bags")); break;
      default: PN5180DEBUG(F("Unknown")); break;
    }
    PN5180DEBUG_PRINTLN();
  }
#ifdef DEBUG
  else PN5180DEBUG(F("No AFI\n"));
#endif

  if (infoFlags & 0x04) { // VICC Memory size
    *numBlocks = *p++;
    *blockSize = *p++;
    *blockSize = (*blockSize) & 0x1f;

    *blockSize = *blockSize + 1; // range: 1-32
    *numBlocks = *numBlocks + 1; // range: 1-256

    PN5180DEBUG("VICC MemSize=");
    PN5180DEBUG(uint16_t(*blockSize) * (*numBlocks));
    PN5180DEBUG(" BlockSize=");
    PN5180DEBUG(*blockSize);
    PN5180DEBUG(" NumBlocks=");
    PN5180DEBUG(*numBlocks);
    PN5180DEBUG_PRINTLN();
  }
#ifdef DEBUG
  else PN5180DEBUG(F("No VICC memory size\n"));
#endif
   
  if (infoFlags & 0x08) { // IC reference
	uint8_t __attribute__((unused)) iRef = *p++;
    PN5180DEBUG("IC Ref=");
    PN5180DEBUG(formatHex(iRef));
    PN5180DEBUG_PRINTLN();
  }
#ifdef DEBUG
  else PN5180DEBUG(F("No IC ref\n"));
#endif

  return ISO15693_EC_OK;
}


// ICODE SLIX specific commands

/*
 * The GET RANDOM NUMBER command is required to receive a random number from the label IC. 
 * The passwords that will be transmitted with the SET PASSWORD,ENABLEPRIVACY and DESTROY commands 
 * have to be calculated with the password and the random number (see Section 9.5.3.2 "SET PASSWORD")
 */
ISO15693ErrorCode PN5180ISO15693::getRandomNumber(uint8_t *randomData) {
  uint8_t getrandom[] = {0x02, 0xB2, 0x04};
  uint8_t *readBuffer;
  ISO15693ErrorCode rc = issueISO15693Command(getrandom, sizeof(getrandom), &readBuffer);
  if (rc == ISO15693_EC_OK) {
    randomData[0] = readBuffer[1];
    randomData[1] = readBuffer[2];
  }
  return rc;
}

/*
 * The SET PASSWORD command enables the different passwords to be transmitted to the label 
 * to access the different protected functionalities of the following commands. 
 * The SET PASSWORD command has to be executed just once for the related passwords if the label is powered
 */
ISO15693ErrorCode PN5180ISO15693::setPassword(uint8_t identifier, const uint8_t *password, const uint8_t *random) {
  uint8_t setPassword[] = {0x02, 0xB3, 0x04, 0x04, 0x00, 0x00, 0x00, 0x00};
  uint8_t *readBuffer;
  setPassword[3] = identifier;
  setPassword[4] = password[0] ^ random[0];
  setPassword[5] = password[1] ^ random[1];
  setPassword[6] = password[2] ^ random[0];
  setPassword[7] = password[3] ^ random[1];
  return issueISO15693Command(setPassword, sizeof(setPassword), &readBuffer);
}

/*
 * The ENABLE PRIVACY command enables the ICODE SLIX2 Label IC to be set to
 * Privacy mode if the Privacy password is correct. The ICODE SLIX2 will not respond to
 * any command except GET RANDOM NUMBER and SET PASSWORD
 */
ISO15693ErrorCode PN5180ISO15693::enablePrivacy(const uint8_t *password, const uint8_t *random) {
  uint8_t setPrivacy[] = {0x02, 0xBA, 0x04, 0x00, 0x00, 0x00, 0x00};
  uint8_t *readBuffer;
  setPrivacy[3] = password[0] ^ random[0];
  setPrivacy[4] = password[1] ^ random[1];
  setPrivacy[5] = password[2] ^ random[0];
  setPrivacy[6] = password[3] ^ random[1];
  return issueISO15693Command(setPrivacy, sizeof(setPrivacy), &readBuffer);
}


// disable privacy mode for ICODE SLIX2 tag with given password
ISO15693ErrorCode PN5180ISO15693::disablePrivacyMode(const uint8_t *password) {
  // get a random number from the tag
  uint8_t data[]= {0x00, 0x00};
  ISO15693ErrorCode rc = getRandomNumber(data);
  if (rc != ISO15693_EC_OK) {
    return rc;
  }
  
  // set password to disable privacy mode 
  rc = setPassword(0x04, password, data);
  return rc; 
}

// enable privacy mode for ICODE SLIX2 tag with given password 
ISO15693ErrorCode PN5180ISO15693::enablePrivacyMode(const uint8_t *password) {
  // get a random number from the tag
  uint8_t data[]= {0x00, 0x00};
  ISO15693ErrorCode rc = getRandomNumber(data);
  if (rc != ISO15693_EC_OK) {
    return rc;
  }
  
  // enable privacy command to lock the tag
  rc = enablePrivacy(password, data);
  return rc; 
}


/*
 * ISO 15693 - Protocol
 *
 * General Request Format:
 *  SOF, Req.Flags, Command code, Parameters, Data, CRC16, EOF
 *
 *  Request Flags:
 *    xxxx.3210
 *         |||\_ Subcarrier flag: 0=single sub-carrier, 1=two sub-carrier
 *         ||\__ Datarate flag: 0=low data rate, 1=high data rate
 *         |\___ Inventory flag: 0=no inventory, 1=inventory
 *         \____ Protocol extension flag: 0=no extension, 1=protocol format is extended
 *
 *  If Inventory flag is set:
 *    7654.xxxx
 *     ||\_ AFI flag: 0=no AFI field present, 1=AFI field is present
 *     |\__ Number of slots flag: 0=16 slots, 1=1 slot
 *     \___ Option flag: 0=default, 1=meaning is defined by command description
 *
 *  If Inventory flag is NOT set:
 *    7654.xxxx
 *     ||\_ Select flag: 0=request shall be executed by any VICC according to Address_flag
 *     ||                1=request shall be executed only by VICC in selected state
 *     |\__ Address flag: 0=request is not addressed. UID field is not present.
 *     |                  1=request is addressed. UID field is present. Only VICC with UID shall answer
 *     \___ Option flag: 0=default, 1=meaning is defined by command description
 *
 * General Response Format:
 *  SOF, Resp.Flags, Parameters, Data, CRC16, EOF
 *
 *  Response Flags:
 *    xxxx.3210
 *         |||\_ Error flag: 0=no error, 1=error detected, see error field
 *         ||\__ RFU: 0
 *         |\___ RFU: 0
 *         \____ Extension flag: 0=no extension, 1=protocol format is extended
 *
 *  If Error flag is set, the following error codes are defined:
 *    01 = The command is not supported, i.e. the request code is not recognized.
 *    02 = The command is not recognized, i.e. a format error occurred.
 *    03 = The option is not supported.
 *    0F = Unknown error.
 *    10 = The specific block is not available.
 *    11 = The specific block is already locked and cannot be locked again.
 *    12 = The specific block is locked and cannot be changed.
 *    13 = The specific block was not successfully programmed.
 *    14 = The specific block was not successfully locked.
 *    A0-DF = Custom command error codes
 *
 *  Function return values:
 *    0 = OK
 *   -1 = No card detected
 *   >0 = Error code
 */
ISO15693ErrorCode PN5180ISO15693::issueISO15693Command(const uint8_t *cmd, uint8_t cmdLen, uint8_t **resultPtr) {
#ifdef DEBUG
  PN5180DEBUG(F("Issue Command 0x"));
  PN5180DEBUG(formatHex(cmd[1]));
  PN5180DEBUG("...\n");
#endif

  sendData(cmd, cmdLen);
  delay(10);

  uint32_t irqR = getIRQStatus();
  if (0 == (irqR & RX_SOF_DET_IRQ_STAT)) {
    PN5180DEBUG("Didnt detect RX_SOF_DET_IRQ_STAT after sendData");
    return EC_NO_CARD;
  }
  
  unsigned long startedWaiting = millis();
  while(!(irqR & RX_IRQ_STAT)) {
    irqR = getIRQStatus();
    if (millis() - startedWaiting > commandTimeout) {
      PN5180DEBUG("Didnt detect RX_IRQ_STAT after sendData");
      return EC_NO_CARD;
    }
  }
  
  uint32_t rxStatus;
  //   readRegister(RX_STATUS, &rxStatus);
  if (!readRegister(RX_STATUS, &rxStatus)) {
    PN5180ERROR(F("issueISO15693Command() failed at readRegister()"));
    PN5180DEBUG_EXIT;
    return ISO15693_EC_UNKNOWN_ERROR;
  }
  
  PN5180DEBUG(F("RX-Status="));
  PN5180DEBUG(formatHex(rxStatus));

  uint16_t len = (uint16_t)(rxStatus & 0x000001ff);
  
  PN5180DEBUG(", len=");
  PN5180DEBUG(len);
  PN5180DEBUG_PRINTLN();

  *resultPtr = readData(len);
  if (0L == *resultPtr) {
    PN5180ERROR(F("issueISO15693Command() failed at readData()"));
    return ISO15693_EC_UNKNOWN_ERROR;
  }
  
#ifdef DEBUG
  Serial.print("Read=");
  for (int i=0; i<len; i++) {
    Serial.print(formatHex((*resultPtr)[i]));
    if (i<len-1) Serial.print(":");
  }
  Serial.println();
#endif

  uint32_t irqStatus = getIRQStatus();
  if (0 == (RX_SOF_DET_IRQ_STAT & irqStatus)) { // no card detected
    PN5180DEBUG("Didnt detect RX_SOF_DET_IRQ_STAT after readData");
    clearIRQStatus(TX_IRQ_STAT | IDLE_IRQ_STAT);
    return EC_NO_CARD;
  }

  uint8_t responseFlags = (*resultPtr)[0];
  if (responseFlags & (1<<0)) { // error flag
    uint8_t errorCode = (*resultPtr)[1];
    
    PN5180DEBUG("ERROR code=");
    PN5180DEBUG(formatHex(errorCode));
    PN5180DEBUG(" - ");
    PN5180DEBUG(strerror((ISO15693ErrorCode)errorCode));
    PN5180DEBUG_PRINTLN();

    if (errorCode >= 0xA0) { // custom command error codes
      return ISO15693_EC_CUSTOM_CMD_ERROR;
    }
    else return (ISO15693ErrorCode)errorCode;
  }

#ifdef DEBUG
  if (responseFlags & (1<<3)) { // extendsion flag
    PN5180DEBUG("Extension flag is set!\n");
  }
#endif

  clearIRQStatus(RX_SOF_DET_IRQ_STAT | IDLE_IRQ_STAT | TX_IRQ_STAT | RX_IRQ_STAT);
  return ISO15693_EC_OK;
}

bool PN5180ISO15693::setupRF() {
  PN5180DEBUG(F("Loading RF-Configuration...\n"));
  if (loadRFConfig(0x0d, 0x8d)) {  // ISO15693 parameters
    PN5180DEBUG(F("done.\n"));
  }
  else return false;

  PN5180DEBUG(F("Turning ON RF field...\n"));
  if (setRF_on()) {
    PN5180DEBUG(F("done.\n"));
  }
  else return false;

  writeRegisterWithAndMask(SYSTEM_CONFIG, 0xfffffff8);  // Idle/StopCom Command
  writeRegisterWithOrMask(SYSTEM_CONFIG, 0x00000003);   // Transceive Command

  return true;
}

const char *PN5180ISO15693::strerror(ISO15693ErrorCode code) {
  PN5180DEBUG(("ISO15693ErrorCode="));
  PN5180DEBUG(code);
  PN5180DEBUG_PRINTLN();
  
  switch (code) {
    case EC_NO_CARD: return ("No card detected!");
    case ISO15693_EC_OK: return ("OK!");
    case ISO15693_EC_NOT_SUPPORTED: return ("Command is not supported!");
    case ISO15693_EC_NOT_RECOGNIZED: return ("Command is not recognized!");
    case ISO15693_EC_OPTION_NOT_SUPPORTED: return ("Option is not supported!");
    case ISO15693_EC_UNKNOWN_ERROR: return ("Unknown error!");
    case ISO15693_EC_BLOCK_NOT_AVAILABLE: return ("Specified block is not available!");
    case ISO15693_EC_BLOCK_ALREADY_LOCKED: return ("Specified block is already locked!");
    case ISO15693_EC_BLOCK_IS_LOCKED: return ("Specified block is locked and cannot be changed!");
    case ISO15693_EC_BLOCK_NOT_PROGRAMMED: return ("Specified block was not successfully programmed!");
    case ISO15693_EC_BLOCK_NOT_LOCKED: return ("Specified block was not successfully locked!");
    default:
      if ((code >= 0xA0) && (code <= 0xDF)) {
        return ("Custom command error code!");
      }
      else return ("Undefined error code in ISO15693!");
  }
}
