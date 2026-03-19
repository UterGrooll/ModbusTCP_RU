#include "ModbusTCP_RU.h"

EthernetServer MbServer(MB_PORT);
EthernetClient MbmClient;

// #define DEBUG

#define MB_PACKET_TIMEOUT 50
#define MB_READ_TIMEOUT 1000

ModbusTCP_RU::ModbusTCP_RU()
  : remSlaveIP(0, 0, 0, 0),
    MbmFC(MB_FC_NONE),
    MbmCounter(0),
    MbmPos(0),
    MbmBitCount(0),
    MbsFC(MB_FC_NONE),
    MbsServerStarted(false)
{
}

void ModbusTCP_RU::Req(MB_FC FC, word Ref, word Count, word Pos)
{
  if (remSlaveIP == IPAddress(0, 0, 0, 0)) {
    return;
  }

  MbmFC = FC;

  MbmByteArray[0] = 0;
  MbmByteArray[1] = 1;
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
    if (Count > MB_MAX_COILS) { Count = MB_MAX_COILS; }
    MbmByteArray[10] = highByte(Count);
    MbmByteArray[11] = lowByte(Count);
  }

  if (FC == MB_FC_READ_REGISTERS || FC == MB_FC_READ_INPUT_REGISTER) {
    if (Count < 1) { Count = 1; }
    if (Count > MB_MAX_REGISTERS) { Count = MB_MAX_REGISTERS; }
    MbmByteArray[10] = highByte(Count);
    MbmByteArray[11] = lowByte(Count);
  }

  if (MbmFC == MB_FC_WRITE_COIL) {
    MbmByteArray[10] = GetBit(Pos) ? 0xFF : 0x00;
    MbmByteArray[11] = 0;
  }

  if (MbmFC == MB_FC_WRITE_REGISTER) {
    if (!IsValidRegister(Pos)) {
      return;
    }
    MbmByteArray[10] = highByte(MbData[Pos]);
    MbmByteArray[11] = lowByte(MbData[Pos]);
  }

  if (MbmFC == MB_FC_WRITE_MULTIPLE_COILS) {
    byte byteCount;

    if (Count < 1) { Count = 1; }
    if (Count > MB_MAX_MULTIPLE_COILS) { Count = MB_MAX_MULTIPLE_COILS; }

    byteCount = (Count + 7) / 8;
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

    for (int i = 0; i < Count; i++) {
      bitWrite(MbmByteArray[13 + (i / 8)], i % 8, GetBit(Pos + i));
    }
  }

  if (MbmFC == MB_FC_WRITE_MULTIPLE_REGISTERS) {
    word byteCount;

    if (Count < 1) { Count = 1; }
    if (Count > MB_MAX_MULTIPLE_REGISTERS) { Count = MB_MAX_MULTIPLE_REGISTERS; }

    byteCount = Count * 2;
    if ((13 + byteCount) > MB_BUFFER_SIZE) {
      return;
    }

    MbmByteArray[10] = highByte(Count);
    MbmByteArray[11] = lowByte(Count);
    MbmByteArray[12] = byteCount;
    MbmByteArray[4] = highByte(byteCount + 7);
    MbmByteArray[5] = lowByte(byteCount + 7);

    for (int i = 0; i < Count; i++) {
      if (!IsValidRegister(Pos + i)) {
        return;
      }
      MbmByteArray[(i * 2) + 13] = highByte(MbData[Pos + i]);
      MbmByteArray[(i * 2) + 14] = lowByte(MbData[Pos + i]);
    }
  }

  if (MbmClient.connect(remSlaveIP, MB_PORT)) {
#ifdef DEBUG
    Serial.println("Connected to Modbus slave");
#endif

    for (int i = 0; i < MbmByteArray[5] + 6; i++) {
      MbmClient.write(MbmByteArray[i]);
    }

    MbmCounter = 0;
    MbmByteArray[7] = 0;
    MbmPos = Pos;
    MbmBitCount = Count;
  } else {
    MbmClient.stop();
  }
}

void ModbusTCP_RU::MbmRun()
{
  unsigned long startTime = millis();

  while (MbmClient.connected() && (millis() - startTime) <= MB_READ_TIMEOUT) {
    while (MbmClient.available()) {
      if (MbmCounter >= MB_BUFFER_SIZE) {
        MbmClient.stop();
        MbmCounter = 0;
        return;
      }

      MbmByteArray[MbmCounter] = MbmClient.read();

      if (MbmCounter > 4 && MbmCounter == MbmByteArray[5] + 5) {
        MbmClient.stop();
        MbmProcess();
        MbmCounter = 0;
        return;
      }

      MbmCounter++;
      startTime = millis();
    }
  }

  MbmClient.stop();
  MbmCounter = 0;
}

void ModbusTCP_RU::MbmProcess()
{
  MbmFC = SetFC(int(MbmByteArray[7]));

  if (MbmFC == MB_FC_READ_COILS || MbmFC == MB_FC_READ_DISCRETE_INPUT) {
    word Count = MbmByteArray[8] * 8;
    if (MbmBitCount < Count) {
      Count = MbmBitCount;
    }

    for (int i = 0; i < Count; i++) {
      if (IsValidBit(i + MbmPos)) {
        SetBit(i + MbmPos, bitRead(MbmByteArray[(i / 8) + 9], i % 8));
      }
    }
  }

  if (MbmFC == MB_FC_READ_REGISTERS || MbmFC == MB_FC_READ_INPUT_REGISTER) {
    word Pos = MbmPos;

    for (int i = 0; i < MbmByteArray[8]; i += 2) {
      if (!IsValidRegister(Pos)) {
        break;
      }
      MbData[Pos] = (MbmByteArray[i + 9] * 0x100) + MbmByteArray[i + 10];
      Pos++;
    }
  }
}

void ModbusTCP_RU::MbsRun()
{
  if (!MbsServerStarted) {
    MbServer.begin();
    MbsServerStarted = true;
  }

  EthernetClient client = MbServer.available();
  if (!client) {
    return;
  }

  unsigned long startTime = millis();
  int i = 0;
  int expectedLength = -1;

  while ((millis() - startTime) <= MB_PACKET_TIMEOUT && i < MB_BUFFER_SIZE) {
    if (client.available()) {
      MbsByteArray[i++] = client.read();
      startTime = millis();

      if (i >= 6 && expectedLength < 0) {
        expectedLength = word(MbsByteArray[4], MbsByteArray[5]) + 6;
        if (expectedLength > MB_BUFFER_SIZE) {
          client.stop();
          MbsFC = MB_FC_NONE;
          return;
        }
      }

      if (expectedLength > 0 && i >= expectedLength) {
        break;
      }
    }
  }

  if (i < 8) {
    MbsFC = MB_FC_NONE;
    return;
  }

  MbsFC = SetFC(MbsByteArray[7]);
  if (MbsFC == MB_FC_NONE) {
    return;
  }

  int Start;
  int WordDataLength;
  int ByteDataLength;
  int CoilDataLength;
  int MessageLength;

  if (MbsFC == MB_FC_READ_COILS || MbsFC == MB_FC_READ_DISCRETE_INPUT) {
    Start = word(MbsByteArray[8], MbsByteArray[9]);
    CoilDataLength = word(MbsByteArray[10], MbsByteArray[11]);
    ByteDataLength = CoilDataLength / 8;
    if (ByteDataLength * 8 < CoilDataLength) {
      ByteDataLength++;
    }
    if ((9 + ByteDataLength) > MB_BUFFER_SIZE) {
      MbsFC = MB_FC_NONE;
      return;
    }

    MbsByteArray[5] = ByteDataLength + 3;
    MbsByteArray[8] = ByteDataLength;

    for (int iByte = 0; iByte < ByteDataLength; iByte++) {
      MbsByteArray[9 + iByte] = 0;
      for (int iBit = 0; iBit < 8; iBit++) {
        word bitIndex = Start + iByte * 8 + iBit;
        bitWrite(MbsByteArray[9 + iByte], iBit, IsValidBit(bitIndex) ? GetBit(bitIndex) : 0);
      }
    }

    MessageLength = ByteDataLength + 9;
    client.write(MbsByteArray, MessageLength);
    MbsFC = MB_FC_NONE;
  }

  if (MbsFC == MB_FC_READ_REGISTERS || MbsFC == MB_FC_READ_INPUT_REGISTER) {
    Start = word(MbsByteArray[8], MbsByteArray[9]);
    WordDataLength = word(MbsByteArray[10], MbsByteArray[11]);
    ByteDataLength = WordDataLength * 2;

    if ((9 + ByteDataLength) > MB_BUFFER_SIZE) {
      MbsFC = MB_FC_NONE;
      return;
    }

    MbsByteArray[5] = ByteDataLength + 3;
    MbsByteArray[8] = ByteDataLength;

    for (int iWord = 0; iWord < WordDataLength; iWord++) {
      word registerIndex = Start + iWord;
      word value = IsValidRegister(registerIndex) ? MbData[registerIndex] : 0;
      MbsByteArray[9 + iWord * 2] = highByte(value);
      MbsByteArray[10 + iWord * 2] = lowByte(value);
    }

    MessageLength = ByteDataLength + 9;
    client.write(MbsByteArray, MessageLength);
    MbsFC = MB_FC_NONE;
  }

  if (MbsFC == MB_FC_WRITE_COIL) {
    Start = word(MbsByteArray[8], MbsByteArray[9]);
    if (word(MbsByteArray[10], MbsByteArray[11]) == 0xFF00) {
      SetBit(Start, true);
    }
    if (word(MbsByteArray[10], MbsByteArray[11]) == 0x0000) {
      SetBit(Start, false);
    }
    MbsByteArray[5] = 2;
    MessageLength = 8;
    client.write(MbsByteArray, MessageLength);
    MbsFC = MB_FC_NONE;
  }

  if (MbsFC == MB_FC_WRITE_REGISTER) {
    Start = word(MbsByteArray[8], MbsByteArray[9]);
    if (IsValidRegister(Start)) {
      MbData[Start] = word(MbsByteArray[10], MbsByteArray[11]);
    }
    MbsByteArray[5] = 6;
    MessageLength = 12;
    client.write(MbsByteArray, MessageLength);
    MbsFC = MB_FC_NONE;
  }

  if (MbsFC == MB_FC_WRITE_MULTIPLE_COILS) {
    Start = word(MbsByteArray[8], MbsByteArray[9]);
    CoilDataLength = word(MbsByteArray[10], MbsByteArray[11]);
    ByteDataLength = (CoilDataLength + 7) / 8;

    if ((13 + ByteDataLength) > i) {
      MbsFC = MB_FC_NONE;
      return;
    }

    MbsByteArray[5] = 6;

    for (int iBit = 0; iBit < CoilDataLength; iBit++) {
      SetBit(Start + iBit, bitRead(MbsByteArray[13 + (iBit / 8)], iBit % 8));
    }

    MessageLength = 12;
    client.write(MbsByteArray, MessageLength);
    MbsFC = MB_FC_NONE;
  }

  if (MbsFC == MB_FC_WRITE_MULTIPLE_REGISTERS) {
    Start = word(MbsByteArray[8], MbsByteArray[9]);
    WordDataLength = word(MbsByteArray[10], MbsByteArray[11]);
    ByteDataLength = WordDataLength * 2;

    if ((13 + ByteDataLength) > i) {
      MbsFC = MB_FC_NONE;
      return;
    }

    MbsByteArray[5] = 6;
    for (int iWord = 0; iWord < WordDataLength; iWord++) {
      if (IsValidRegister(Start + iWord)) {
        MbData[Start + iWord] = word(MbsByteArray[13 + iWord * 2], MbsByteArray[14 + iWord * 2]);
      }
    }

    MessageLength = 12;
    client.write(MbsByteArray, MessageLength);
    MbsFC = MB_FC_NONE;
  }
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
  return MB_DATA_LEN;
}

boolean ModbusTCP_RU::GetBit(word Number)
{
  if (!IsValidBit(Number)) {
    return false;
  }

  int ArrayPos = Number / 16;
  int BitPos = Number % 16;
  return bitRead(MbData[ArrayPos], BitPos);
}

boolean ModbusTCP_RU::SetBit(word Number, boolean Data)
{
  if (!IsValidBit(Number)) {
    return true;
  }

  int ArrayPos = Number / 16;
  int BitPos = Number % 16;
  bitWrite(MbData[ArrayPos], BitPos, Data);
  return false;
}

bool ModbusTCP_RU::IsValidRegister(word index)
{
  return index < MB_DATA_LEN;
}

bool ModbusTCP_RU::IsValidBit(word index)
{
  return index < (MB_DATA_LEN * 16);
}
