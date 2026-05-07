/*
 * Minimal SCADA Modbus TCP Slave for HTU21D.
 *
 * FC04: input register 0 = temperature * 100
 * FC04: input register 1 = humidity * 100
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

const word IREG_TEMP_X100 = 0;
const word IREG_HUM_X100 = 1;

void setup() {
  Ethernet.begin(mac, ip, gateway, subnet);

  Mb.Ireg(IREG_TEMP_X100, 0);
  Mb.Ireg(IREG_HUM_X100, 0);

  htu.begin();
}

void loop() {
  Mb.MbsRun();

  if (htu.readTick()) {
    Mb.Ireg(IREG_TEMP_X100, (int16_t)(htu.getTemperature() * 100.0f));
    Mb.Ireg(IREG_HUM_X100, (word)(htu.getHumidity() * 100.0f));
  }
}
