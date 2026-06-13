/*
 * SCADA Modbus TCP Slave for one relay with an Ethernet link watchdog.
 *
 * Демонстрирует возможности v0.2.0:
 *   - Mb.begin()   — явный старт сервера;
 *   - Mb.restart() — корректное восстановление связи после пропадания линка:
 *                    закрывает клиентские слоты и заново поднимает сервер
 *                    (иначе после Ethernet.begin() остаются "мусорные" слоты,
 *                     дающие "приём по 0 байт" со стороны SCADA).
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
IPAddress subnet(255, 255, 255, 0);

const byte RELAY_PIN = 2;
const word COIL_RELAY = 0;

/* ---------- Ethernet link watchdog ---------- */
const unsigned long LINK_CHECK_PERIOD_MS = 1000UL;
const unsigned long LINK_RECOVER_DELAY_MS = 1500UL;

unsigned long linkCheckTimer = 0;
unsigned long linkDownTime = 0;
bool linkWasDown = false;

void startEthernet() {
  Ethernet.begin(mac, ip, gateway, subnet);
  delay(500);
  Mb.restart();   // re-begin сервера + очистка клиентских слотов (v0.2.0)
}

void updateLinkWatchdog() {
  unsigned long now = millis();

  if (now - linkCheckTimer < LINK_CHECK_PERIOD_MS) {
    return;
  }
  linkCheckTimer = now;

  EthernetLinkStatus link = Ethernet.linkStatus();

  if (link == LinkOFF) {
    if (!linkWasDown) {
      linkWasDown = true;
      linkDownTime = now;
    }
    return;
  }

  // Линк вернулся и держится дольше LINK_RECOVER_DELAY_MS — переподнимаем Ethernet.
  if (link == LinkON && linkWasDown && (now - linkDownTime >= LINK_RECOVER_DELAY_MS)) {
    linkWasDown = false;
    startEthernet();
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
