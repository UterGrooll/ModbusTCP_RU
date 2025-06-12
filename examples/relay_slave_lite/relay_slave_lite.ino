/*
 * Описание:
 * Минимальный рабочий пример реализации Modbus TCP Slave на Arduino.
 * Управляет состоянием реле через Ethernet по протоколу Modbus TCP.
 * Не содержит дополнительных проверок и диагностики — только базовая функциональность.
 *
 * Функционал:
 * - Принимает команды Modbus TCP (функция 0x06 — запись регистра)
 * - Управляет реле через цифровой выход
 * - Регистр управления реле: 0 (значение 0 = выкл, 1 = вкл)
 *
 * Требования:
 * - Arduino + Ethernet Shield (W5100/W5500)
 * - Библиотеки: SPI, Ethernet, ModbusTCP_RU
 * - Поддерживает подключение одного внешнего реле
 */

#include <SPI.h>
#include <Ethernet.h>
#include "ModbusTCP_RU.h"

ModbusTCP_RU Mb;

// Настройки Ethernet
byte mac[] = {0x90, 0xA2, 0xDA, 0x0D, 0x3F, 0xCD};
IPAddress ip(192, 168, 1, 100);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);

const int RELAY_PIN = 2;  // Пин, к которому подключено реле

void setup() {
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW); // Гарантированное выключение реле при старте
  
  Ethernet.begin(mac, ip, gateway, subnet);
  Mb.MbData[0] = 0;  // Инициализация регистра реле (0 = выключено)
}

void loop() {
  Mb.MbsRun();  // Обработка входящих Modbus-запросов
  // Обновление состояния реле в соответствии с регистром Modbus
  digitalWrite(RELAY_PIN, Mb.MbData[0] == 1 ? HIGH : LOW);
}