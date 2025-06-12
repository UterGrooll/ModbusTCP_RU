/*
 * Название: sensor_htu21d_fixedpoint_serial_millis.ino
 *
 * Описание:
 * Пример реализации Modbus TCP Slave на Arduino для передачи данных
 * с датчика HTU21D (температура и влажность) через Ethernet.
 *
 * Формат данных:
 * - Значения температуры и влажности умножаются на 100 и передаются как целые числа
 *   Например: температура 23.33 → передаётся как 2333
 *             влажность 43.33 → передаётся как 4333
 *
 * Адреса регистров:
 * - Регистр 0: Температура × 100
 * - Регистр 1: Влажность × 100
 *
 * Также выводит данные через Serial для диагностики, но не чаще одного раза в заданный интервал.
 *
 * Требования:
 * - Arduino Uno/Nano/Mega
 * - Ethernet Shield (W5100/W5500)
 * - Датчик HTU21D (подключается по шине I2C)
 * - Библиотеки: SPI, Ethernet, ModbusTCP_RU, GyverHTU21D
 */

#include <SPI.h>
#include <Ethernet.h>
#include "ModbusTCP_RU.h"     // Библиотека для работы с Modbus TCP (Slave)
#include <GyverHTU21D.h>      // Библиотека для работы с датчиком HTU21D

// Экземпляры объектов
ModbusTCP_RU Mb;              // Создаем экземпляр Modbus Slave
GyverHTU21D htu;              // Создаем экземпляр датчика HTU21D

// Настройки сети
byte mac[] = {0x90, 0xA5, 0xDA, 0x0E, 0x94, 0xB5}; // MAC-адрес устройства
IPAddress ip(192, 168, 1, 100);       // Статический IP-адрес
IPAddress gateway(192, 168, 1, 1);    // IP-адрес шлюза
IPAddress subnet(255, 255, 255, 0);   // Маска подсети

// Параметры вывода данных
const long serialPrintInterval = 1000; // Интервал вывода в Serial (в мс)
unsigned long lastSerialPrint = 0;     // Хранит время последнего вывода в Serial

void setup() {
  // Инициализация последовательного порта для диагностики
  Serial.begin(9600);
  Serial.println("HTU21D + Modbus TCP запущен...");

  // Инициализация Ethernet
  Ethernet.begin(mac, ip, gateway, subnet);
  delay(1000); // Даём время на установку сетевого соединения

  // Проверка наличия физического подключения Ethernet
  if (Ethernet.linkStatus() == LinkOFF) {
    Serial.println("Ошибка: Ethernet не подключен!");
    while (true); // Останавливаем выполнение программы
  }

  // Проверка связи с датчиком HTU21D
  if (!htu.begin()) {
    Serial.println("Ошибка: HTU21D не найден!");
    while (true); // Останавливаем выполнение программы
  }

  // Инициализация регистров Modbus
  Mb.MbData[0] = 0; // Регистр 0: Температура × 100
  Mb.MbData[1] = 0; // Регистр 1: Влажность × 100

  // Вывод информации о сетевых настройках
  Serial.print("IP-адрес устройства: ");
  Serial.println(Ethernet.localIP());
}

void loop() {
  Mb.MbsRun(); // Обработка входящих Modbus-запросов

  // Чтение с датчика HTU21D (выполняется ~раз в секунду)
  if (htu.readTick()) {
    float temperature = htu.getTemperature();  // Получаем температуру (°C)
    float humidity = htu.getHumidity();        // Получаем влажность (%)

    // Преобразуем значения в формат Modbus (умножение на 100)
    int tempFixed = static_cast<int>(temperature * 100); // 23.33 → 2333
    int humFixed = static_cast<int>(humidity * 100);     // 43.33 → 4333

    // Обновляем значения в регистрах Modbus
    Mb.MbData[0] = tempFixed;
    Mb.MbData[1] = humFixed;

    // Периодический вывод данных в Serial порт
    if (millis() - lastSerialPrint >= serialPrintInterval) {
      Serial.print("Температура: ");
      Serial.print(tempFixed / 100.0, 2);         // Восстанавливаем десятичный вид
      Serial.print(" °C | Влажность: ");
      Serial.print(humFixed / 100.0, 2);          // То же самое
      Serial.println(" %");
      lastSerialPrint = millis();                 // Обновляем время последнего вывода
    }
  }
}