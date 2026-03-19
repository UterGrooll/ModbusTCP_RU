/*
 * Minimal Modbus TCP Slave for HTU21D.
 * Register 0: temperature * 100.
 * Register 1: humidity * 100.
 */

#include <SPI.h>
#include <Ethernet.h>
#include "ModbusTCP_RU.h"
#include <GyverHTU21D.h>

ModbusTCP_RU Mb;
GyverHTU21D htu;

byte mac[] = {0x90, 0xA5, 0xDA, 0x0E, 0x94, 0xB5};
IPAddress ip(192, 168, 1, 100);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);

void setup() {
  Ethernet.begin(mac, ip, gateway, subnet);

  Mb.MbData[0] = 0;
  Mb.MbData[1] = 0;

  htu.begin();
}

void loop() {
  Mb.MbsRun();

  if (htu.readTick()) {
    float temperature = htu.getTemperature();
    float humidity = htu.getHumidity();

    Mb.MbData[0] = static_cast<int>(temperature * 100);
    Mb.MbData[1] = static_cast<int>(humidity * 100);
  }
}
