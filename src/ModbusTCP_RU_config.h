#ifndef MODBUSTCP_RU_CONFIG_H
#define MODBUSTCP_RU_CONFIG_H

// Edit this shared file, or supply flags to the ENTIRE build, never just an .ino.
#if defined(MB_ENABLE_MASTER)
#error "ModbusTCP_RU 0.3.0 is Slave-only; use 0.2.x for the legacy Master API."
#endif
#ifndef MB_SLAVE_ONLY
#define MB_SLAVE_ONLY
#endif
#if defined(ARDUINO_ARCH_AVR) && !defined(MB_SMALL_MEMORY)
#define MB_SMALL_MEMORY
#endif

#ifndef MB_PORT
#define MB_PORT 502
#endif
#ifndef MB_IDLE_TIMEOUT
#define MB_IDLE_TIMEOUT 60000UL
#endif
#ifndef MB_PACKET_TIMEOUT
#define MB_PACKET_TIMEOUT 1000UL
#endif
#ifndef MB_FRAME_TIMEOUT
#define MB_FRAME_TIMEOUT 5000UL
#endif
#ifndef MB_TX_TIMEOUT
#define MB_TX_TIMEOUT 1000UL
#endif
#ifndef MB_MAX_BYTES_PER_POLL
#define MB_MAX_BYTES_PER_POLL 32
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
#ifndef MB_BUFFER_SIZE
#define MB_BUFFER_SIZE 128
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
#ifndef MB_BUFFER_SIZE
#define MB_BUFFER_SIZE 260
#endif
#endif
#ifndef MB_DATA_LEN
#define MB_DATA_LEN MB_MAX_HOLDING
#endif
// Define MB_DEBUG here (or globally) to enable Serial diagnostics.
#endif
