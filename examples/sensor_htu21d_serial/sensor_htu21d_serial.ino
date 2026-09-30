/*
 * SCADA Modbus TCP Slave for HTU21D with Serial diagnostics.
 *
 * FC04: input register 0 = signed temperature * 100 (0x8000 unavailable)
 * FC04: input register 1 = humidity * 100 (0xFFFF unavailable)
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
IPAddress dnsServer(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);

const word IREG_TEMP_X100 = 0;
const word IREG_HUM_X100 = 1;
const word TEMP_UNAVAILABLE = 0x8000;
const word HUM_UNAVAILABLE = 0xFFFF;
const uint32_t SENSOR_RETRY_MS = 1000;
const uint32_t SENSOR_STEP_MS = 100;

enum SensorState : uint8_t {
  SENSOR_OFF,
  SENSOR_RESET,
  SENSOR_TEMPERATURE,
  SENSOR_HUMIDITY
};

SensorState sensorState = SENSOR_OFF;
uint32_t sensorTimer = 0;
int16_t temperatureX100 = 0;

void sensorFailed(uint32_t now) {
  sensorState = SENSOR_OFF;
  sensorTimer = now;
  Mb.Ireg(IREG_TEMP_X100, TEMP_UNAVAILABLE);
  Mb.Ireg(IREG_HUM_X100, HUM_UNAVAILABLE);
}

void updateSensor(uint32_t now) {
  const uint32_t period = sensorState == SENSOR_OFF
      ? SENSOR_RETRY_MS : SENSOR_STEP_MS;
  if (now - sensorTimer < period) {
    return;
  }
  sensorTimer = now;

  // Wait between reset, conversion request and reading; keep TCP polling.
  switch (sensorState) {
    case SENSOR_OFF:
      if (htu.begin()) {
        sensorState = SENSOR_RESET;
      }
      break;

    case SENSOR_RESET:
      if (htu.requestTemperature()) {
        sensorState = SENSOR_TEMPERATURE;
      } else {
        sensorFailed(now);
      }
      break;

    case SENSOR_TEMPERATURE:
      if (!htu.readTemperature() || !htu.requestHumidity()) {
        sensorFailed(now);
        break;
      }
      temperatureX100 = (int16_t)(htu.getTemperature() * 100.0f);
      sensorState = SENSOR_HUMIDITY;
      break;

    case SENSOR_HUMIDITY:
      if (!htu.readHumidity() || !htu.requestTemperature()) {
        sensorFailed(now);
        break;
      }
      Mb.Ireg(IREG_TEMP_X100, (word)temperatureX100);
      Mb.Ireg(IREG_HUM_X100, (word)(htu.getHumidity() * 100.0f));
      sensorState = SENSOR_TEMPERATURE;
      break;
  }
}

const uint32_t SERIAL_PERIOD = 1000;
uint32_t serialTimer = 0;

void setup() {
  Serial.begin(9600);
  Ethernet.begin(mac, ip, dnsServer, gateway, subnet);
  Mb.begin();

  sensorFailed(millis());
#if defined(WIRE_HAS_TIMEOUT)
  Wire.setWireTimeout(25000, true);
#endif

  Serial.println(F("ModbusTCP_RU SCADA Slave started"));
  Serial.print(F("IP: "));
  Serial.println(Ethernet.localIP());
}

void loop() {
  Mb.MbsRun();
  uint32_t now = millis();
  updateSensor(now);

  if (now - serialTimer >= SERIAL_PERIOD) {
    serialTimer = now;
    if (Mb.Ireg(IREG_TEMP_X100) == TEMP_UNAVAILABLE) {
      Serial.println(F("HTU21D unavailable; retrying"));
    } else {
      Serial.print(F("T="));
      Serial.print((int16_t)Mb.Ireg(IREG_TEMP_X100) / 100.0f, 2);
      Serial.print(F(" C, H="));
      Serial.print(Mb.Ireg(IREG_HUM_X100) / 100.0f, 2);
      Serial.println(F(" %"));
    }
  }
}
