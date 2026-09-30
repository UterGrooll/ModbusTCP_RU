#include <cstdio>
#include <cstring>
#include <limits>
#include "ModbusTCP_RU.h"

FakeSocket sockets[MAX_SOCK_NUM];
uint32_t fakeMillis = 0;
unsigned beginCalls = 0;
bool fakeW5100 = false;
FakeEthernet Ethernet;
unsigned failures = 0, tests = 0;
ModbusTCP_RU* callbackObject = nullptr;
unsigned callbackCount = 0;
bool sawCommittedRange = false;

void check(bool condition, const char* label) {
  ++tests;
  if (!condition) ++failures;
  std::printf("%s %s\n", condition ? "PASS" : "FAIL", label);
}
void resetTransport(bool connect = true) {
  for (auto &s : sockets) s = FakeSocket();
  if (connect) { sockets[0].status = Established; sockets[0].serverPort = MB_PORT; }
  fakeMillis = 0; beginCalls = 0; callbackCount = 0;
  fakeW5100 = false;
}
std::vector<uint8_t> request(byte fc, word address, word value, word tid = 1) {
  return {highByte(tid), lowByte(tid), 0, 0, 0, 6, 1, fc,
          highByte(address), lowByte(address), highByte(value), lowByte(value)};
}
void enqueue(const std::vector<uint8_t>& frame, int socket = 0) {
  sockets[socket].rx.insert(sockets[socket].rx.end(), frame.begin(), frame.end());
}
void poll(ModbusTCP_RU &mb, unsigned n = 20) { while (n--) mb.MbsRun(); }
std::vector<uint8_t> write16(word count, word value, word start = 0) {
  auto frame = request(16, start, count);
  frame[5] = 7 + count * 2; frame.push_back(count * 2);
  for (unsigned i = 0; i < count; ++i) {
    frame.push_back(highByte(value)); frame.push_back(lowByte(value));
  }
  return frame;
}
bool exception(byte fc, byte code) {
  if (sockets[0].tx.empty()) return false;
  const auto &r = sockets[0].tx.back();
  return r.size() == 9 && r[7] == (fc | 0x80) && r[8] == code;
}
void recursiveHandler(word address, word value) {
  ++callbackCount;
  callbackObject->Hreg(address, value);
}
void batchHandler(word, word) {
  sawCommittedRange = callbackObject->Hreg(0) == 17 && callbackObject->Hreg(1) == 17;
  ++callbackCount;
}
void coilHandler(word, bool) { ++callbackCount; }
unsigned listeners() {
  unsigned n = 0;
  for (auto &s : sockets) n += s.status == Listening;
  return n;
}
int main() {
  {
    resetTransport(); ModbusTCP_RU mb;
    mb.Coil(0, true); mb.Discrete(0, false); mb.Hreg(0, 123); mb.Ireg(0, 456);
    for (byte fc = 1; fc <= 4; ++fc) {
      enqueue(request(fc, 0, 1, 0xFF00 + fc)); poll(mb);
      const auto &r = sockets[0].tx.back();
      const unsigned value = fc <= 2 ? r[9] : r[9] * 256U + r[10];
      check(r[0] == 255 && r[1] == fc && r[7] == fc &&
            value == (fc == 1 ? 1 : fc == 2 ? 0 : fc == 3 ? 123 : 456),
            "read memory isolation and transaction ID echo");
    }
    enqueue(request(3, MB_MAX_HOLDING - 1, 2)); poll(mb);
    check(exception(3, 2), "read beyond map returns 02");
    enqueue(request(3, 0, 0)); poll(mb);
    check(exception(3, 3), "zero count returns 03");
    enqueue(request(3, 65535, 2)); poll(mb);
    check(exception(3, 2), "address wrap rejected");
    enqueue(request(5, 0, 0xFF00)); poll(mb);
    check(mb.Coil(0) && sockets[0].tx.back() == request(5, 0, 0xFF00), "FC05 exact echo");
    enqueue(request(5, 0, 1)); poll(mb);
    check(exception(5, 3) && mb.Coil(0), "invalid FC05 does not mutate");
    auto multiple = request(15, 0, 8); multiple[5] = 8;
    multiple.push_back(1); multiple.push_back(0xA5);
    enqueue(multiple); poll(mb);
    check(mb.Coil(0) && !mb.Coil(1) && mb.Coil(7) && sockets[0].tx.back().size() == 12,
          "FC15 packing and response length");
    enqueue(write16(16, 19)); poll(mb);
    check(mb.Hreg(0) == 19 && mb.Hreg(15) == 19, "FC16 writes 16 registers");
    enqueue(request(99, 0, 1)); poll(mb);
    check(exception(99, 1), "unsupported function returns 01");
    const word maxRead = MB_MAX_HOLDING < 125 ? MB_MAX_HOLDING : 125;
    enqueue(request(3, 0, maxRead)); poll(mb);
    check(sockets[0].tx.back().size() == 9U + 2U * maxRead &&
          sockets[0].tx.back()[5] == 3U + 2U * maxRead, "largest holding response fits");
    const word maxWrite = MB_MAX_HOLDING < 123 ? MB_MAX_HOLDING : 123;
    enqueue(write16(maxWrite, 1234)); poll(mb);
    check(mb.Hreg(maxWrite - 1) == 1234 && sockets[0].tx.back().size() == 12,
          "largest FC16 request and fixed-length reply");
  }
  {
    resetTransport(); ModbusTCP_RU mb;
    enqueue(request(6, 0, 7)); poll(mb); mb.Hreg(0, 9);
    auto frame = request(6, 0, 0); frame.resize(8); frame[5] = 2;
    enqueue(frame); poll(mb);
    check(mb.Hreg(0) == 9 && exception(6, 3), "short FC06 cannot reuse stale data");
    enqueue(request(5, 0, 0xFF00)); poll(mb); mb.Coil(0, false);
    frame[7] = 5; enqueue(frame); poll(mb);
    check(!mb.Coil(0) && exception(5, 3), "short FC05 cannot reactivate relay");
    frame = request(6, 0, 10); frame.push_back(0); frame[5] = 7;
    enqueue(frame); poll(mb);
    check(mb.Hreg(0) == 9 && exception(6, 3), "extra FC06 payload rejected");
    frame = write16(2, 88); frame.pop_back(); frame[5]--;
    enqueue(frame); poll(mb);
    check(mb.Hreg(0) == 9 && exception(16, 3), "truncated FC16 has no partial mutation");
    frame = write16(2, 88); frame.push_back(0); frame[5]++;
    enqueue(frame); poll(mb);
    check(mb.Hreg(0) == 9 && exception(16, 3), "extra FC16 payload rejected");
  }
  {
    resetTransport(); ModbusTCP_RU mb;
    auto f = request(6, 0, 777); f[3] = 1; enqueue(f); poll(mb);
    check(mb.Hreg(0) == 0 && sockets[0].status == Closed &&
          mb.stats().malformedFrames == 1, "bad protocol ID closes without writing");
  }
  for (word length : {word(0), word(1), word(255), word(65535)}) {
    resetTransport(); ModbusTCP_RU mb;
    auto f = request(6, 0, 777); f[4] = highByte(length); f[5] = lowByte(length);
    enqueue(f); poll(mb);
    check(mb.Hreg(0) == 0 && sockets[0].status == Closed, "bad MBAP length rejected");
  }
  {
    resetTransport(); ModbusTCP_RU mb;
    enqueue(write16(16, 11)); enqueue(write16(16, 22)); poll(mb);
    check(sockets[0].tx.size() == 2 && mb.Hreg(0) == 22 && mb.stats().socketErrors == 0,
          "coalesced FC16 frames use one parser");
  }
  {
    resetTransport(); ModbusTCP_RU mb;
    auto f = request(4, 0, 1); f[6] = 247;
    for (byte b : f) { enqueue({b}); poll(mb); fakeMillis += 60; }
    check(sockets[0].tx.size() == 1 && sockets[0].tx[0][6] == 247,
          "one-byte fragments with 60ms gaps and unit ID echo");
  }
  {
    resetTransport(); ModbusTCP_RU mb;
    mb.setPacketTimeout(100); enqueue({0, 1, 0}); poll(mb);
    fakeMillis = 101; poll(mb);
    check(sockets[0].status == Closed && mb.stats().timeouts == 1, "partial frame timeout");
  }
  {
    resetTransport(); ModbusTCP_RU mb;
    mb.setPacketTimeout(0); enqueue({0}); poll(mb);
    fakeMillis = MB_FRAME_TIMEOUT; enqueue({1}); poll(mb);
    check(sockets[0].status == Closed, "total frame deadline prevents slow drip");
  }
  {
    resetTransport(); ModbusTCP_RU mb;
    mb.setIdleTimeout(10); poll(mb); fakeMillis = 11; poll(mb);
    check(sockets[0].status == Closed, "idle connection without first byte reclaimed");
  }
  {
    resetTransport(); ModbusTCP_RU mb;
    mb.setIdleTimeout(0); poll(mb); fakeMillis = MB_IDLE_TIMEOUT + 1; poll(mb);
    check(sockets[0].status == Established, "runtime disable idle timeout");
    mb.setIdleTimeout(10); enqueue(request(4, 0, 1)); poll(mb);
    check(sockets[0].tx.size() == 1, "queued request survives busy loop idle interval");
  }
  {
    resetTransport(); ModbusTCP_RU mb;
    fakeMillis = UINT32_MAX - 5; mb.setIdleTimeout(10); poll(mb);
    fakeMillis = 6; poll(mb);
    check(sockets[0].status == Closed, "millis rollover handled");
  }
  {
    resetTransport(false); ModbusTCP_RU mb;
    mb.begin(); mb.begin();
    check(beginCalls == 1 && listeners() == 1, "begin is idempotent");
    for (unsigned i = 0; i < 20; ++i) mb.restart();
    check(beginCalls == 1 && listeners() == 1, "restart reuses listener");
  }
  {
    resetTransport(); ModbusTCP_RU mb;
    poll(mb); mb.restart();
    check(sockets[0].status == Closed && listeners() == 1, "restart closes owned connections");
  }
  {
    resetTransport(); ModbusTCP_RU mb;
    mb.begin(); mb.restart();
    check(sockets[0].status == Closed && listeners() == 1, "restart drains unclaimed connections");
  }
  {
    resetTransport(); ModbusTCP_RU mb;
    enqueue({0, 1, 0}); poll(mb);
    sockets[0].status = Closed; poll(mb);
    sockets[0] = FakeSocket(); sockets[0].status = Established; sockets[0].serverPort = MB_PORT;
    enqueue(request(6, 0, 44)); poll(mb);
    check(mb.Hreg(0) == 44 && sockets[0].tx.size() == 1, "reused socket starts with clean parser");
  }
  {
    resetTransport(); ModbusTCP_RU mb;
    for (unsigned i = 1; i <= MB_MAX_CLIENTS; ++i) {
      sockets[i].status = Established; sockets[i].serverPort = MB_PORT;
    }
    poll(mb);
    check(sockets[MB_MAX_CLIENTS].status == Closed &&
          sockets[0].status == Established && mb.stats().rejectedClients == 1,
          "excess client rejected without closing existing clients");
    enqueue(request(6, 0, 91, 101), 0);
    enqueue(request(6, 1, 92, 102), 1); poll(mb);
    check(sockets[0].tx.back()[1] == 101 && sockets[1].tx.back()[1] == 102 &&
          mb.Hreg(0) == 91 && mb.Hreg(1) == 92, "independent client transactions");
  }
  {
    resetTransport(); ModbusTCP_RU mb;
    callbackObject = &mb; mb.onHoldingWrite(recursiveHandler); mb.Hreg(0, 10);
    check(callbackCount == 1 && mb.Hreg(0) == 10, "callback write-back is not recursive");
    callbackCount = 0; mb.setHoldingLocal(0, 12);
    check(callbackCount == 0, "explicit local write is silent");
    mb.onHoldingWrite(batchHandler); enqueue(write16(2, 17)); poll(mb);
    check(callbackCount == 2 && sawCommittedRange, "FC16 commits range before callbacks");
    callbackCount = 0; mb.onCoilWrite(coilHandler); mb.Coil(0, true); mb.Coil(0, true);
    check(callbackCount == 2, "legacy callback still fires on every external setter");
    mb.setCoilLocal(0, false);
    check(callbackCount == 2 && !mb.Coil(0), "local coil setter is silent");
  }
#if MB_MAX_CLIENTS > 3
  {
    resetTransport(); ModbusTCP_RU mb; fakeW5100 = true;
    for (unsigned i = 1; i < 4; ++i) {
      sockets[i].status = Established; sockets[i].serverPort = MB_PORT;
    }
    poll(mb);
    check(sockets[2].status == Established && sockets[3].status == Closed &&
          mb.stats().rejectedClients == 1, "W5100 reserves listener with full profile");
  }
#endif
  {
    resetTransport(); ModbusTCP_RU mb; sockets[0].failWrite = true;
    enqueue(request(4, 0, 1)); poll(mb);
    check(mb.stats().txPackets == 0 && mb.stats().socketErrors == 1 &&
          sockets[0].status == Closed, "failed send does not count as successful tx");
  }
  {
    resetTransport(); ModbusTCP_RU mb; sockets[0].txFree = 0;
    enqueue(request(6, 0, 11)); enqueue(request(6, 0, 22)); poll(mb);
    check(mb.Hreg(0) == 11 && sockets[0].tx.empty(), "backpressure preserves first reply");
    sockets[0].txFree = 2048; poll(mb);
    check(mb.Hreg(0) == 22 && sockets[0].tx.size() == 2, "queued replies retain order");
  }
  {
    resetTransport(); ModbusTCP_RU mb; sockets[0].txFree = 0; mb.setResponseTimeout(10);
    enqueue(request(4, 0, 1)); poll(mb); fakeMillis = 11; poll(mb);
    check(sockets[0].status == Closed && mb.stats().timeouts == 1, "response deadline closes stalled peer");
  }
  {
    resetTransport(); ModbusTCP_RU mb; sockets[0].txFree = 0;
    enqueue(request(4, 0, 1)); poll(mb);
    sockets[0].readClosed = true; poll(mb);
    sockets[0].txFree = 2048; poll(mb);
    check(sockets[0].tx.size() == 1, "peer half-close still receives pending reply");
  }
  {
    resetTransport(); ModbusTCP_RU mb;
    mb.WriteUInt32(0, 0x11223344, MB_WORD_ORDER_SWAPPED);
    check(mb.Hreg(0) == 0x3344 && mb.ReadUInt32(0, MB_WORD_ORDER_SWAPPED) == 0x11223344,
          "32-bit swapped words");
    mb.WriteFloat(0, -12.5f);
    check(mb.ReadFloat(0) == -12.5f, "float round trip");
    const word before = mb.Hreg(0);
    check(!mb.WriteUInt32(65535, 5) && !mb.WriteFloat(MB_MAX_HOLDING - 1, 1) &&
          mb.ReadUInt32(65535) == 0 && mb.Hreg(0) == before, "32-bit boundary writes rejected");
  }
  std::printf("RESULT %u/%u passed\n", tests - failures, tests);
  return failures ? 1 : 0;
}
