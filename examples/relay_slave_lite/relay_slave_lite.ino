/*
 * Minimal SCADA Modbus TCP Slave for one relay.
 *
 * FC01: read coil 0
 * FC05/FC15: write coil 0
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

  Ethernet.begin(mac, ip, gateway, subnet);
}

void loop() {
  Mb.MbsRun();
}
