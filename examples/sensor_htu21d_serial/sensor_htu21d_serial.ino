/*
 * SCADA Modbus TCP Slave for HTU21D with Serial diagnostics.
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
const unsigned long SERIAL_PERIOD = 1000;
unsigned long serialTimer = 0;

void setup() {
  Serial.begin(9600);

  Ethernet.begin(mac, ip, gateway, subnet);
  delay(1000);

  Mb.Ireg(IREG_TEMP_X100, 0);
  Mb.Ireg(IREG_HUM_X100, 0);

  if (!htu.begin()) {
    Serial.println(F("HTU21D not found"));
    while (true) {
    }
  }

  Serial.println(F("ModbusTCP_RU SCADA Slave started"));
  Serial.print(F("IP: "));
  Serial.println(Ethernet.localIP());
}

void loop() {
  Mb.MbsRun();

  if (htu.readTick()) {
    int16_t tempX100 = (int16_t)(htu.getTemperature() * 100.0f);
    word humX100 = (word)(htu.getHumidity() * 100.0f);

    Mb.Ireg(IREG_TEMP_X100, tempX100);
    Mb.Ireg(IREG_HUM_X100, humX100);

    if (millis() - serialTimer >= SERIAL_PERIOD) {
      serialTimer = millis();

      Serial.print(F("T="));
      Serial.print(tempX100 / 100.0f, 2);
      Serial.print(F(" C, H="));
      Serial.print(humX100 / 100.0f, 2);
      Serial.println(F(" %"));
    }
  }
}
