/*
 * Название: sensor_htu21d_lite.ino
 *
 * Описание:
 * Пример реализации Modbus TCP Slave на Arduino для передачи данных
 * с датчика HTU21D (температура и влажность) через Ethernet.
 *
 * Формат данных:
 * - Значения передаются как целые числа, умноженные на 100
 * - Например: температура 23.54 → передаётся как 2354
 *             влажность 43.33 → передаётся как 4333
 *
 * Требования:
 * - Arduino + Ethernet Shield (W5100/W5500)
 * - Библиотеки: SPI, Ethernet, ModbusTCP_RU, GyverHTU21D
 */

#include <SPI.h>
#include <Ethernet.h>
#include "ModbusTCP_RU.h"
#include <GyverHTU21D.h>

// Экземпляры
ModbusTCP_RU Mb;
GyverHTU21D htu;

// Настройки сети
byte mac[] = {0x90, 0xA5, 0xDA, 0x0E, 0x94, 0xB5};
IPAddress ip(192, 168, 1, 100);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);

void setup() {
  // Инициализация Ethernet
  Ethernet.begin(mac, ip, gateway, subnet);

  // Инициализация регистров
  Mb.MbData[0] = 0;  
  Mb.MbData[1] = 0; 

  // Инициализация датчика
  htu.begin();
}

void loop() {
  Mb.MbsRun(); // Обработка Modbus-запросов

  // Чтение с датчика раз в секунду
  if (htu.readTick()) {
    float temperature = htu.getTemperature();  // °C
    float humidity = htu.getHumidity();        // %

    // Если нужны точные показания 23,54 данные придут в формате 2354
    Mb.MbData[0] = static_cast<int>(temperature);  // получим 23
    Mb.MbData[1] = static_cast<int>(humidity * 100);     // либо так 43.33 → 4333
  }
}