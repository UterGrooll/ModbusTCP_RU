/*
 * SCADA Modbus TCP Slave for one relay with Serial diagnostics.
 *
 * FC01: read coil 0
 * FC05/FC15: write coil 0
 * FC04: read input register 0, actual relay state
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
const word IREG_RELAY_STATE = 0;

void onCoilWrite(word address, bool value) {
  if (address != COIL_RELAY) {
    return;
  }

  digitalWrite(RELAY_PIN, value ? HIGH : LOW);
  Mb.Ireg(IREG_RELAY_STATE, value ? 1 : 0);

  Serial.print(F("Relay: "));
  Serial.println(value ? F("ON") : F("OFF"));
}

void setup() {
  Serial.begin(9600);

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);

  Mb.Coil(COIL_RELAY, false);
  Mb.Ireg(IREG_RELAY_STATE, 0);
  Mb.onCoilWrite(onCoilWrite);

  Ethernet.begin(mac, ip, gateway, subnet);
  Mb.begin();
  delay(1000);

  Serial.println(F("ModbusTCP_RU SCADA Slave started"));
  Serial.print(F("IP: "));
  Serial.println(Ethernet.localIP());
}

void loop() {
  Mb.MbsRun();
}
