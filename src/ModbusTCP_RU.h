/*
  ModbusTCP_RU.h - библиотека Arduino для реализации Modbus TCP Master и Slave.
  Версия: V-0.1.3
  Оригинальный автор: Marco Gerritse (2013)
  Форк и доработка: UterGrooll (2025)
  Протестировано на сетевом модуле W5500 TCP/IP (Ethernet)
*/

#ifndef ModbusTCP_RU_h
#define ModbusTCP_RU_h

#include "Arduino.h"
#include <SPI.h>
#include <Ethernet.h>

#define MB_PORT 502
#define MB_BUFFER_SIZE 260
#define MB_TIMEOUT 1000
#define MB_MAX_COILS 2000
#define MB_MAX_REGISTERS 125
#define MB_MAX_MULTIPLE_COILS 800
#define MB_MAX_MULTIPLE_REGISTERS 100
#define MB_DATA_LEN 30

enum MB_FC {
  MB_FC_NONE                     = 0,
  MB_FC_READ_COILS               = 1,
  MB_FC_READ_DISCRETE_INPUT      = 2,
  MB_FC_READ_REGISTERS           = 3,
  MB_FC_READ_INPUT_REGISTER      = 4,
  MB_FC_WRITE_COIL               = 5,
  MB_FC_WRITE_REGISTER           = 6,
  MB_FC_WRITE_MULTIPLE_COILS     = 15,
  MB_FC_WRITE_MULTIPLE_REGISTERS = 16
};

class ModbusTCP_RU
{
public:
  ModbusTCP_RU();

  word MbData[MB_DATA_LEN];
  boolean GetBit(word Number);
  boolean SetBit(word Number, boolean Data);

  void Req(MB_FC FC, word Ref, word Count, word Pos);
  void MbmRun();
  IPAddress remSlaveIP;

  void MbsRun();
  word GetDataLen();

private:
  MB_FC SetFC(int fc);
  bool IsValidRegister(word index);
  bool IsValidBit(word index);

  uint8_t MbmByteArray[MB_BUFFER_SIZE];
  MB_FC MbmFC;
  int MbmCounter;
  void MbmProcess();
  word MbmPos;
  word MbmBitCount;

  uint8_t MbsByteArray[MB_BUFFER_SIZE];
  MB_FC MbsFC;
  bool MbsServerStarted;
};

#endif
