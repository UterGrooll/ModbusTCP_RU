#pragma once
#include <cstdint>
#include <cstddef>
using byte = uint8_t;
using word = uint16_t;
using boolean = bool;
extern uint32_t fakeMillis;
inline uint32_t millis() { return fakeMillis; }
#define highByte(x) ((uint8_t)((x) >> 8))
#define lowByte(x) ((uint8_t)(x))
#define bitRead(x, n) (((x) >> (n)) & 1U)
#define bitWrite(x, n, v) ((v) ? ((x) |= (1UL << (n))) : ((x) &= ~(1UL << (n))))
