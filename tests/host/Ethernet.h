#pragma once
#include "Arduino.h"
#include <deque>
#include <vector>
#define MAX_SOCK_NUM 8

enum SocketStatus { Closed, Listening, Established };
struct FakeSocket {
  SocketStatus status = Closed;
  uint16_t serverPort = 0;
  bool failWrite = false;
  bool readClosed = false;
  int txFree = 2048;
  std::deque<uint8_t> rx;
  std::vector<std::vector<uint8_t>> tx;
};
extern FakeSocket sockets[MAX_SOCK_NUM];
extern unsigned beginCalls;
enum EthernetHardwareStatus { EthernetW5100, EthernetW5500 };
extern bool fakeW5100;
struct FakeEthernet {
  EthernetHardwareStatus hardwareStatus() const { return fakeW5100 ? EthernetW5100 : EthernetW5500; }
};
extern FakeEthernet Ethernet;
class EthernetClient {
public:
  int id;
  explicit EthernetClient(int index = -1) : id(index) {}
  explicit operator bool() const { return id >= 0; }
  bool connected() const {
    return id >= 0 && sockets[id].status == Established &&
           !(sockets[id].readClosed && sockets[id].rx.empty());
  }
  int available() const { return id >= 0 ? int(sockets[id].rx.size()) : 0; }
  int availableForWrite() const { return id >= 0 && sockets[id].status == Established ? sockets[id].txFree : 0; }
  void setConnectionTimeout(uint16_t) {}
  int read() {
    if (!available()) return -1;
    int b = sockets[id].rx.front(); sockets[id].rx.pop_front(); return b;
  }
  size_t write(const uint8_t* data, size_t length) {
    if (id < 0 || sockets[id].status != Established || sockets[id].failWrite) return 0;
    sockets[id].tx.emplace_back(data, data + length);
    return length;
  }
  void stop() {
    if (id >= 0) { sockets[id].status = Closed; sockets[id].rx.clear(); }
    id = -1;
  }
};
// Models Ethernet 2.x listener allocation and accept() transferring ownership.
class EthernetServer {
  uint16_t port;
public:
  explicit EthernetServer(uint16_t p) : port(p) {}
  explicit operator bool() const {
    for (const auto &s : sockets)
      if (s.serverPort == port && s.status == Listening) return true;
    return false;
  }
  void begin() {
    ++beginCalls;
    for (auto &s : sockets) {
      if (s.status != Closed) continue;
      s.status = Listening; s.serverPort = port; return;
    }
  }
  EthernetClient accept() {
    int found = -1;
    for (int i = 0; i < MAX_SOCK_NUM; ++i) {
      auto &s = sockets[i];
      if (s.serverPort != port) continue;
      if (found < 0 && s.status == Established) {
        found = i; s.serverPort = 0;
      } else if (s.status == Closed) s.serverPort = 0;
    }
    if (!*this) begin();
    return EthernetClient(found);
  }
};
