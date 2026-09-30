/*
  ModbusTCP_RU.h - Arduino Modbus TCP Slave library.
  Version: V-0.3.0
*/

#ifndef ModbusTCP_RU_h
#define ModbusTCP_RU_h

#include "Arduino.h"
#include <SPI.h>
#include <Ethernet.h>
#include <string.h>

#ifndef MAX_SOCK_NUM
#error "Use Arduino Ethernet >= 2.0.2 for W5100/W5500, not EthernetENC."
#endif

#include "ModbusTCP_RU_config.h"

#define MB_PROTOCOL_MAX_READ_BITS 2000
#define MB_PROTOCOL_MAX_READ_REGISTERS 125
#define MB_PROTOCOL_MAX_WRITE_COILS 1968
#define MB_PROTOCOL_MAX_WRITE_REGISTERS 123

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
  unsigned long crcErrors; // Legacy field: always zero for Modbus TCP.
  unsigned long exceptionCount;
  unsigned long socketErrors;
  unsigned long malformedFrames;
  unsigned long timeouts;
  unsigned long rejectedClients;
};

template<unsigned C, unsigned D, unsigned H, unsigned I, unsigned N, unsigned B>
struct ModbusConfigTag {};

typedef ModbusConfigTag<MB_MAX_COILS, MB_MAX_DISCRETE, MB_MAX_HOLDING,
                        MB_MAX_INPUT, MB_MAX_CLIENTS, MB_BUFFER_SIZE> ModbusBuildConfig;

static_assert(MB_MAX_COILS > 0 && MB_MAX_COILS <= 65535UL, "Invalid coil map");
static_assert(MB_MAX_DISCRETE > 0 && MB_MAX_DISCRETE <= 65535UL, "Invalid discrete map");
static_assert(MB_MAX_HOLDING > 0 && MB_MAX_HOLDING <= 65535UL, "Invalid holding map");
static_assert(MB_MAX_INPUT > 0 && MB_MAX_INPUT <= 65535UL, "Invalid input map");
static_assert(MB_MAX_CLIENTS > 0 && MB_MAX_CLIENTS < MAX_SOCK_NUM, "Reserve one Ethernet socket for listening");
static_assert(MB_BUFFER_SIZE >= 13 && MB_BUFFER_SIZE <= 260, "Invalid TCP ADU buffer size");
static_assert(MB_MAX_BYTES_PER_POLL > 0 && MB_MAX_BYTES_PER_POLL <= 260, "Invalid polling budget");
static_assert(MB_FRAME_TIMEOUT > 0, "A complete-frame deadline is required");
static_assert(9UL + 2UL * (MB_MAX_HOLDING < 125 ? MB_MAX_HOLDING : 125) <= MB_BUFFER_SIZE,
              "Holding read response does not fit MB_BUFFER_SIZE");
static_assert(13UL + 2UL * (MB_MAX_HOLDING < 123 ? MB_MAX_HOLDING : 123) <= MB_BUFFER_SIZE,
              "Holding write request does not fit MB_BUFFER_SIZE");
static_assert(9UL + 2UL * (MB_MAX_INPUT < 125 ? MB_MAX_INPUT : 125) <= MB_BUFFER_SIZE,
              "Input read response does not fit MB_BUFFER_SIZE");
static_assert(9UL + ((MB_MAX_COILS < 2000 ? MB_MAX_COILS : 2000) + 7UL) / 8 <= MB_BUFFER_SIZE,
              "Coil read response does not fit MB_BUFFER_SIZE");
static_assert(13UL + ((MB_MAX_COILS < 1968 ? MB_MAX_COILS : 1968) + 7UL) / 8 <= MB_BUFFER_SIZE,
              "Coil write request does not fit MB_BUFFER_SIZE");
static_assert(9UL + ((MB_MAX_DISCRETE < 2000 ? MB_MAX_DISCRETE : 2000) + 7UL) / 8 <= MB_BUFFER_SIZE,
              "Discrete read response does not fit MB_BUFFER_SIZE");
static_assert(sizeof(float) == 4, "Float helpers require 32-bit float");

class ModbusTCP_RU
{
public:
  // The tag makes inconsistent array sizes across compilation units a link error.
  explicit ModbusTCP_RU(ModbusBuildConfig *config = 0);
  ModbusTCP_RU(const ModbusTCP_RU&) = delete;
  ModbusTCP_RU& operator=(const ModbusTCP_RU&) = delete;

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

  // Use the common callback and dispatch on its address argument.
  void onCoilWrite(word address, ModbusCoilWriteCallback callback) = delete;
  void onHoldingWrite(word address, ModbusHoldingWriteCallback callback) = delete;
  void onCoilWrite(ModbusCoilWriteCallback callback);
  void onHoldingWrite(ModbusHoldingWriteCallback callback);

  void MbsRun();
  void serverProcess();
  word GetDataLen();

  // restart() clears connections but reuses an existing listener. Does not reset Ethernet.
  bool begin();
  bool restart();
  void setIdleTimeout(uint32_t milliseconds);
  void setPacketTimeout(uint32_t milliseconds);
  void setResponseTimeout(uint32_t milliseconds);
  bool setCoilLocal(word address, bool value);
  bool setHoldingLocal(word address, word value);

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
    uint16_t expectedLength;
    bool responsePending;
    uint32_t frameStarted;
    unsigned long lastActivity;
  };

  void notifyCoil(word address, bool value);
  void notifyHolding(word address, word value);
  static bool isRangeValid(word start, word count, word limit);
  bool isValidReadCount(MB_FC fc, word count);
  bool isValidWriteCount(MB_FC fc, word count);

  void clearServerClient(byte slot);
  void acceptServerClient();
  void processServerClient(byte slot);
  void processRequest(ServerClientState &state);
  void sendException(ServerClientState &state, byte exceptionCode);
  void sendResponse(ServerClientState &state, uint16_t length);
  void transmitResponse(byte slot);
  void debugRequest(const char *prefix, byte fc, word address, word count);
  void debugException(byte fc, byte exceptionCode);

  ServerClientState serverClients[MB_MAX_CLIENTS];
  uint32_t idleTimeout;
  uint32_t packetTimeout;
  uint32_t responseTimeout;
  bool notifyingCoil;
  bool notifyingHolding;

  ModbusCoilWriteCallback coilWriteCallback;
  ModbusHoldingWriteCallback holdingWriteCallback;
  ModbusStats modbusStats;
};

#endif
