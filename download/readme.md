## Установка вручную

1. Скачайте и распакуйте архив - [ModbusTCP_RU.zip](download/ModbusTCP_RU.zip) с библиотекой в папку:

```
C:\Users\UterGrooll\Documents\Arduino\libraries\ModbusTCP_RU
```

2. Перезапустите Arduino IDE, если она была открыта.

3. Скачайте пример:

   - 📁 Найти его можно в папке [`examples/`](examples/)

4. Подключите Ethernet-модуль W5500 к Arduino по схеме:

   - 📁 Найти её можно в папке [`schematics/`](schematics/)

5. Загрузите скетч на плату и проверьте работу через Modbus-клиент.

6. Настройте сетевой адаптер компьютера на статический IP-адрес:

   - Пуск → Параметры → Сеть и Интернет → Ethernet → Изменить параметры подключения
   - Установите:
     - **IP-адрес:** `192.168.1.101`
     - **Маска подсети:** `255.255.255.0`
     - **Шлюз:** можно оставить пустым

7. Подключитесь патч-кордом к сетевому модулю W5500 и проверьте доступность по сети в командной строке(cmd) выполните:

```bash
ping 192.168.1.100
```
если:

```
Ответ от 192.168.1.100: число байт=32 время=4мс TTL=128

Статистика Ping для 192.168.1.100:
    Пакетов: отправлено = 4, получено = 4, потеряно = 0
    (0% потерь)
```

все ок, плата в сети.

---

## Тест в Modbus Poll

1. Откройте **Modbus Poll**
2. Меню: `Connection` → `Connect...`
3. Выберите:
   - **Connection:** Modbus TCP/IP
   - **IP Address:** `192.168.1.100`
   - **Port:** `502`
4. Нажмите **OK**
5. Далее: `Setup` → `Read/Write Definition` → укажите:
   - **Function:** `03 - Read Holding Registers`
   - **Address:** `0`
   - **Quantity:** `2`
   - **Slave ID:** `1`

Если всё подключено правильно, вы увидите значения регистров, обновляющиеся в реальном времени.

P.S. гайд с картинками размещен в [`schematics/`](schematics/) файл `guide.pdf`