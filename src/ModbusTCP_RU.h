/*
  ModbusTCP_RU.h - Arduino Modbus TCP Master/Slave library.
  Version: V-0.2.0
*/

#ifndef ModbusTCP_RU_h
#define ModbusTCP_RU_h

#include "Arduino.h"
#include <SPI.h>
#include <Ethernet.h>
#include <string.h>

#define MB_PORT 502
#define MB_TIMEOUT 1000
#define MB_PACKET_TIMEOUT 50

// Закрывать "тихий" клиентский сокет после простоя. На медленной/многоустройственной
// линии Rapid SCADA опрос одного устройства может приходить реже — иначе библиотека
// сама рвёт живое соединение. Переопредели своим #define до include.
// 0 = таймаут простоя отключён.
#ifndef MB_IDLE_TIMEOUT
  #define MB_IDLE_TIMEOUT 60000UL
#endif

#if defined(ARDUINO_ARCH_AVR) && !defined(MB_SMALL_MEMORY)
  #define MB_SMALL_MEMORY
#endif

#if defined(ARDUINO_ARCH_AVR) && !defined(MB_ENABLE_MASTER) && !defined(MB_SLAVE_ONLY)
  #define MB_SLAVE_ONLY
#endif

#ifdef MB_SMALL_MEMORY
  #ifndef MB_MAX_COILS
    #define MB_MAX_COILS 8
  #endif
  #ifndef MB_MAX_DISCRETE
    #define MB_MAX_DISCRETE 8
  #endif
  #ifndef MB_MAX_HOLDING
    #define MB_MAX_HOLDING 16
  #endif
  #ifndef MB_MAX_INPUT
    #define MB_MAX_INPUT 16
  #endif
  #ifndef MB_MAX_CLIENTS
    #define MB_MAX_CLIENTS 2
  #endif
#else
  #ifndef MB_MAX_COILS
    #define MB_MAX_COILS 128
  #endif
  #ifndef MB_MAX_DISCRETE
    #define MB_MAX_DISCRETE 128
  #endif
  #ifndef MB_MAX_HOLDING
    #define MB_MAX_HOLDING 128
  #endif
  #ifndef MB_MAX_INPUT
    #define MB_MAX_INPUT 128
  #endif
  #ifndef MB_MAX_CLIENTS
    #define MB_MAX_CLIENTS 4
  #endif
#endif

#ifndef MB_BUFFER_SIZE
  #ifdef MB_SMALL_MEMORY
    #define MB_BUFFER_SIZE 128
  #else
    #define MB_BUFFER_SIZE 260
  #endif
#endif

#ifndef MB_DATA_LEN
  #define MB_DATA_LEN MB_MAX_HOLDING
#endif

#define MB_PROTOCOL_MAX_READ_BITS 2000
#define MB_PROTOCOL_MAX_READ_REGISTERS 125
#define MB_PROTOCOL_MAX_WRITE_COILS 1968
#define MB_PROTOCOL_MAX_WRITE_REGISTERS 123
#define MB_MAX_BYTES_PER_POLL 32

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

enum MB_EXCEPTION {
  MB_EX_ILLEGAL_FUNCTION = 1,
  MB_EX_ILLEGAL_DATA_ADDRESS = 2,
  MB_EX_ILLEGAL_DATA_VALUE = 3,
  MB_EX_SLAVE_DEVICE_FAILURE = 4
};

enum MB_WORD_ORDER {
  MB_WORD_ORDER_NORMAL = 0,
  MB_WORD_ORDER_SWAPPED = 1
};

typedef void (*ModbusCoilWriteCallback)(word address, bool value);
typedef void (*ModbusHoldingWriteCallback)(word address, word value);

struct ModbusStats {
  unsigned long rxPackets;
  unsigned long txPackets;
  unsigned long crcErrors;
  unsigned long exceptionCount;
  unsigned long socketErrors;
};

class ModbusTCP_RU
{
public:
  ModbusTCP_RU();

  bool MbCoils[MB_MAX_COILS];
  bool MbDiscreteInputs[MB_MAX_DISCRETE];
  uint16_t MbHoldingRegisters[MB_MAX_HOLDING];
  uint16_t MbInputRegisters[MB_MAX_INPUT];

  // Compatibility alias: old sketches using MbData[] now address holding registers.
  uint16_t (&MbData)[MB_MAX_HOLDING];

  bool Coil(word address) const;
  bool Coil(word address, bool value);
  bool Discrete(word address) const;
  bool Discrete(word address, bool value);
  word Hreg(word address) const;
  bool Hreg(word address, word value);
  word Ireg(word address) const;
  bool Ireg(word address, word value);

  bool CoilRead(word address) const;
  bool CoilWrite(word address, bool value);
  bool DiscreteRead(word address) const;
  bool DiscreteWrite(word address, bool value);

  boolean GetBit(word Number);
  boolean SetBit(word Number, boolean Data);

  void onCoilWrite(word address, ModbusCoilWriteCallback callback);
  void onHoldingWrite(word address, ModbusHoldingWriteCallback callback);
  void onCoilWrite(ModbusCoilWriteCallback callback);
  void onHoldingWrite(ModbusHoldingWriteCallback callback);

#ifndef MB_SLAVE_ONLY
  void Req(MB_FC FC, word Ref, word Count, word Pos);
  void MbmRun();
  void clientProcess();
  IPAddress remSlaveIP;
#endif

  void MbsRun();
  void serverProcess();
  word GetDataLen();

  // Управление сервером без обращения к глобальному EthernetServer из скетча.
  // begin()   — поднять слушающий сокет (идемпотентно).
  // restart() — закрыть все клиентские слоты и заново поднять сервер; вызывать
  //             после Ethernet.begin() в watchdog'е, чтобы не оставлять "мусорные" слоты.
  void begin();
  void restart();

  uint32_t ReadUInt32(word address, MB_WORD_ORDER order = MB_WORD_ORDER_NORMAL) const;
  int32_t ReadInt32(word address, MB_WORD_ORDER order = MB_WORD_ORDER_NORMAL) const;
  float ReadFloat(word address, MB_WORD_ORDER order = MB_WORD_ORDER_NORMAL) const;
  bool WriteUInt32(word address, uint32_t value, MB_WORD_ORDER order = MB_WORD_ORDER_NORMAL);
  bool WriteInt32(word address, int32_t value, MB_WORD_ORDER order = MB_WORD_ORDER_NORMAL);
  bool WriteFloat(word address, float value, MB_WORD_ORDER order = MB_WORD_ORDER_NORMAL);
  static void floatToRegs(float value, word *regs, MB_WORD_ORDER order = MB_WORD_ORDER_NORMAL);
  static float regsToFloat(const word *regs, MB_WORD_ORDER order = MB_WORD_ORDER_NORMAL);

  const ModbusStats& stats() const;
  void resetStats();

private:
  struct ServerClientState {
    EthernetClient client;
    uint8_t buffer[MB_BUFFER_SIZE];
    uint16_t length;
    int16_t expectedLength;
    unsigned long lastActivity;
  };

  MB_FC SetFC(int fc);
  bool IsValidRegister(word index);
  bool IsValidBit(word index);
  bool isRangeValid(word start, word count, word limit);
  bool isValidReadCount(MB_FC fc, word count);
  bool isValidWriteCount(MB_FC fc, word count);

  void clearServerClient(byte slot);
  void acceptServerClient();
  void processServerClient(byte slot);
  void processRequest(ServerClientState &state);
  void sendException(EthernetClient &client, uint8_t *request, byte exceptionCode);
  void sendResponse(EthernetClient &client, uint8_t *response, uint16_t length);
  void debugRequest(const char *prefix, byte fc, word address, word count);
  void debugException(byte fc, byte exceptionCode);

#ifndef MB_SLAVE_ONLY
  uint8_t MbmByteArray[MB_BUFFER_SIZE];
  MB_FC MbmFC;
  int MbmCounter;
  int MbmExpectedLength;
  unsigned long MbmLastActivity;
  uint16_t MbmTransactionId;
  uint16_t MbmPendingTransactionId;
  void MbmProcess();
  word MbmPos;
  word MbmBitCount;
#endif

  ServerClientState serverClients[MB_MAX_CLIENTS];
  MB_FC MbsFC;
  bool MbsServerStarted;

  ModbusCoilWriteCallback coilWriteCallback;
  ModbusHoldingWriteCallback holdingWriteCallback;
  ModbusStats modbusStats;
};

#endif
