/*
 * Modbus TCP Slave for HTU21D with Serial diagnostics.
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

const long serialPrintInterval = 1000;
unsigned long lastSerialPrint = 0;

void setup() {
  Serial.begin(9600);
  Serial.println("HTU21D + Modbus TCP started");

  Ethernet.begin(mac, ip, gateway, subnet);
  delay(1000);

  if (Ethernet.linkStatus() == LinkOFF) {
    Serial.println("Error: Ethernet link is down");
    while (true) {
    }
  }

  if (!htu.begin()) {
    Serial.println("Error: HTU21D not found");
    while (true) {
    }
  }

  Mb.MbData[0] = 0;
  Mb.MbData[1] = 0;

  Serial.print("IP: ");
  Serial.println(Ethernet.localIP());
}

void loop() {
  Mb.MbsRun();

  if (htu.readTick()) {
    float temperature = htu.getTemperature();
    float humidity = htu.getHumidity();

    int tempFixed = static_cast<int>(temperature * 100);
    int humFixed = static_cast<int>(humidity * 100);

    Mb.MbData[0] = tempFixed;
    Mb.MbData[1] = humFixed;

    if (millis() - lastSerialPrint >= serialPrintInterval) {
      Serial.print("Temperature: ");
      Serial.print(tempFixed / 100.0, 2);
      Serial.print(" C | Humidity: ");
      Serial.print(humFixed / 100.0, 2);
      Serial.println(" %");
      lastSerialPrint = millis();
    }
  }
}
