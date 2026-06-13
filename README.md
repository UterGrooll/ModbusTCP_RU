# ModbusTCP_RU

`ModbusTCP_RU` — лёгкая библиотека Arduino для работы по `Modbus TCP` в режиме `Slave`.

Библиотека предназначена для простых Arduino-проектов, которые нужно быстро подключить к `SCADA`, `HMI` или другому Modbus TCP клиенту через Ethernet-модуль `W5100` / `W5500`.

Главная идея версии `0.2.0` — не делать тяжёлый универсальный стек, а дать быстрый и предсказуемый Modbus TCP Slave для Arduino I/O модулей.

## Общая информация

- Версия: `0.2.0`
- Режим работы: `Modbus TCP Slave`
- Оригинальный автор: Marco Gerritse (2013) https://myarduinoprojects.com/modbus.html
- Форк и доработка: UterGrooll
- Протестировано с: `W5500`
- Среда разработки: `Arduino IDE 2.3.5`

## Назначение

Библиотека подходит для:

- подключения Arduino к SCADA;
- простых модулей релейных выходов;
- модулей дискретных входов;
- телеметрии;
- датчиков температуры, влажности, напряжения;
- небольших Arduino PLC / I/O устройств.

## Возможности

- Режим `Modbus TCP Slave`
- Лёгкий профиль для `AVR` включается автоматически
- Один SCADA-клиент по умолчанию на UNO / Nano
- Раздельные области памяти Modbus
- Поддержка стандартных функций:
  - `FC01` Read Coils
  - `FC02` Read Discrete Inputs
  - `FC03` Read Holding Registers
  - `FC04` Read Input Registers
  - `FC05` Write Single Coil
  - `FC06` Write Single Register
  - `FC15` Write Multiple Coils
  - `FC16` Write Multiple Registers
- Modbus Exception Responses:
  - `01` Illegal Function
  - `02` Illegal Data Address
  - `03` Illegal Data Value
  - `04` Slave Device Failure
- Неблокирующая обработка TCP
- Callback при записи coil / holding register
- Совместимость со старыми скетчами через `MbData[]`

## Требования

- Arduino-совместимая плата
- Ethernet-контроллер `W5100` или `W5500`
- Библиотека `SPI`
- Библиотека `Ethernet`

Для части примеров дополнительно требуются:

- `GyverHTU21D`
- `GyverDS18`

## Модель данных

Области памяти разделены по Modbus-модели:

| Function | Область памяти | API |
| -------- | -------------- | --- |
| `FC01` | Coils | `Coil()` |
| `FC02` | Discrete Inputs | `Discrete()` |
| `FC03` | Holding Registers | `Hreg()` |
| `FC04` | Input Registers | `Ireg()` |
| `FC05` / `FC15` | Coils | `onCoilWrite()` |
| `FC06` / `FC16` | Holding Registers | `onHoldingWrite()` |

По умолчанию для `AVR` используется лёгкая карта:

```cpp
MB_MAX_COILS     8
MB_MAX_DISCRETE  8
MB_MAX_HOLDING   16
MB_MAX_INPUT     16
MB_MAX_CLIENTS   1
MB_BUFFER_SIZE   128
```

Для более крупных плат размеры можно переопределить до подключения библиотеки:

```cpp
#define MB_MAX_COILS 32
#define MB_MAX_DISCRETE 32
#define MB_MAX_HOLDING 64
#define MB_MAX_INPUT 64

#include "ModbusTCP_RU.h"
```

## Базовое использование

```cpp
#include <SPI.h>
#include <Ethernet.h>
#include "ModbusTCP_RU.h"

ModbusTCP_RU Mb;

byte mac[] = {0x90, 0xA2, 0xDA, 0x0D, 0x3F, 0xCD};
IPAddress ip(192, 168, 1, 100);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);

const byte RELAY_PIN = 2;

void onCoilWrite(word address, bool value) {
  if (address == 0) {
    digitalWrite(RELAY_PIN, value ? HIGH : LOW);
  }
}

void setup() {
  pinMode(RELAY_PIN, OUTPUT);

  Mb.Coil(0, false);       // FC01 / FC05
  Mb.Ireg(0, 0);           // FC04
  Mb.onCoilWrite(onCoilWrite);

  Ethernet.begin(mac, ip, gateway, subnet);
}

void loop() {
  Mb.MbsRun();

  Mb.Ireg(0, analogRead(A0));
}
```

## Публичный интерфейс

### Coils

- `bool Coil(word address) const`
- `bool Coil(word address, bool value)`
- `bool CoilRead(word address) const`
- `bool CoilWrite(word address, bool value)`

Используется для реле, выходов и команд от SCADA.

### Discrete Inputs

- `bool Discrete(word address) const`
- `bool Discrete(word address, bool value)`
- `bool DiscreteRead(word address) const`
- `bool DiscreteWrite(word address, bool value)`

Используется для кнопок, концевиков и дискретных входов.

### Holding Registers

- `word Hreg(word address) const`
- `bool Hreg(word address, word value)`

Используется для настроек, уставок и значений, которые SCADA может записывать.

### Input Registers

- `word Ireg(word address) const`
- `bool Ireg(word address, word value)`

Используется для телеметрии: температура, напряжение, ток, счётчики.

### Callbacks

```cpp
void onCoilWrite(word address, bool value) {
  if (address == 0) {
    digitalWrite(2, value ? HIGH : LOW);
  }
}

Mb.onCoilWrite(onCoilWrite);
```

Callback общий для всей области. Адрес передаётся в обработчик.

### Управление сервером (begin / restart)

Начиная с `0.2.0` сервером можно управлять методами объекта, не обращаясь к внутреннему `EthernetServer`:

- `void begin()` — поднять слушающий сокет (идемпотентно). Можно вызвать в `setup()` явно.
- `void restart()` — закрыть все клиентские слоты и заново поднять сервер. Вызывать после `Ethernet.begin()` при восстановлении линка / в watchdog'е, чтобы не оставлять «мусорные» слоты (они давали «приём по 0 байт» после реинициализации).

```cpp
void restartEthernet() {
  Ethernet.begin(mac, ip, gateway, subnet);
  delay(500);
  Mb.restart();   // re-begin сервера + очистка клиентских слотов
}
```

Раньше для этого приходилось объявлять `extern EthernetServer MbServer;` и звать `MbServer.begin()` — теперь это не нужно.

### Таймаут простоя клиента

`MB_IDLE_TIMEOUT` (мс) — после простоя без пакетов «тихий» клиентский сокет закрывается, чтобы освободить ресурс. По умолчанию `60000` (в `0.1.4` было жёстко `30000`). На медленной или многоустройственной линии SCADA опрос одного устройства может приходить реже — слишком малый таймаут заставляет библиотеку самой рвать живое соединение. Значение переопределяется до подключения библиотеки, `0` отключает закрытие по простою:

```cpp
#define MB_IDLE_TIMEOUT 0   // не закрывать соединение по простою
#include "ModbusTCP_RU.h"
```

### Совместимость

`MbData[]` оставлен для старых скетчей.

Теперь `MbData[]` — это alias на `Holding Registers`.

Новые проекты лучше писать через:

- `Coil()`
- `Discrete()`
- `Hreg()`
- `Ireg()`

## Примеры

В каталоге [`examples/`](examples/) доступны готовые примеры.

### Relay Slave

- [`examples/relay_slave_lite/relay_slave_lite.ino`](examples/relay_slave_lite/relay_slave_lite.ino)  
  Минимальный пример управления реле через `Coil 0`.

- [`examples/relay_slave_serial/relay_slave_serial.ino`](examples/relay_slave_serial/relay_slave_serial.ino)  
  Управление реле с диагностикой через `Serial`.

### Sensor Slave

- [`examples/sensor_htu21d_lite/sensor_htu21d_lite.ino`](examples/sensor_htu21d_lite/sensor_htu21d_lite.ino)  
  Минимальный Modbus TCP Slave для `HTU21D`.

- [`examples/sensor_htu21d_serial/sensor_htu21d_serial.ino`](examples/sensor_htu21d_serial/sensor_htu21d_serial.ino)  
  `HTU21D` с выводом значений в `Serial`.

## Сетевая конфигурация

В каждом примере задаются локальные сетевые параметры:

- `mac[]`
- `ip`
- `gateway`
- `subnet`

SCADA подключается к Arduino по IP-адресу платы и порту `502`.

## Особенности текущей реализации

- Библиотека ориентирована на `Slave` для SCADA.
- На `AVR` автоматически включается лёгкий профиль.
- Master-код на `AVR` не компилируется по умолчанию.
- Для UNO / Nano по умолчанию используется один TCP-клиент.
- TCP-сервер запускается при первом вызове `MbsRun()`.
- Адресация Modbus начинается с `0`.
- Размер карты памяти задаётся через `define`.

## Изменения в 0.2.0

- Добавлены методы `begin()` и `restart()` для управления сервером без обращения к глобальному `EthernetServer` (убирает хак `extern EthernetServer MbServer`).
- `restart()` закрывает все клиентские слоты и заново поднимает сервер — корректное восстановление после `Ethernet.begin()` (лечит «приём по 0 байт» после моргания линка).
- `MB_IDLE_TIMEOUT` теперь переопределяется через `#define` до подключения библиотеки; значение по умолчанию увеличено с `30000` до `60000` мс; `0` отключает закрытие соединения по простою.
- Настройка остаётся в едином стиле — только через `#define`, без рантайм-сеттеров (сохранение лёгкости).

## Изменения в 0.1.4

- Библиотека переведена в философию лёгкого `Modbus TCP Slave` для SCADA.
- Для `AVR` автоматически включается малый профиль памяти.
- Уменьшено потребление SRAM на UNO / Nano.
- Добавлены раздельные области памяти:
  - coils;
  - discrete inputs;
  - holding registers;
  - input registers.
- Обновлены обработчики `FC01`–`FC16`.
- Добавлены стандартные Modbus exception responses.
- Добавлен callback при записи coil / holding register.
- Обновлены примеры под SCADA Slave.
- Пример Master-Slave заменён на пример карты памяти SCADA.

## История версий

- `0.2.0`
  `begin()` / `restart()` для чистого управления сервером и восстановления линка; настраиваемый `MB_IDLE_TIMEOUT` (по умолчанию 60 с).

- `0.1.4`
  Лёгкий Modbus TCP Slave для Arduino + SCADA, раздельная карта памяти, малый профиль для AVR.

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
