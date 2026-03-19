# ModbusTCP_RU 0.1.3

Архив `ModbusTCP_RU.zip` содержит актуальную версию библиотеки для ручной установки в Arduino IDE.

## Установка

1. Распаковать архив `ModbusTCP_RU.zip`
2. Скопировать папку библиотеки в каталог:

```text
<Документы>/Arduino/libraries/ModbusTCP_RU
```

3. Перезапустить Arduino IDE

После установки библиотека будет доступна в списке установленных библиотек и примеров.

## Содержимое

Архив включает:

- исходный код библиотеки
- примеры использования
- схемы подключения
- файл `library.properties`
- документацию `README.md`

## Примеры

В состав библиотеки входят следующие примеры:

- `relay_slave_lite`
- `relay_slave_serial`
- `sensor_htu21d_lite`
- `sensor_htu21d_serial`
- `master_read_slave`

## Назначение примеров

- `relay_slave_lite`  
  Минимальный Modbus TCP slave для управления реле

- `relay_slave_serial`  
  Slave для управления реле с выводом диагностики в `Serial`

- `sensor_htu21d_lite`  
  Минимальный Modbus TCP slave для передачи температуры и влажности

- `sensor_htu21d_serial`  
  Slave для датчика `HTU21D` с выводом значений в `Serial`

- `master_read_slave`  
  Прямой обмен `Master -> Slave` без SCADA

## Сетевые параметры

В каждом примере необходимо проверить и при необходимости изменить:

- `mac[]`
- `ip`
- `gateway`
- `subnet`

Для master-примеров также настраивается:

- `Mb.remSlaveIP`

## Дополнительные материалы

Схемы и вспомогательные файлы находятся в каталоге `schematics/`.

## Версия

Текущая версия архива: `0.1.3`
