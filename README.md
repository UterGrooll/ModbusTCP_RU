# ModbusTCP_RU

`ModbusTCP_RU` — библиотека Arduino для работы по `Modbus TCP` в режимах `Master` и `Slave`.

Библиотека предназначена для плат Arduino с Ethernet-контроллерами `W5100` и `W5500` и подходит как для интеграции со SCADA, так и для прямого обмена между устройствами.

## Общая информация

- Версия: `0.1.3`
- Оригинальный автор: Marco Gerritse (2013) https://myarduinoprojects.com/modbus.html
- Форк и сопровождение: UterGrooll
- Протестировано с: `W5500`
- Среда разработки: `Arduino IDE 2.3.5`

## Возможности

- Режим `Modbus TCP Slave`
- Режим `Modbus TCP Master`
- Общий массив `MbData[]` для регистров и побитового доступа
- Адресация с нуля
- Поддержка стандартных функций Modbus:
  - `1` Read Coils
  - `2` Read Discrete Inputs
  - `3` Read Holding Registers
  - `4` Read Input Registers
  - `5` Write Single Coil
  - `6` Write Single Register
  - `15` Write Multiple Coils
  - `16` Write Multiple Registers

## Требования

- Arduino-совместимая плата
- Ethernet-контроллер `W5100` или `W5500`
- Библиотека `SPI`
- Библиотека `Ethernet`

Для части примеров также требуется:

- `GyverHTU21D`

## Модель данных

Все Modbus-данные хранятся в массиве `MbData[]`.

- `MB_DATA_LEN = 30`
- Доступно `30` 16-битных регистров
- Доступно `30 * 16 = 480` бит

Один и тот же массив используется и для 16-битного доступа, и для побитовой работы.

## Базовое использование

### Slave

```cpp
#include <SPI.h>
#include <Ethernet.h>
#include "ModbusTCP_RU.h"

ModbusTCP_RU Mb;

byte mac[] = {0x90, 0xA2, 0xDA, 0x0D, 0x3F, 0xCD};
IPAddress ip(192, 168, 1, 100);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);

void setup() {
  Ethernet.begin(mac, ip, gateway, subnet);
  Mb.MbData[0] = 123;
}

void loop() {
  Mb.MbsRun();
}
```

### Master

```cpp
#include <SPI.h>
#include <Ethernet.h>
#include "ModbusTCP_RU.h"

ModbusTCP_RU Mb;

byte mac[] = {0x90, 0xA5, 0xDA, 0x0E, 0x94, 0xB6};
IPAddress ip(192, 168, 1, 110);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);

void setup() {
  Ethernet.begin(mac, ip, gateway, subnet);
  Mb.remSlaveIP = IPAddress(192, 168, 1, 50);
}

void loop() {
  Mb.Req(MB_FC_READ_REGISTERS, 0, 2, 0);
  Mb.MbmRun();
}
```

## Публичный интерфейс

### Доступ к данным

- `word MbData[MB_DATA_LEN]`  
  Общий массив данных Modbus.

- `boolean GetBit(word Number)`  
  Возвращает значение бита по битовому адресу.

- `boolean SetBit(word Number, boolean Data)`  
  Записывает значение бита по битовому адресу.

- `word GetDataLen()`  
  Возвращает длину массива регистров.

### Методы Master

- `void Req(MB_FC FC, word Ref, word Count, word Pos)`  
  Формирует и отправляет Modbus TCP-запрос.

- `void MbmRun()`  
  Принимает и обрабатывает ответ в режиме master.

- `IPAddress remSlaveIP`  
  IP-адрес удалённого slave-устройства.

### Методы Slave

- `void MbsRun()`  
  Обрабатывает входящие Modbus TCP-запросы.

## Примеры

В каталоге [`examples/`](examples/) доступны готовые примеры.

### Relay Slave

- [`examples/relay_slave_lite/relay_slave_lite.ino`](examples/relay_slave_lite/relay_slave_lite.ino)  
  Минимальный пример управления реле.  
  Регистр `0`: команда на реле (`0` = выкл., `1` = вкл.).

- [`examples/relay_slave_serial/relay_slave_serial.ino`](examples/relay_slave_serial/relay_slave_serial.ino)  
  Управление реле с диагностикой через `Serial`.  
  Регистр `0`: команда на реле.  
  Регистр `1`: фактическое состояние реле.

### HTU21D Sensor Slave

- [`examples/sensor_htu21d_lite/sensor_htu21d_lite.ino`](examples/sensor_htu21d_lite/sensor_htu21d_lite.ino)  
  Минимальный Modbus TCP slave для `HTU21D`.  
  Регистр `0`: температура `* 100`.  
  Регистр `1`: влажность `* 100`.

- [`examples/sensor_htu21d_serial/sensor_htu21d_serial.ino`](examples/sensor_htu21d_serial/sensor_htu21d_serial.ino)  
  `HTU21D` slave с выводом значений в `Serial`.  
  Регистр `0`: температура `* 100`.  
  Регистр `1`: влажность `* 100`.

### Прямой обмен Master-Slave

- [`examples/master_read_slave/master_read_slave.ino`](examples/master_read_slave/master_read_slave.ino)  
  Прямой обмен без SCADA.  
  Пример работает как `Master` и читает два holding-регистра у удалённого `Slave`.

## Сетевая конфигурация

В каждом примере задаются локальные сетевые параметры:

- `mac[]`
- `ip`
- `gateway`
- `subnet`

В режиме master адрес удалённого устройства задаётся через:

- `Mb.remSlaveIP`

Все устройства в одной сети должны иметь уникальные IP-адреса и MAC-адреса.

## Особенности текущей реализации

- В режиме slave TCP-сервер запускается при первом вызове `MbsRun()`.
- В режиме master для подключения используется `remSlaveIP`.
- Для `MbData[]` и битового доступа добавены проверки границ.
- Размер буфера запросов и ответов задаётся через `MB_BUFFER_SIZE = 260`.

## Изменения в 0.1.3

- Исправлены критичные проверки границ для регистров и битов.
- Убран жёстко заданный IP-адрес master-устройства.
- Master теперь использует `remSlaveIP`.
- Автоматизирован запуск TCP-сервера в режиме slave.
- Улучшена обработка буферов запросов и ответов.
- Обновлены примеры в соответствии с текущим поведением библиотеки.
- Добавлен пример прямого обмена `Master -> Slave` без SCADA.

## История версий

- `0.1.3`  
  Исправления стабильности master/slave-обмена, проверки границ и обновление примеров.

- `0.1.2`  
  Крупная переработка библиотеки, улучшение производительности и читаемости.

- `0.1.1`  
  Исправление ошибок.

- `0.1.0`  
  Первая версия.

## Дополнительные файлы

Схемы подключения и дополнительные материалы находятся в каталоге [`schematics/`](schematics/):

- `relay_slave.png`
- `sensor_htu21d.png`
- `guide.pdf`

Архив библиотеки расположен в [`download/ModbusTCP_RU.zip`](download/ModbusTCP_RU.zip).
