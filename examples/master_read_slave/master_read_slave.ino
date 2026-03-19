/*
 * Direct Modbus TCP exchange without SCADA.
 * This device acts as Modbus TCP Master and reads 2 holding registers
 * from another Arduino/controller acting as Slave.
 *
 * Slave IP: 192.168.1.50
 * Register 0: temperature * 100
 * Register 1: humidity * 100
 */

#include <SPI.h>
#include <Ethernet.h>
#include "ModbusTCP_RU.h"

ModbusTCP_RU Mb;

byte mac[] = {0x90, 0xA5, 0xDA, 0x0E, 0x94, 0xB6};
IPAddress ip(192, 168, 1, 110);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);

const unsigned long POLL_INTERVAL = 1000;
unsigned long lastPollTime = 0;
bool requestInFlight = false;

void setup() {
  Serial.begin(9600);

  Ethernet.begin(mac, ip, gateway, subnet);
  delay(1000);

  Mb.remSlaveIP = IPAddress(192, 168, 1, 50);

  Serial.print("Master IP: ");
  Serial.println(Ethernet.localIP());
  Serial.print("Slave IP: ");
  Serial.println(Mb.remSlaveIP);
}

void loop() {
  if (!requestInFlight && millis() - lastPollTime >= POLL_INTERVAL) {
    Mb.Req(MB_FC_READ_REGISTERS, 0, 2, 0);
    lastPollTime = millis();
    requestInFlight = true;
  }

  Mb.MbmRun();

  if (requestInFlight) {
    float temperature = Mb.MbData[0] / 100.0;
    float humidity = Mb.MbData[1] / 100.0;

    Serial.print("Temperature: ");
    Serial.print(temperature, 2);
    Serial.print(" C, Humidity: ");
    Serial.print(humidity, 2);
    Serial.println(" %");

    requestInFlight = false;
  }
}
