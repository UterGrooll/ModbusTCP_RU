#include "ModbusTCP_RU.h"

EthernetServer MbServer(MB_PORT);
#ifndef MB_SLAVE_ONLY
EthernetClient MbmClient;
#endif

static word mbWord(byte high, byte low)
{
  return ((word)high << 8) | low;
}

ModbusTCP_RU::ModbusTCP_RU()
  : MbData(MbHoldingRegisters),
#ifndef MB_SLAVE_ONLY
    remSlaveIP(0, 0, 0, 0),
    MbmFC(MB_FC_NONE),
    MbmCounter(0),
    MbmExpectedLength(-1),
    MbmLastActivity(0),
    MbmTransactionId(0),
    MbmPendingTransactionId(0),
    MbmPos(0),
    MbmBitCount(0),
#endif
    MbsFC(MB_FC_NONE),
    MbsServerStarted(false)
{
  memset(MbCoils, 0, sizeof(MbCoils));
  memset(MbDiscreteInputs, 0, sizeof(MbDiscreteInputs));
  memset(MbHoldingRegisters, 0, sizeof(MbHoldingRegisters));
  memset(MbInputRegisters, 0, sizeof(MbInputRegisters));
  coilWriteCallback = 0;
  holdingWriteCallback = 0;
  resetStats();

  for (byte i = 0; i < MB_MAX_CLIENTS; i++) {
    serverClients[i].length = 0;
    serverClients[i].expectedLength = -1;
    serverClients[i].lastActivity = 0;
  }
}

#ifndef MB_SLAVE_ONLY
void ModbusTCP_RU::Req(MB_FC FC, word Ref, word Count, word Pos)
{
  if (remSlaveIP == IPAddress(0, 0, 0, 0)) {
    return;
  }

  if (MbmClient.connected()) {
    return;
  }

  MbmFC = FC;
  MbmPendingTransactionId = ++MbmTransactionId;

  MbmByteArray[0] = highByte(MbmPendingTransactionId);
  MbmByteArray[1] = lowByte(MbmPendingTransactionId);
  MbmByteArray[2] = 0;
  MbmByteArray[3] = 0;
  MbmByteArray[4] = 0;
  MbmByteArray[5] = 6;
  MbmByteArray[6] = 1;
  MbmByteArray[7] = FC;
  MbmByteArray[8] = highByte(Ref);
  MbmByteArray[9] = lowByte(Ref);

  if (FC == MB_FC_READ_COILS || FC == MB_FC_READ_DISCRETE_INPUT) {
    if (Count < 1) { Count = 1; }
    if (Count > MB_PROTOCOL_MAX_READ_BITS) { Count = MB_PROTOCOL_MAX_READ_BITS; }
    MbmByteArray[10] = highByte(Count);
    MbmByteArray[11] = lowByte(Count);
  }

  if (FC == MB_FC_READ_REGISTERS || FC == MB_FC_READ_INPUT_REGISTER) {
    if (Count < 1) { Count = 1; }
    if (Count > MB_PROTOCOL_MAX_READ_REGISTERS) { Count = MB_PROTOCOL_MAX_READ_REGISTERS; }
    MbmByteArray[10] = highByte(Count);
    MbmByteArray[11] = lowByte(Count);
  }

  if (FC == MB_FC_WRITE_COIL) {
    MbmByteArray[10] = CoilRead(Pos) ? 0xFF : 0x00;
    MbmByteArray[11] = 0;
  }

  if (FC == MB_FC_WRITE_REGISTER) {
    if (!IsValidRegister(Pos)) {
      return;
    }
    MbmByteArray[10] = highByte(MbHoldingRegisters[Pos]);
    MbmByteArray[11] = lowByte(MbHoldingRegisters[Pos]);
  }

  if (FC == MB_FC_WRITE_MULTIPLE_COILS) {
    if (Count < 1) { Count = 1; }
    if (Count > MB_PROTOCOL_MAX_WRITE_COILS) { Count = MB_PROTOCOL_MAX_WRITE_COILS; }

    byte byteCount = (Count + 7) / 8;
    if ((13 + byteCount) > MB_BUFFER_SIZE) {
      return;
    }

    MbmByteArray[10] = highByte(Count);
    MbmByteArray[11] = lowByte(Count);
    MbmByteArray[12] = byteCount;
    MbmByteArray[4] = highByte(byteCount + 7);
    MbmByteArray[5] = lowByte(byteCount + 7);

    for (int i = 0; i < byteCount; i++) {
      MbmByteArray[13 + i] = 0;
    }

    for (word i = 0; i < Count; i++) {
      bitWrite(MbmByteArray[13 + (i / 8)], i % 8, CoilRead(Pos + i));
    }
  }

  if (FC == MB_FC_WRITE_MULTIPLE_REGISTERS) {
    if (Count < 1) { Count = 1; }
    if (Count > MB_PROTOCOL_MAX_WRITE_REGISTERS) { Count = MB_PROTOCOL_MAX_WRITE_REGISTERS; }

    word byteCount = Count * 2;
    if ((13 + byteCount) > MB_BUFFER_SIZE) {
      return;
    }

    MbmByteArray[10] = highByte(Count);
    MbmByteArray[11] = lowByte(Count);
    MbmByteArray[12] = byteCount;
    MbmByteArray[4] = highByte(byteCount + 7);
    MbmByteArray[5] = lowByte(byteCount + 7);

    for (word i = 0; i < Count; i++) {
      if (!IsValidRegister(Pos + i)) {
        return;
      }
      MbmByteArray[(i * 2) + 13] = highByte(MbHoldingRegisters[Pos + i]);
      MbmByteArray[(i * 2) + 14] = lowByte(MbHoldingRegisters[Pos + i]);
    }
  }

  if (MbmClient.connect(remSlaveIP, MB_PORT)) {
#ifdef MB_DEBUG
    Serial.println(F("MB master connected"));
#endif
    uint16_t messageLength = mbWord(MbmByteArray[4], MbmByteArray[5]) + 6;
    MbmClient.write(MbmByteArray, messageLength);
    modbusStats.txPackets++;
    MbmCounter = 0;
    MbmExpectedLength = -1;
    MbmLastActivity = millis();
    MbmPos = Pos;
    MbmBitCount = Count;
  } else {
    MbmClient.stop();
    modbusStats.socketErrors++;
  }
}

void ModbusTCP_RU::MbmRun()
{
  clientProcess();
}

void ModbusTCP_RU::clientProcess()
{
  if (!MbmClient.connected() && !MbmClient.available()) {
    MbmClient.stop();
    MbmCounter = 0;
    MbmExpectedLength = -1;
    return;
  }

  unsigned long now = millis();
  if (MbmCounter > 0 && (now - MbmLastActivity) > MB_TIMEOUT) {
    MbmClient.stop();
    MbmCounter = 0;
    MbmExpectedLength = -1;
    modbusStats.socketErrors++;
    return;
  }

  byte reads = 0;
  while (MbmClient.available() && reads < MB_MAX_BYTES_PER_POLL) {
    if (MbmCounter >= MB_BUFFER_SIZE) {
      MbmClient.stop();
      MbmCounter = 0;
      MbmExpectedLength = -1;
      modbusStats.socketErrors++;
      return;
    }

    MbmByteArray[MbmCounter++] = MbmClient.read();
    reads++;
    MbmLastActivity = now;

    if (MbmCounter >= 6 && MbmExpectedLength < 0) {
      MbmExpectedLength = mbWord(MbmByteArray[4], MbmByteArray[5]) + 6;
      if (MbmExpectedLength < 9 || MbmExpectedLength > MB_BUFFER_SIZE) {
        MbmClient.stop();
        MbmCounter = 0;
        MbmExpectedLength = -1;
        modbusStats.socketErrors++;
        return;
      }
    }

    if (MbmExpectedLength > 0 && MbmCounter >= MbmExpectedLength) {
      modbusStats.rxPackets++;
      MbmProcess();
      MbmClient.stop();
      MbmCounter = 0;
      MbmExpectedLength = -1;
      return;
    }
  }
}

void ModbusTCP_RU::MbmProcess()
{
  uint16_t transactionId = mbWord(MbmByteArray[0], MbmByteArray[1]);
  if (transactionId != MbmPendingTransactionId) {
    modbusStats.socketErrors++;
    return;
  }

  byte responseFc = MbmByteArray[7];
  if (responseFc & 0x80) {
    modbusStats.exceptionCount++;
    return;
  }

  MB_FC responseFunction = SetFC(responseFc);

  if (responseFunction == MB_FC_READ_COILS || responseFunction == MB_FC_READ_DISCRETE_INPUT) {
    word Count = MbmByteArray[8] * 8;
    if (MbmBitCount < Count) {
      Count = MbmBitCount;
    }

    for (word i = 0; i < Count; i++) {
      bool value = bitRead(MbmByteArray[(i / 8) + 9], i % 8);
      if (responseFunction == MB_FC_READ_COILS) {
        CoilWrite(i + MbmPos, value);
      } else {
        DiscreteWrite(i + MbmPos, value);
      }
    }
  }

  if (responseFunction == MB_FC_READ_REGISTERS || responseFunction == MB_FC_READ_INPUT_REGISTER) {
    word Pos = MbmPos;

    for (int i = 0; i < MbmByteArray[8]; i += 2) {
      word value = mbWord(MbmByteArray[i + 9], MbmByteArray[i + 10]);
      if (responseFunction == MB_FC_READ_REGISTERS) {
        Hreg(Pos, value);
      } else {
        Ireg(Pos, value);
      }
      Pos++;
    }
  }
}
#endif

void ModbusTCP_RU::MbsRun()
{
  serverProcess();
}

void ModbusTCP_RU::serverProcess()
{
  if (!MbsServerStarted) {
    MbServer.begin();
    MbsServerStarted = true;
#ifdef MB_DEBUG
    Serial.println(F("MB server started"));
#endif
  }

  for (byte slot = 0; slot < MB_MAX_CLIENTS; slot++) {
    processServerClient(slot);
  }

  acceptServerClient();
}

void ModbusTCP_RU::acceptServerClient()
{
  EthernetClient newClient = MbServer.available();
  if (!newClient) {
    return;
  }

  for (byte slot = 0; slot < MB_MAX_CLIENTS; slot++) {
    if (!serverClients[slot].client) {
      serverClients[slot].client = newClient;
      serverClients[slot].length = 0;
      serverClients[slot].expectedLength = -1;
      serverClients[slot].lastActivity = millis();
#ifdef MB_DEBUG
      Serial.println(F("MB client connected"));
#endif
      return;
    }
  }

  // EthernetServer.available() can return a client for an already tracked socket.
  // Do not stop it here: on W5100/W5500 that can tear down the active SCADA link.
  modbusStats.socketErrors++;
}

void ModbusTCP_RU::processServerClient(byte slot)
{
  ServerClientState &state = serverClients[slot];
  if (!state.client) {
    return;
  }

  unsigned long now = millis();

  if (!state.client.connected() && !state.client.available()) {
    clearServerClient(slot);
    return;
  }

  if (state.length > 0 && (now - state.lastActivity) > MB_PACKET_TIMEOUT) {
    clearServerClient(slot);
    modbusStats.socketErrors++;
    return;
  }

  if (state.length == 0 && (now - state.lastActivity) > MB_IDLE_TIMEOUT) {
    clearServerClient(slot);
    return;
  }

  byte reads = 0;
  while (state.client.available() && reads < MB_MAX_BYTES_PER_POLL) {
    if (state.length >= MB_BUFFER_SIZE) {
      clearServerClient(slot);
      modbusStats.socketErrors++;
      return;
    }

    state.buffer[state.length++] = state.client.read();
    state.lastActivity = now;
    reads++;

    if (state.length >= 6 && state.expectedLength < 0) {
      state.expectedLength = mbWord(state.buffer[4], state.buffer[5]) + 6;
      if (state.expectedLength < 8 || state.expectedLength > MB_BUFFER_SIZE) {
        clearServerClient(slot);
        modbusStats.socketErrors++;
        return;
      }
    }

    if (state.expectedLength > 0 && state.length >= state.expectedLength) {
      modbusStats.rxPackets++;
      processRequest(state);
      state.length = 0;
      state.expectedLength = -1;
      state.lastActivity = millis();
      return;
    }
  }
}

void ModbusTCP_RU::processRequest(ServerClientState &state)
{
  uint8_t *request = state.buffer;
  byte fc = request[7];
  MbsFC = SetFC(fc);

  if (MbsFC == MB_FC_NONE) {
    sendException(state.client, request, MB_EX_ILLEGAL_FUNCTION);
    MbsFC = MB_FC_NONE;
    return;
  }

  word Start = 0;
  word WordDataLength = 0;
  word ByteDataLength = 0;
  word CoilDataLength = 0;
  uint16_t MessageLength = 0;

  if (MbsFC == MB_FC_READ_COILS || MbsFC == MB_FC_READ_DISCRETE_INPUT) {
    Start = mbWord(request[8], request[9]);
    CoilDataLength = mbWord(request[10], request[11]);
    word limit = (MbsFC == MB_FC_READ_COILS) ? MB_MAX_COILS : MB_MAX_DISCRETE;

    debugRequest("RX", fc, Start, CoilDataLength);

    if (!isValidReadCount(MbsFC, CoilDataLength)) {
      sendException(state.client, request, MB_EX_ILLEGAL_DATA_VALUE);
      MbsFC = MB_FC_NONE;
      return;
    }
    if (!isRangeValid(Start, CoilDataLength, limit)) {
      sendException(state.client, request, MB_EX_ILLEGAL_DATA_ADDRESS);
      MbsFC = MB_FC_NONE;
      return;
    }

    ByteDataLength = (CoilDataLength + 7) / 8;
    request[5] = ByteDataLength + 3;
    request[8] = ByteDataLength;

    for (word iByte = 0; iByte < ByteDataLength; iByte++) {
      request[9 + iByte] = 0;
      for (byte iBit = 0; iBit < 8; iBit++) {
        word bitIndex = Start + iByte * 8 + iBit;
        if (bitIndex < (Start + CoilDataLength)) {
          bool value = (MbsFC == MB_FC_READ_COILS) ? MbCoils[bitIndex] : MbDiscreteInputs[bitIndex];
          bitWrite(request[9 + iByte], iBit, value);
        }
      }
    }

    MessageLength = ByteDataLength + 9;
    sendResponse(state.client, request, MessageLength);
    MbsFC = MB_FC_NONE;
    return;
  }

  if (MbsFC == MB_FC_READ_REGISTERS || MbsFC == MB_FC_READ_INPUT_REGISTER) {
    Start = mbWord(request[8], request[9]);
    WordDataLength = mbWord(request[10], request[11]);
    word limit = (MbsFC == MB_FC_READ_REGISTERS) ? MB_MAX_HOLDING : MB_MAX_INPUT;
    ByteDataLength = WordDataLength * 2;

    debugRequest("RX", fc, Start, WordDataLength);

    if (!isValidReadCount(MbsFC, WordDataLength)) {
      sendException(state.client, request, MB_EX_ILLEGAL_DATA_VALUE);
      MbsFC = MB_FC_NONE;
      return;
    }
    if (!isRangeValid(Start, WordDataLength, limit)) {
      sendException(state.client, request, MB_EX_ILLEGAL_DATA_ADDRESS);
      MbsFC = MB_FC_NONE;
      return;
    }

    request[5] = ByteDataLength + 3;
    request[8] = ByteDataLength;

    for (word iWord = 0; iWord < WordDataLength; iWord++) {
      word registerIndex = Start + iWord;
      word value = (MbsFC == MB_FC_READ_REGISTERS) ? MbHoldingRegisters[registerIndex] : MbInputRegisters[registerIndex];
      request[9 + iWord * 2] = highByte(value);
      request[10 + iWord * 2] = lowByte(value);
    }

    MessageLength = ByteDataLength + 9;
    sendResponse(state.client, request, MessageLength);
    MbsFC = MB_FC_NONE;
    return;
  }

  if (MbsFC == MB_FC_WRITE_COIL) {
    Start = mbWord(request[8], request[9]);
    word value = mbWord(request[10], request[11]);

    debugRequest("RX", fc, Start, 1);

    if (!isRangeValid(Start, 1, MB_MAX_COILS)) {
      sendException(state.client, request, MB_EX_ILLEGAL_DATA_ADDRESS);
      MbsFC = MB_FC_NONE;
      return;
    }
    if (value != 0xFF00 && value != 0x0000) {
      sendException(state.client, request, MB_EX_ILLEGAL_DATA_VALUE);
      MbsFC = MB_FC_NONE;
      return;
    }

    CoilWrite(Start, value == 0xFF00);
    request[5] = 6;
    sendResponse(state.client, request, 12);
    MbsFC = MB_FC_NONE;
    return;
  }

  if (MbsFC == MB_FC_WRITE_REGISTER) {
    Start = mbWord(request[8], request[9]);

    debugRequest("RX", fc, Start, 1);

    if (!isRangeValid(Start, 1, MB_MAX_HOLDING)) {
      sendException(state.client, request, MB_EX_ILLEGAL_DATA_ADDRESS);
      MbsFC = MB_FC_NONE;
      return;
    }

    Hreg(Start, mbWord(request[10], request[11]));
    request[5] = 6;
    sendResponse(state.client, request, 12);
    MbsFC = MB_FC_NONE;
    return;
  }

  if (MbsFC == MB_FC_WRITE_MULTIPLE_COILS) {
    Start = mbWord(request[8], request[9]);
    CoilDataLength = mbWord(request[10], request[11]);
    ByteDataLength = (CoilDataLength + 7) / 8;

    debugRequest("RX", fc, Start, CoilDataLength);

    if (!isValidWriteCount(MbsFC, CoilDataLength) || request[12] != ByteDataLength) {
      sendException(state.client, request, MB_EX_ILLEGAL_DATA_VALUE);
      MbsFC = MB_FC_NONE;
      return;
    }
    if (!isRangeValid(Start, CoilDataLength, MB_MAX_COILS)) {
      sendException(state.client, request, MB_EX_ILLEGAL_DATA_ADDRESS);
      MbsFC = MB_FC_NONE;
      return;
    }
    if ((13 + ByteDataLength) > state.length) {
      sendException(state.client, request, MB_EX_ILLEGAL_DATA_VALUE);
      MbsFC = MB_FC_NONE;
      return;
    }

    for (word iBit = 0; iBit < CoilDataLength; iBit++) {
      CoilWrite(Start + iBit, bitRead(request[13 + (iBit / 8)], iBit % 8));
    }

    request[5] = 6;
    sendResponse(state.client, request, 12);
    MbsFC = MB_FC_NONE;
    return;
  }

  if (MbsFC == MB_FC_WRITE_MULTIPLE_REGISTERS) {
    Start = mbWord(request[8], request[9]);
    WordDataLength = mbWord(request[10], request[11]);
    ByteDataLength = WordDataLength * 2;

    debugRequest("RX", fc, Start, WordDataLength);

    if (!isValidWriteCount(MbsFC, WordDataLength) || request[12] != ByteDataLength) {
      sendException(state.client, request, MB_EX_ILLEGAL_DATA_VALUE);
      MbsFC = MB_FC_NONE;
      return;
    }
    if (!isRangeValid(Start, WordDataLength, MB_MAX_HOLDING)) {
      sendException(state.client, request, MB_EX_ILLEGAL_DATA_ADDRESS);
      MbsFC = MB_FC_NONE;
      return;
    }
    if ((13 + ByteDataLength) > state.length) {
      sendException(state.client, request, MB_EX_ILLEGAL_DATA_VALUE);
      MbsFC = MB_FC_NONE;
      return;
    }

    for (word iWord = 0; iWord < WordDataLength; iWord++) {
      Hreg(Start + iWord, mbWord(request[13 + iWord * 2], request[14 + iWord * 2]));
    }

    request[5] = 6;
    sendResponse(state.client, request, 12);
    MbsFC = MB_FC_NONE;
    return;
  }
}

void ModbusTCP_RU::sendException(EthernetClient &client, uint8_t *request, byte exceptionCode)
{
  request[4] = 0;
  request[5] = 3;
  request[7] = request[7] | 0x80;
  request[8] = exceptionCode;
  debugException(request[7], exceptionCode);
  client.write(request, 9);
  modbusStats.txPackets++;
  modbusStats.exceptionCount++;
}

void ModbusTCP_RU::sendResponse(EthernetClient &client, uint8_t *response, uint16_t length)
{
  client.write(response, length);
  modbusStats.txPackets++;
}

void ModbusTCP_RU::clearServerClient(byte slot)
{
  if (serverClients[slot].client) {
    serverClients[slot].client.stop();
#ifdef MB_DEBUG
    Serial.println(F("MB client disconnected"));
#endif
  }
  serverClients[slot].length = 0;
  serverClients[slot].expectedLength = -1;
  serverClients[slot].lastActivity = 0;
}

MB_FC ModbusTCP_RU::SetFC(int fc)
{
  MB_FC FC = MB_FC_NONE;
  if (fc == 1) FC = MB_FC_READ_COILS;
  if (fc == 2) FC = MB_FC_READ_DISCRETE_INPUT;
  if (fc == 3) FC = MB_FC_READ_REGISTERS;
  if (fc == 4) FC = MB_FC_READ_INPUT_REGISTER;
  if (fc == 5) FC = MB_FC_WRITE_COIL;
  if (fc == 6) FC = MB_FC_WRITE_REGISTER;
  if (fc == 15) FC = MB_FC_WRITE_MULTIPLE_COILS;
  if (fc == 16) FC = MB_FC_WRITE_MULTIPLE_REGISTERS;
  return FC;
}

word ModbusTCP_RU::GetDataLen()
{
  return MB_MAX_HOLDING;
}

bool ModbusTCP_RU::Coil(word address) const
{
  return CoilRead(address);
}

bool ModbusTCP_RU::Coil(word address, bool value)
{
  return CoilWrite(address, value);
}

bool ModbusTCP_RU::Discrete(word address) const
{
  return DiscreteRead(address);
}

bool ModbusTCP_RU::Discrete(word address, bool value)
{
  return DiscreteWrite(address, value);
}

word ModbusTCP_RU::Hreg(word address) const
{
  if (address >= MB_MAX_HOLDING) {
    return 0;
  }
  return MbHoldingRegisters[address];
}

bool ModbusTCP_RU::Hreg(word address, word value)
{
  if (address >= MB_MAX_HOLDING) {
    return false;
  }
  MbHoldingRegisters[address] = value;
  if (holdingWriteCallback) {
    holdingWriteCallback(address, value);
  }
  return true;
}

word ModbusTCP_RU::Ireg(word address) const
{
  if (address >= MB_MAX_INPUT) {
    return 0;
  }
  return MbInputRegisters[address];
}

bool ModbusTCP_RU::Ireg(word address, word value)
{
  if (address >= MB_MAX_INPUT) {
    return false;
  }
  MbInputRegisters[address] = value;
  return true;
}

bool ModbusTCP_RU::CoilRead(word address) const
{
  if (address >= MB_MAX_COILS) {
    return false;
  }
  return MbCoils[address];
}

bool ModbusTCP_RU::CoilWrite(word address, bool value)
{
  if (address >= MB_MAX_COILS) {
    return false;
  }
  MbCoils[address] = value;
  if (coilWriteCallback) {
    coilWriteCallback(address, value);
  }
  return true;
}

bool ModbusTCP_RU::DiscreteRead(word address) const
{
  if (address >= MB_MAX_DISCRETE) {
    return false;
  }
  return MbDiscreteInputs[address];
}

bool ModbusTCP_RU::DiscreteWrite(word address, bool value)
{
  if (address >= MB_MAX_DISCRETE) {
    return false;
  }
  MbDiscreteInputs[address] = value;
  return true;
}

boolean ModbusTCP_RU::GetBit(word Number)
{
  return CoilRead(Number);
}

boolean ModbusTCP_RU::SetBit(word Number, boolean Data)
{
  return !CoilWrite(Number, Data);
}

void ModbusTCP_RU::onCoilWrite(word address, ModbusCoilWriteCallback callback)
{
  (void)address;
  coilWriteCallback = callback;
}

void ModbusTCP_RU::onHoldingWrite(word address, ModbusHoldingWriteCallback callback)
{
  (void)address;
  holdingWriteCallback = callback;
}

void ModbusTCP_RU::onCoilWrite(ModbusCoilWriteCallback callback)
{
  coilWriteCallback = callback;
}

void ModbusTCP_RU::onHoldingWrite(ModbusHoldingWriteCallback callback)
{
  holdingWriteCallback = callback;
}

uint32_t ModbusTCP_RU::ReadUInt32(word address, MB_WORD_ORDER order) const
{
  if ((address + 1) >= MB_MAX_HOLDING) {
    return 0;
  }

  word highWord = MbHoldingRegisters[address];
  word lowWord = MbHoldingRegisters[address + 1];
  if (order == MB_WORD_ORDER_SWAPPED) {
    word tmp = highWord;
    highWord = lowWord;
    lowWord = tmp;
  }

  return ((uint32_t)highWord << 16) | lowWord;
}

int32_t ModbusTCP_RU::ReadInt32(word address, MB_WORD_ORDER order) const
{
  return (int32_t)ReadUInt32(address, order);
}

float ModbusTCP_RU::ReadFloat(word address, MB_WORD_ORDER order) const
{
  word regs[2];
  if ((address + 1) >= MB_MAX_HOLDING) {
    return 0.0f;
  }
  regs[0] = MbHoldingRegisters[address];
  regs[1] = MbHoldingRegisters[address + 1];
  return regsToFloat(regs, order);
}

bool ModbusTCP_RU::WriteUInt32(word address, uint32_t value, MB_WORD_ORDER order)
{
  if ((address + 1) >= MB_MAX_HOLDING) {
    return false;
  }

  word highWord = (word)(value >> 16);
  word lowWord = (word)(value & 0xFFFF);
  if (order == MB_WORD_ORDER_SWAPPED) {
    Hreg(address, lowWord);
    Hreg(address + 1, highWord);
  } else {
    Hreg(address, highWord);
    Hreg(address + 1, lowWord);
  }
  return true;
}

bool ModbusTCP_RU::WriteInt32(word address, int32_t value, MB_WORD_ORDER order)
{
  return WriteUInt32(address, (uint32_t)value, order);
}

bool ModbusTCP_RU::WriteFloat(word address, float value, MB_WORD_ORDER order)
{
  if ((address + 1) >= MB_MAX_HOLDING) {
    return false;
  }

  word regs[2];
  floatToRegs(value, regs, order);
  Hreg(address, regs[0]);
  Hreg(address + 1, regs[1]);
  return true;
}

void ModbusTCP_RU::floatToRegs(float value, word *regs, MB_WORD_ORDER order)
{
  uint32_t raw;
  memcpy(&raw, &value, sizeof(raw));
  regs[0] = (word)(raw >> 16);
  regs[1] = (word)(raw & 0xFFFF);

  if (order == MB_WORD_ORDER_SWAPPED) {
    word tmp = regs[0];
    regs[0] = regs[1];
    regs[1] = tmp;
  }
}

float ModbusTCP_RU::regsToFloat(const word *regs, MB_WORD_ORDER order)
{
  word highWord = regs[0];
  word lowWord = regs[1];
  if (order == MB_WORD_ORDER_SWAPPED) {
    highWord = regs[1];
    lowWord = regs[0];
  }

  uint32_t raw = ((uint32_t)highWord << 16) | lowWord;
  float value;
  memcpy(&value, &raw, sizeof(value));
  return value;
}

const ModbusStats& ModbusTCP_RU::stats() const
{
  return modbusStats;
}

void ModbusTCP_RU::resetStats()
{
  memset(&modbusStats, 0, sizeof(modbusStats));
}

bool ModbusTCP_RU::IsValidRegister(word index)
{
  return index < MB_MAX_HOLDING;
}

bool ModbusTCP_RU::IsValidBit(word index)
{
  return index < MB_MAX_COILS;
}

bool ModbusTCP_RU::isRangeValid(word start, word count, word limit)
{
  if (count == 0 || start >= limit) {
    return false;
  }
  return count <= (limit - start);
}

bool ModbusTCP_RU::isValidReadCount(MB_FC fc, word count)
{
  if (count < 1) {
    return false;
  }
  if (fc == MB_FC_READ_COILS || fc == MB_FC_READ_DISCRETE_INPUT) {
    return count <= MB_PROTOCOL_MAX_READ_BITS;
  }
  return count <= MB_PROTOCOL_MAX_READ_REGISTERS;
}

bool ModbusTCP_RU::isValidWriteCount(MB_FC fc, word count)
{
  if (count < 1) {
    return false;
  }
  if (fc == MB_FC_WRITE_MULTIPLE_COILS) {
    return count <= MB_PROTOCOL_MAX_WRITE_COILS;
  }
  return count <= MB_PROTOCOL_MAX_WRITE_REGISTERS;
}

void ModbusTCP_RU::debugRequest(const char *prefix, byte fc, word address, word count)
{
#ifdef MB_DEBUG
  Serial.print(prefix);
  Serial.print(F(" FC="));
  Serial.print(fc);
  Serial.print(F(" addr="));
  Serial.print(address);
  Serial.print(F(" count="));
  Serial.println(count);
#else
  (void)prefix;
  (void)fc;
  (void)address;
  (void)count;
#endif
}

void ModbusTCP_RU::debugException(byte fc, byte exceptionCode)
{
#ifdef MB_DEBUG
  Serial.print(F("MB exception FC="));
  Serial.print(fc & 0x7F);
  Serial.print(F(" code="));
  Serial.println(exceptionCode);
#else
  (void)fc;
  (void)exceptionCode;
#endif
}
