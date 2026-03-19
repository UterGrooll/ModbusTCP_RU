/*
 * Minimal Modbus TCP Slave for relay control.
 * Register 0: relay command (0 = OFF, 1 = ON).
 */

#include <SPI.h>
#include <Ethernet.h>
#include "ModbusTCP_RU.h"

ModbusTCP_RU Mb;

byte mac[] = {0x90, 0xA2, 0xDA, 0x0D, 0x3F, 0xCD};
IPAddress ip(192, 168, 1, 100);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);

const int RELAY_PIN = 2;
const word RELAY_REGISTER = 0;

void setup() {
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);

  Ethernet.begin(mac, ip, gateway, subnet);
  Mb.MbData[RELAY_REGISTER] = 0;
}

void loop() {
  Mb.MbsRun();
  digitalWrite(RELAY_PIN, Mb.MbData[RELAY_REGISTER] == 1 ? HIGH : LOW);
}
