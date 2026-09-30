#include "ModbusTCP_RU.h"

// One Modbus server per Ethernet interface. Retained for link compatibility only.
EthernetServer MbServer(MB_PORT);

static word mbWord(byte high, byte low)
{
  return ((word)high << 8) | low;
}

ModbusTCP_RU::ModbusTCP_RU(ModbusBuildConfig *)
  : MbData(MbHoldingRegisters),
    idleTimeout(MB_IDLE_TIMEOUT), packetTimeout(MB_PACKET_TIMEOUT),
    responseTimeout(MB_TX_TIMEOUT), notifyingCoil(false), notifyingHolding(false),
    coilWriteCallback(0), holdingWriteCallback(0)
{
  memset(MbCoils, 0, sizeof(MbCoils));
  memset(MbDiscreteInputs, 0, sizeof(MbDiscreteInputs));
  memset(MbHoldingRegisters, 0, sizeof(MbHoldingRegisters));
  memset(MbInputRegisters, 0, sizeof(MbInputRegisters));
  resetStats();
  for (byte slot = 0; slot < MB_MAX_CLIENTS; ++slot) {
    serverClients[slot].length = 0;
    serverClients[slot].expectedLength = 0;
    serverClients[slot].responsePending = false;
    serverClients[slot].frameStarted = 0;
    serverClients[slot].lastActivity = 0;
  }
}

bool ModbusTCP_RU::begin()
{
  if (notifyingCoil || notifyingHolding) return false;
  if (!MbServer) MbServer.begin();
  return (bool)MbServer;
}

bool ModbusTCP_RU::restart()
{
  if (notifyingCoil || notifyingHolding) return false;
  for (byte slot = 0; slot < MB_MAX_CLIENTS; ++slot) clearServerClient(slot);
  // Drain unclaimed connections too. accept() retains/recreates a single listener.
  for (byte i = 0; i < MAX_SOCK_NUM; ++i) {
    EthernetClient client = MbServer.accept();
    if (!client) break;
    client.setConnectionTimeout(0);
    client.stop();
  }
  return begin();
}

void ModbusTCP_RU::setIdleTimeout(uint32_t milliseconds) { idleTimeout = milliseconds; }
void ModbusTCP_RU::setPacketTimeout(uint32_t milliseconds) { packetTimeout = milliseconds; }
void ModbusTCP_RU::setResponseTimeout(uint32_t milliseconds) { responseTimeout = milliseconds; }

void ModbusTCP_RU::MbsRun() { serverProcess(); }

void ModbusTCP_RU::serverProcess()
{
  if (notifyingCoil || notifyingHolding) return;
  begin();
  for (byte slot = 0; slot < MB_MAX_CLIENTS; ++slot) processServerClient(slot);
  acceptServerClient();
}

void ModbusTCP_RU::acceptServerClient()
{
  EthernetClient client = MbServer.accept();
  if (!client) return;
  client.setConnectionTimeout(0);
  for (byte slot = 0; slot < MB_MAX_CLIENTS; ++slot) {
#if MB_MAX_CLIENTS > 3
    // On 32-bit boards MAX_SOCK_NUM may be 8 even with a four-socket W5100.
    if (slot == 3 && Ethernet.hardwareStatus() == EthernetW5100) break;
#endif
    ServerClientState &state = serverClients[slot];
    if (state.client) continue;
    state.client = client;
    state.length = 0;
    state.expectedLength = 0;
    state.responsePending = false;
    state.lastActivity = millis();
    state.frameStarted = 0;
#ifdef MB_DEBUG
    Serial.println(F("MB client connected"));
#endif
    return;
  }
  ++modbusStats.rejectedClients;
  client.stop();
}

void ModbusTCP_RU::clearServerClient(byte slot)
{
  ServerClientState &state = serverClients[slot];
  if (state.client) {
    state.client.setConnectionTimeout(0);
    state.client.stop();
#ifdef MB_DEBUG
    Serial.println(F("MB client disconnected"));
#endif
  }
  state.length = 0;
  state.expectedLength = 0;
  state.responsePending = false;
  state.lastActivity = 0;
  state.frameStarted = 0;
}

void ModbusTCP_RU::processServerClient(byte slot)
{
  ServerClientState &state = serverClients[slot];
  if (!state.client) return;
  // CLOSE_WAIT can still send a queued reply after the peer shuts down its TX side.
  if (state.responsePending) {
    transmitResponse(slot);
    return;
  }
  if (!state.client.connected() && !state.client.available()) {
    clearServerClient(slot);
    return;
  }
  const uint32_t now = millis();
  // A total deadline also prevents a slow peer from holding a slot with single bytes.
  if (state.length && (uint32_t)(now - state.frameStarted) >= MB_FRAME_TIMEOUT) {
    ++modbusStats.timeouts;
    clearServerClient(slot);
    return;
  }
  if (!state.client.available()) {
    const uint32_t timeout = state.length ? packetTimeout : idleTimeout;
    if (timeout && (uint32_t)(now - state.lastActivity) >= timeout) {
      ++modbusStats.timeouts;
      clearServerClient(slot);
    }
    return;
  }
  // Read queued bytes before applying an inter-byte timeout: loop may have been busy.
  for (uint16_t reads = 0; reads < MB_MAX_BYTES_PER_POLL && state.client.available(); ++reads) {
    const int value = state.client.read();
    if (value < 0) break;
    if (!state.length) state.frameStarted = now;
    state.buffer[state.length++] = (uint8_t)value;
    state.lastActivity = now;
    if (state.length == 6) {
      const word bodyLength = mbWord(state.buffer[4], state.buffer[5]);
      if (state.buffer[2] || state.buffer[3] || bodyLength < 2 ||
          bodyLength > 254 || bodyLength > MB_BUFFER_SIZE - 6) {
        ++modbusStats.malformedFrames;
        clearServerClient(slot);
        return;
      }
      state.expectedLength = bodyLength + 6;
    }
    if (state.expectedLength && state.length == state.expectedLength) {
      ++modbusStats.rxPackets;
      processRequest(state);
      transmitResponse(slot);
      return;
    }
  }
}

void ModbusTCP_RU::processRequest(ServerClientState &state)
{
  uint8_t *request = state.buffer;
  const byte fc = request[7];
  const bool multiple = fc == 15 || fc == 16;
  if (!(fc >= 1 && fc <= 6) && !multiple) {
    sendException(state, MB_EX_ILLEGAL_FUNCTION);
    return;
  }
  if ((!multiple && state.length != 12) || (multiple && state.length < 13)) {
    ++modbusStats.malformedFrames;
    sendException(state, MB_EX_ILLEGAL_DATA_VALUE);
    return;
  }
  const word start = mbWord(request[8], request[9]);
  const word data = mbWord(request[10], request[11]);
  debugRequest("RX", fc, start, data);

  if (fc == 1 || fc == 2) {
    const word limit = fc == 1 ? MB_MAX_COILS : MB_MAX_DISCRETE;
    if (!isValidReadCount((MB_FC)fc, data)) {
      sendException(state, MB_EX_ILLEGAL_DATA_VALUE);
      return;
    }
    if (!isRangeValid(start, data, limit)) {
      sendException(state, MB_EX_ILLEGAL_DATA_ADDRESS);
      return;
    }
    const word bytes = (data + 7) / 8;
    if (bytes > MB_BUFFER_SIZE - 9) {
      sendException(state, MB_EX_ILLEGAL_DATA_VALUE);
      return;
    }
    request[8] = (byte)bytes;
    memset(request + 9, 0, bytes);
    for (word i = 0; i < data; ++i) {
      const bool value = fc == 1 ? MbCoils[start + i] : MbDiscreteInputs[start + i];
      bitWrite(request[9 + i / 8], i % 8, value);
    }
    sendResponse(state, bytes + 9);
    return;
  }

  if (fc == 3 || fc == 4) {
    const word limit = fc == 3 ? MB_MAX_HOLDING : MB_MAX_INPUT;
    if (!isValidReadCount((MB_FC)fc, data)) {
      sendException(state, MB_EX_ILLEGAL_DATA_VALUE);
      return;
    }
    if (!isRangeValid(start, data, limit)) {
      sendException(state, MB_EX_ILLEGAL_DATA_ADDRESS);
      return;
    }
    const word bytes = data * 2;
    if (bytes > MB_BUFFER_SIZE - 9) {
      sendException(state, MB_EX_ILLEGAL_DATA_VALUE);
      return;
    }
    request[8] = (byte)bytes;
    for (word i = 0; i < data; ++i) {
      const word value = fc == 3 ? MbHoldingRegisters[start + i] : MbInputRegisters[start + i];
      request[9 + i * 2] = highByte(value);
      request[10 + i * 2] = lowByte(value);
    }
    sendResponse(state, bytes + 9);
    return;
  }

  if (fc == 5 || fc == 6) {
    if (!isRangeValid(start, 1, fc == 5 ? MB_MAX_COILS : MB_MAX_HOLDING)) {
      sendException(state, MB_EX_ILLEGAL_DATA_ADDRESS);
      return;
    }
    if (fc == 5 && data != 0 && data != 0xFF00) {
      sendException(state, MB_EX_ILLEGAL_DATA_VALUE);
      return;
    }
    if (fc == 5) CoilWrite(start, data == 0xFF00);
    else Hreg(start, data);
    sendResponse(state, 12);
    return;
  }

  if (!isValidWriteCount((MB_FC)fc, data)) {
    sendException(state, MB_EX_ILLEGAL_DATA_VALUE);
    return;
  }
  const word bytes = fc == 15 ? (data + 7) / 8 : data * 2;
  if (request[12] != bytes || state.length != 13 + bytes) {
    ++modbusStats.malformedFrames;
    sendException(state, MB_EX_ILLEGAL_DATA_VALUE);
    return;
  }
  if (!isRangeValid(start, data, fc == 15 ? MB_MAX_COILS : MB_MAX_HOLDING)) {
    sendException(state, MB_EX_ILLEGAL_DATA_ADDRESS);
    return;
  }
  // Commit the entire validated range before notifying application callbacks.
  for (word i = 0; i < data; ++i) {
    if (fc == 15) setCoilLocal(start + i, bitRead(request[13 + i / 8], i % 8));
    else setHoldingLocal(start + i, mbWord(request[13 + i * 2], request[14 + i * 2]));
  }
  for (word i = 0; i < data; ++i) {
    if (fc == 15) notifyCoil(start + i, bitRead(request[13 + i / 8], i % 8));
    else notifyHolding(start + i, mbWord(request[13 + i * 2], request[14 + i * 2]));
  }
  sendResponse(state, 12);
}

void ModbusTCP_RU::sendResponse(ServerClientState &state, uint16_t length)
{
  state.buffer[4] = highByte(length - 6);
  state.buffer[5] = lowByte(length - 6);
  state.length = length;
  state.expectedLength = 0;
  state.responsePending = true;
  state.lastActivity = millis();
}

void ModbusTCP_RU::sendException(ServerClientState &state, byte exceptionCode)
{
  state.buffer[7] |= 0x80;
  state.buffer[8] = exceptionCode;
  debugException(state.buffer[7], exceptionCode);
  ++modbusStats.exceptionCount;
  sendResponse(state, 9);
}

void ModbusTCP_RU::transmitResponse(byte slot)
{
  ServerClientState &state = serverClients[slot];
  if (!state.responsePending) return;
  if (responseTimeout && (uint32_t)(millis() - state.lastActivity) >= responseTimeout) {
    ++modbusStats.timeouts;
    ++modbusStats.socketErrors;
    clearServerClient(slot);
    return;
  }
  const int available = state.client.availableForWrite();
  if (available < 0 || (uint16_t)available < state.length) return;
  // Ethernet 2.x still waits for hardware SEND_OK inside write().
  if (state.client.write(state.buffer, state.length) != state.length) {
    ++modbusStats.socketErrors;
    clearServerClient(slot);
    return;
  }
  ++modbusStats.txPackets;
  state.length = 0;
  state.responsePending = false;
  state.frameStarted = 0;
  state.lastActivity = millis();
}

bool ModbusTCP_RU::setCoilLocal(word address, bool value)
{
  if (address >= MB_MAX_COILS) return false;
  MbCoils[address] = value;
  return true;
}

bool ModbusTCP_RU::setHoldingLocal(word address, word value)
{
  if (address >= MB_MAX_HOLDING) return false;
  MbHoldingRegisters[address] = value;
  return true;
}

void ModbusTCP_RU::notifyCoil(word address, bool value)
{
  if (!coilWriteCallback || notifyingCoil) return;
  notifyingCoil = true;
  coilWriteCallback(address, value);
  notifyingCoil = false;
}

void ModbusTCP_RU::notifyHolding(word address, word value)
{
  if (!holdingWriteCallback || notifyingHolding) return;
  notifyingHolding = true;
  holdingWriteCallback(address, value);
  notifyingHolding = false;
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
  if (!setHoldingLocal(address, value)) return false;
  notifyHolding(address, value);
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
  if (!setCoilLocal(address, value)) return false;
  notifyCoil(address, value);
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
  if (!isRangeValid(address, 2, MB_MAX_HOLDING)) {
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
  if (!isRangeValid(address, 2, MB_MAX_HOLDING)) {
    return 0.0f;
  }
  regs[0] = MbHoldingRegisters[address];
  regs[1] = MbHoldingRegisters[address + 1];
  return regsToFloat(regs, order);
}

bool ModbusTCP_RU::WriteUInt32(word address, uint32_t value, MB_WORD_ORDER order)
{
  if (!isRangeValid(address, 2, MB_MAX_HOLDING)) {
    return false;
  }

  word highWord = (word)(value >> 16);
  word lowWord = (word)(value & 0xFFFF);
  if (order == MB_WORD_ORDER_SWAPPED) {
    setHoldingLocal(address, lowWord);
    setHoldingLocal(address + 1, highWord);
  } else {
    setHoldingLocal(address, highWord);
    setHoldingLocal(address + 1, lowWord);
  }
  notifyHolding(address, MbHoldingRegisters[address]);
  notifyHolding(address + 1, MbHoldingRegisters[address + 1]);
  return true;
}

bool ModbusTCP_RU::WriteInt32(word address, int32_t value, MB_WORD_ORDER order)
{
  return WriteUInt32(address, (uint32_t)value, order);
}

bool ModbusTCP_RU::WriteFloat(word address, float value, MB_WORD_ORDER order)
{
  if (!isRangeValid(address, 2, MB_MAX_HOLDING)) {
    return false;
  }

  word regs[2];
  floatToRegs(value, regs, order);
  setHoldingLocal(address, regs[0]);
  setHoldingLocal(address + 1, regs[1]);
  notifyHolding(address, regs[0]);
  notifyHolding(address + 1, regs[1]);
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
