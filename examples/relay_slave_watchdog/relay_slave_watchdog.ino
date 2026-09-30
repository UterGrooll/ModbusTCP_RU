/*
 * SCADA Modbus TCP Slave for one relay with an Ethernet link watchdog.
 *
 * Демонстрирует возможности v0.3.0:
 *   - Mb.begin()   — явный старт сервера;
 *   - Mb.restart(): закрывает соединения, сохраняя один слушающий сокет.
 *     Аппаратный reset Ethernet для обычного пропадания линка не нужен.
 *
 * Watchdog линка опирается на Ethernet.linkStatus() и работает на W5500 / W5200.
 * На W5100 linkStatus() возвращает Unknown — там не делайте реинициализацию по
 * таймеру, а полагайтесь на штатное восстановление сокетов библиотекой.
 *
 * FC01: read coil 0
 * FC05 / FC15: write coil 0
 */

#include <SPI.h>
#include <Ethernet.h>
#include "ModbusTCP_RU.h"

ModbusTCP_RU Mb;

byte mac[] = {0x90, 0xA2, 0xDA, 0x0D, 0x3F, 0xCD};
IPAddress ip(192, 168, 1, 100);
IPAddress gateway(192, 168, 1, 1);
IPAddress dnsServer(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);

const byte RELAY_PIN = 2;
const word COIL_RELAY = 0;

/* ---------- Ethernet link watchdog ---------- */
const unsigned long LINK_CHECK_PERIOD_MS = 1000UL;
const unsigned long LINK_RECOVER_DELAY_MS = 1500UL;

unsigned long linkCheckTimer = 0;
unsigned long linkUpTime = 0;
bool linkWasDown = false;
bool linkRecovering = false;

void startEthernet() {
  Ethernet.begin(mac, ip, dnsServer, gateway, subnet);
  Mb.begin();
}

void updateLinkWatchdog() {
  unsigned long now = millis();

  if (now - linkCheckTimer < LINK_CHECK_PERIOD_MS) {
    return;
  }
  linkCheckTimer = now;

  EthernetLinkStatus link = Ethernet.linkStatus();

  if (link == LinkOFF) {
    linkWasDown = true;
    linkRecovering = false;
    return;
  }

  if (link == Unknown) {
    linkRecovering = false;
    return;
  }

  if (link == LinkON && linkWasDown) {
    if (!linkRecovering) {
      linkRecovering = true;
      linkUpTime = now;
    }
    if (now - linkUpTime >= LINK_RECOVER_DELAY_MS) {
      linkWasDown = false;
      linkRecovering = false;
      Mb.restart();
    }
  }
}

void onCoilWrite(word address, bool value) {
  if (address == COIL_RELAY) {
    digitalWrite(RELAY_PIN, value ? HIGH : LOW);
  }
}

void setup() {
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);

  Mb.Coil(COIL_RELAY, false);
  Mb.onCoilWrite(onCoilWrite);

  Ethernet.init(10);   // CS-пин Ethernet-модуля (W5500/W5100)
  startEthernet();
}

void loop() {
  Mb.MbsRun();
  updateLinkWatchdog();
}
