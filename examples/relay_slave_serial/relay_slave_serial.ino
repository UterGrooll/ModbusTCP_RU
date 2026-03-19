/*
 * Modbus TCP Slave for relay control with Serial diagnostics.
 * Register 0: relay command.
 * Register 1: actual relay state.
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

#define RELAY_REGISTER 0
#define STATUS_REGISTER 1

bool lastRelayState = false;
unsigned long lastStatusUpdate = 0;
const unsigned long STATUS_UPDATE_INTERVAL = 1000;

void setup() {
  Serial.begin(9600);

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);

  Ethernet.begin(mac, ip, gateway, subnet);
  delay(1000);

  if (Ethernet.linkStatus() == LinkOFF) {
    Serial.println("Error: Ethernet link is down");
    while (1) {
    }
  }

  Mb.MbData[RELAY_REGISTER] = 0;
  Mb.MbData[STATUS_REGISTER] = 0;

  Serial.println("Modbus TCP Slave started");
  Serial.print("IP: ");
  Serial.println(Ethernet.localIP());
  Serial.println("Register 0 = command, register 1 = status");
}

void loop() {
  Mb.MbsRun();

  bool currentRelayState = (Mb.MbData[RELAY_REGISTER] == 1);
  if (currentRelayState != lastRelayState) {
    digitalWrite(RELAY_PIN, currentRelayState ? HIGH : LOW);
    lastRelayState = currentRelayState;
    Serial.print("Relay state changed: ");
    Serial.println(currentRelayState ? "ON" : "OFF");
  }

  if (millis() - lastStatusUpdate >= STATUS_UPDATE_INTERVAL) {
    Mb.MbData[STATUS_REGISTER] = digitalRead(RELAY_PIN) ? 1 : 0;
    lastStatusUpdate = millis();
  }
}
