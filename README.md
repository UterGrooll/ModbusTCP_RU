# ModbusTCP_RU

`ModbusTCP_RU` — лёгкая библиотека Arduino для работы по `Modbus TCP` в режиме `Slave`.

Развиваю её для своих Arduino-проектов со `SCADA` и Ethernet-модулями `W5100` / `W5500`.

Главная задача — удобная работа с реле, входами, датчиками и настройками без лишней нагрузки на Arduino. Только то, что нужно в моих устройствах.

## Общая информация

- Версия: `0.3.0`
- Режим работы: `Modbus TCP Slave`
- Разработка: UterGrooll
- Проверял с `W5500`; `0.3.0` проверена на моём стенде `Arduino UNO + Ethernet` с проектом `ScadaRelayTimer`
- Среда разработки: `Arduino IDE 2.3.5`

## Назначение

Использую библиотеку для:

- подключения Arduino к SCADA;
- простых модулей релейных выходов;
- модулей дискретных входов;
- телеметрии;
- датчиков температуры, влажности, напряжения;
- небольших Arduino PLC / I/O устройств.

## Возможности

- Режим `Modbus TCP Slave`
- Лёгкий профиль для `AVR` включается автоматически
- Два TCP-клиента по умолчанию на UNO / Nano
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
- Пошаговый приём TCP с ограничением работы за проход
- Callback при записи coil / holding register
- Совместимость со старыми скетчами через `MbData[]`

## Требования

- Arduino-совместимая плата
- Ethernet-контроллер `W5100` или `W5500`
- Библиотека `SPI`
- Библиотека `Ethernet >= 2.0.2` (проверено с `2.0.2`)

Для части примеров дополнительно требуются:

- `GyverHTU21D 1.1.2` — для двух датчиковых примеров

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
MB_MAX_CLIENTS   2
MB_BUFFER_SIZE   128
```

Размеры меняются в общем файле [`src/ModbusTCP_RU_config.h`](src/ModbusTCP_RU_config.h), либо флагами **для всей сборки**. Определять их только в `.ino` нельзя: скетч и библиотечный `.cpp` компилируются отдельно. Конструктор проверяет согласованность размеров через тип конфигурации; разные размеры дают ошибку линковки вместо повреждения памяти.

Буфер должен вмещать запросы и ответы выбранной карты. Например, для 64 holding-регистров нужен буфер не меньше 141 байта (`13 + 64 * 2` для FC16). Несовместимые параметры останавливают сборку через `static_assert`. Верхний предел ADU: 260 байт. Большие карты обслуживаются порциями в пределах протокола.

На остальных платах по умолчанию: четыре области по 128 элементов, четыре клиента и буфер 260 байт на клиента. Один аппаратный сокет резервируется под listener; при W5100 принимается не более трёх клиентов даже в FULL. Память выделяется статически; динамических таблиц callback нет.

## Базовое использование

```cpp
#include <SPI.h>
#include <Ethernet.h>
#include "ModbusTCP_RU.h"

ModbusTCP_RU Mb;

byte mac[] = {0x90, 0xA2, 0xDA, 0x0D, 0x3F, 0xCD};
IPAddress ip(192, 168, 1, 100);
IPAddress gateway(192, 168, 1, 1);
IPAddress dnsServer(192, 168, 1, 1);
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

  Ethernet.begin(mac, ip, dnsServer, gateway, subnet);
  Mb.begin();
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

Callback общий для всей области. Адрес передаётся в обработчик. `Coil/Hreg` вызывают его при каждой записи, даже если значение не изменилось. Изменение и повторную команду при необходимости различает скетч.

Вложенная запись в ту же область меняет значение, но не вызывает callback повторно. Для явной записи без уведомлений есть `setCoilLocal(address, value)` и `setHoldingLocal(address, value)`. FC15/16 сначала записывают весь проверенный диапазон, затем вызывают обработчики.

Перегрузки `onCoilWrite(address, callback)` и `onHoldingWrite(address, callback)` в старой версии игнорировали адрес. В 0.3.0 они запрещены при компиляции: используйте общий callback с проверкой адреса.

Обработчик должен быть коротким. Не вызывайте из него `MbsRun/begin/restart`: повторный вход заблокирован. Callback уведомляет о записи, а не подтверждает выполнение физического действия; отклонение прикладных уставок до записи пока не реализовано.

### Управление сервером (begin / restart)

- `bool begin()`: запускает один слушающий сокет. Повторный вызов не занимает новый. Возвращает наличие listener; `MbsRun()` также пытается поднять его автоматически.
- `bool restart()`: закрывает принятые и ожидающие принятия соединения, сохраняет существующий listener или создаёт его, если он пропал. Карта и callbacks сохраняются.
- `restart()` не сбрасывает Ethernet-чип, не меняет IP и не перезагружает Arduino. Клиент после него должен подключиться заново.

Один объект `ModbusTCP_RU` на Ethernet-интерфейс. Копирование объекта запрещено. Не обращайтесь к глобальному `MbServer`: символ оставлен только для совместимости линковки.

При восстановлении физического линка W5500 пример вызывает `Mb.restart()` после устойчивого `LinkON`; результат нужно проверить на конкретном устройстве. У W5100 `Unknown` является нормальным результатом `linkStatus()`, а не причиной периодического reset. Если оба слота заняты зависшими соединениями, их освобождение может занять до idle timeout (по умолчанию 60 с). Аппаратный сброс чипа и его повторная настройка драйвером требуют отдельной проверки; обычный `Ethernet.begin()` не гарантирует полную аппаратную переинициализацию.

### Таймауты и ограничения

Настройка в `setup()`, без `define` в скетче:

```cpp
Mb.setIdleTimeout(60000);     // тишина, в том числе до первого запроса
Mb.setPacketTimeout(1000);    // пауза между порциями запроса
Mb.setResponseTimeout(1000);  // ожидание свободного TX-буфера
```

`0` отключает соответствующий таймаут. Полный запрос дополнительно ограничен `MB_FRAME_TIMEOUT = 5000` мс из общего конфигурационного файла. Все таймеры учитывают переполнение `millis()`.

Приём выполняется порциями до 32 байт на клиента за проход; обрабатывается не больше одного запроса клиента за проход. Сначала читаются уже поступившие байты, чтобы задержка пользовательского `loop()` не вызывала ложный межбайтовый таймаут.

Если TX-буфер занят, ответ сохраняется в том же буфере до следующего прохода. Однако `EthernetClient::write()` в Ethernet 2.x может ждать аппаратного `SEND_OK`. Поэтому **строго неблокирующий сервер пока не заявляется**. Таймаут ответа не прерывает уже выполняющийся `write()`. Закрытие клиентов не ждёт штатную секунду graceful shutdown. Аппаратный WDT и политику безопасного состояния выходов задаёт приложение.

### Диагностика

`stats()`: `rxPackets`, `txPackets`, `exceptionCount`, `socketErrors`, `malformedFrames`, `timeouts`, `rejectedClients`. Успешная отправка учитывается только после успешного возврата драйвера; это не подтверждение прикладной обработки SCADA. `crcErrors` оставлен для совместимости и всегда равен нулю: Modbus TCP CRC не проверяет. `resetStats()` обнуляет счётчики.

Для Serial-лога включите `MB_DEBUG` в общем файле конфигурации и запустите Serial в скетче. Без этого библиотека не печатает в Serial.

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

- [`examples/relay_slave_watchdog/relay_slave_watchdog.ino`](examples/relay_slave_watchdog/relay_slave_watchdog.ino)  
  Реле + Ethernet link watchdog с `Mb.restart()` — рекомендуемая схема устойчивости к пропаданию линка (W5500).

### Sensor Slave

- [`examples/sensor_htu21d_lite/sensor_htu21d_lite.ino`](examples/sensor_htu21d_lite/sensor_htu21d_lite.ino)  
  Минимальный Modbus TCP Slave для `HTU21D`.

- [`examples/sensor_htu21d_serial/sensor_htu21d_serial.ino`](examples/sensor_htu21d_serial/sensor_htu21d_serial.ino)  
  `HTU21D` с выводом значений в `Serial`.

В датчиковых примерах отсутствие датчика не останавливает Modbus. Подключение повторяется раз в секунду. До первого успешного измерения и при ошибке чтения `Input Register 0` содержит `0x8000` (температура недоступна), а `Input Register 1` — `0xFFFF` (влажность недоступна). Температуру в SCADA отображайте как signed int16 и делите на 100; влажность — unsigned int16 / 100, исключая значения ошибки.

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
- Только Slave на всех платформах. Legacy Master API удалён; для него остаётся ветка истории `0.2.x`.
- Для UNO / Nano по умолчанию используются два TCP-клиента.
- TCP-сервер запускается при первом вызове `MbsRun()`.
- Адресация Modbus начинается с `0`.
- Размер карты памяти задаётся в общей конфигурации сборки.
- Unit ID и Transaction ID возвращаются из запроса; фильтрации Unit ID у standalone Slave нет.
- Неверный MBAP/Protocol ID или ADU больше буфера закрывает соединение. Ошибки поддерживаемых PDU возвращают exceptions 01/02/03. Код 04 оставлен в enum, но автоматически не выдаётся.

## Изменения в 0.3.0

- Исправлена обработка нескольких TCP-клиентов: один сокет, один парсер, приём через `accept()`.
- Исправлены короткие и склеенные запросы, проверка Protocol ID, длины, количества элементов и границ буфера.
- Исправлены повторные `begin/restart`, учёт молчаливых подключений и отказ лишнему клиенту.
- Добавлены runtime-таймауты, общий файл конфигурации и проверки размеров при сборке.
- Убрана рекурсия callbacks, добавлены локальные setters без уведомлений.
- Исправлены границы 32-битных helpers и порядок обновления FC15/16.
- Удалён legacy Master; запрещены вводившие в заблуждение адресные overloads callback.
- Исправлен порядок параметров Ethernet в примерах; обновлён пример восстановления линка.
- Датчиковые примеры не останавливают сервер при отсутствии HTU21D и повторяют подключение при ошибке чтения.

### Переход с 0.2.x

Сохраняются `Coil/Hreg/Ireg/Discrete/MbsRun`, общий callback и `MbData[]` для holding. Удалите локальные defines размеров из скетча, используйте общий config. Настраивайте таймауты методами объекта. Замените `extern MbServer` на `Mb.begin()/Mb.restart()`. Схема EEPROM и карта пользовательского устройства от обновления библиотеки не меняются.

## Изменения в 0.2.0

Добавлены первые `begin/restart` и настройка idle timeout. Ошибки их поведения и прежняя рекомендация задавать defines только в скетче исправлены в 0.3.0.

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

- `0.3.0`
  Надёжнее TCP и проверка запросов, согласованная конфигурация, безопасные callbacks, только Slave.

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

## Проверка на устройстве

На своём стенде `Arduino UNO + Ethernet` с `ScadaRelayTimer` проверил обмен через Modbus Poll, отключение питания, перезагрузки и отключение/возврат патч-корда. Соединение восстанавливается стабильно.

Это результат проверки конкретного устройства. Датчиковые примеры и длительную работу под нагрузкой проверяю отдельно.

## Дополнительные файлы

Схемы подключения и дополнительные материалы находятся в каталоге [`schematics/`](schematics/):

- `relay_slave.png`
- `sensor_htu21d.png`
- `guide.pdf`

`guide.pdf` — прежний иллюстрированный материал по подключению и Modbus Poll. Его скриншоты относятся к старой карте holding-регистров (`FC03`); в текущих датчиковых примерах используются input-регистры (`FC04`). Актуальные адреса и функции указаны в скетчах и этом README.

Архив библиотеки расположен в [`download/ModbusTCP_RU.zip`](download/ModbusTCP_RU.zip).

Уведомления об исходном проекте сохранены отдельно в [`NOTICE.md`](NOTICE.md).
