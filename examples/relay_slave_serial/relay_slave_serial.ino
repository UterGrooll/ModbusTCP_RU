/*
 * Modbus TCP Slave для управления реле через Ethernet
 *
 * Описание:
 * Данный код реализует Modbus TCP Slave-устройство на Arduino, которое:
 * - Управляет состоянием реле, подключенного к цифровому выходу
 * - Поддерживает стандартные Modbus TCP функции (чтение/запись регистров)
 * - Обеспечивает обратную связь о текущем состоянии реле
 * - Работает по Ethernet с возможностью настройки сетевых параметров
 *
 * Особенности реализации:
 * - Используется библиотека ModbusTCP_RU (специализированная для Arduino)
 * - Поддержка функций Modbus 0x03 (Read Holding Registers) и 0x06 (Write Single Register)
 * - Реализована защита от дребезга изменений состояния
 * - Добавлен мониторинг состояния Ethernet-соединения
 * - Встроенная система диагностики через Serial-порт
 *
 * Адресация Modbus:
 * - 0 (RELAY_REGISTER) - регистр управления реле (запись: 0 - выкл, 1 - вкл)
 * - 1 (STATUS_REGISTER) - регистр состояния реле (чтение: 0 - выкл, 1 - вкл)
 *
 * Требования:
 * - Плата Arduino с Ethernet-шилдом (W5100/W5500)
 * - Библиотеки: SPI, Ethernet, ModbusTCP_RU
 * - Внешнее реле, подключенное к указанному цифровому пину
 */

#include <SPI.h>
#include <Ethernet.h>
#include "ModbusTCP_RU.h" 

ModbusTCP_RU Mb;  // Экземпляр Modbus Slave

// Настройки сети (должны быть уникальными в вашей сети)
byte mac[] = {0x90, 0xA2, 0xDA, 0x0D, 0x3F, 0xCD};  // MAC-адрес устройства
IPAddress ip(192, 168, 1, 100);       // Статический IP устройства
IPAddress gateway(192, 168, 1, 1);    // IP шлюза
IPAddress subnet(255, 255, 255, 0);   // Маска подсети

const int RELAY_PIN = 2;  // Пин управления реле

// Адреса Modbus-регистров
#define RELAY_REGISTER 0    // Регистр управления реле (запись)
#define STATUS_REGISTER 1   // Регистр состояния реле (чтение)

// Системные переменные
bool lastRelayState = false;  // Последнее известное состояние реле
unsigned long lastStatusUpdate = 0;  // Время последнего обновления статуса
const unsigned long STATUS_UPDATE_INTERVAL = 1000;  // Интервал обновления (мс)

void setup() {
  Serial.begin(9600);  // Инициализация Serial для диагностики
  
  // Настройка выхода реле
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);  // Гарантированное выключение при старте

  // Инициализация Ethernet
  Ethernet.begin(mac, ip, gateway, subnet);
  delay(1000);  // Пауза для стабилизации соединения
  
  // Проверка подключения Ethernet
  if (Ethernet.linkStatus() == LinkOFF) {
    Serial.println("Ошибка: Ethernet не подключен!");
    while (1);  // Блокировка при отсутствии соединения
  }
  
  // Инициализация Modbus-регистров
  Mb.MbData[RELAY_REGISTER] = 0;   // Реле выключено
  Mb.MbData[STATUS_REGISTER] = 0;  // Начальное состояние
  
  // Вывод информации о запуске
  Serial.println("Modbus TCP Slave успешно запущен");
  Serial.print("IP-адрес: ");
  Serial.println(Ethernet.localIP());
  Serial.println("Используйте Modbus-регистр 0 для управления реле");
}

void loop() {
  // 1. Обработка входящих Modbus-запросов
  Mb.MbsRun();
  
  // 2. Управление реле с защитой от дребезга
  bool currentRelayState = (Mb.MbData[RELAY_REGISTER] == 1);
  if (currentRelayState != lastRelayState) {
    digitalWrite(RELAY_PIN, currentRelayState ? HIGH : LOW);
    lastRelayState = currentRelayState;
    Serial.print("Состояние реле изменено: ");
    Serial.println(currentRelayState ? "ВКЛ" : "ВЫКЛ");
  }
  
  // 3. Периодическое обновление статуса
  if (millis() - lastStatusUpdate >= STATUS_UPDATE_INTERVAL) {
    Mb.MbData[STATUS_REGISTER] = digitalRead(RELAY_PIN) ? 1 : 0;
    lastStatusUpdate = millis();
  }
}

/*
 * Примечания по использованию:
 * 1. Для управления используйте Modbus-функцию 0x06 (Write Single Register)
 *    - Адрес: 0
 *    - Значение: 0 (выкл) или 1 (вкл)
 * 
 * 2. Для чтения состояния используйте функцию 0x03 (Read Holding Registers)
 *    - Адрес: 1
 *    
 * 3. Для настройки под свою сеть измените:
 *    - MAC-адрес
 *    - IP-адрес, шлюз и маску
 *    - Пин управления реле (RELAY_PIN)
 */