#include "ModbusTCP_RU.h"

// Инициализация сервера и клиента для Arduino
EthernetServer MbServer(MB_PORT);
EthernetClient MbmClient;

// #define DEBUG // Раскомментируйте для отладки

// Константы для таймаутов
#define MB_PACKET_TIMEOUT 50  // Таймаут ожидания пакета (мс)
#define MB_READ_TIMEOUT 1000  // Таймаут чтения данных (мс)

ModbusTCP_RU::ModbusTCP_RU()
{
}

//****************** Отправка данных для ModBusMaster ****************
void ModbusTCP_RU::Req(MB_FC FC, word Ref, word Count, word Pos)
{
  MbmFC = FC;
  byte ServerIp[] = {192,168,200,163}; // IP-адрес ведомого устройства
  
  // Формирование заголовка Modbus TCP
  MbmByteArray[0] = 0;  // ID высокий байт
  MbmByteArray[1] = 1;  // ID низкий байт
  MbmByteArray[2] = 0;  // Протокол высокий байт
  MbmByteArray[3] = 0;  // Протокол низкий байт
  MbmByteArray[5] = 6;  // Длина низкий байт
  MbmByteArray[4] = 0;  // Длина высокий байт
  MbmByteArray[6] = 1;  // Идентификатор устройства
  MbmByteArray[7] = FC; // Код функции
  
  // Адрес первого элемента
  MbmByteArray[8] = highByte(Ref);
  MbmByteArray[9] = lowByte(Ref);

  //****************** Чтение катушек (1) и дискретных входов (2) **********************
  if(FC == MB_FC_READ_COILS || FC == MB_FC_READ_DISCRETE_INPUT) {
    if (Count < 1) {Count = 1;}
    if (Count > MB_MAX_COILS) {Count = MB_MAX_COILS;}
    MbmByteArray[10] = highByte(Count);
    MbmByteArray[11] = lowByte(Count);
  }

  //****************** Чтение регистров (3) и регистров ввода (4) ******************
  if(FC == MB_FC_READ_REGISTERS || FC == MB_FC_READ_INPUT_REGISTER) {
    if (Count < 1) {Count = 1;}
    if (Count > MB_MAX_REGISTERS) {Count = MB_MAX_REGISTERS;}
    MbmByteArray[10] = highByte(Count);
    MbmByteArray[11] = lowByte(Count);
  }

  //****************** Запись катушки (5) **********************
  if(MbmFC == MB_FC_WRITE_COIL) {
    if (GetBit(Pos)) {MbmByteArray[10] = 0xFF;} else {MbmByteArray[10] = 0;} // 0xFF катушка включена, 0x00 выключена
    MbmByteArray[11] = 0; // всегда ноль
  }

  //****************** Запись регистра (6) ******************
  if(MbmFC == MB_FC_WRITE_REGISTER) {
    MbmByteArray[10] = highByte(MbData[Pos]);
    MbmByteArray[11] = lowByte(MbData[Pos]);
  }

  //****************** Запись нескольких катушек (15) **********************
  if(MbmFC == MB_FC_WRITE_MULTIPLE_COILS) {
    if (Count < 1) {Count = 1;}
    if (Count > MB_MAX_MULTIPLE_COILS) {Count = MB_MAX_MULTIPLE_COILS;}
    MbmByteArray[10] = highByte(Count);
    MbmByteArray[11] = lowByte(Count);
    MbmByteArray[12] = (Count + 7) / 8;
    MbmByteArray[4] = highByte(MbmByteArray[12] + 7); // Длина высокий байт
    MbmByteArray[5] = lowByte(MbmByteArray[12] + 7);  // Длина низкий байт
    
    // Упаковка битов в байты
    for (int i=0; i<Count; i++) {
      bitWrite(MbmByteArray[13+(i/8)], i-((i/8)*8), GetBit(Pos+i));
    }
  }

  //****************** Запись нескольких регистров (16) ******************
  if(MbmFC == MB_FC_WRITE_MULTIPLE_REGISTERS) {
    if (Count < 1) {Count = 1;}
    if (Count > MB_MAX_MULTIPLE_REGISTERS) {Count = MB_MAX_MULTIPLE_REGISTERS;}
    MbmByteArray[10] = highByte(Count);
    MbmByteArray[11] = lowByte(Count);
    MbmByteArray[12] = (Count*2);
    MbmByteArray[4] = highByte(MbmByteArray[12] + 7); // Длина высокий байт
    MbmByteArray[5] = lowByte(MbmByteArray[12] + 7);  // Длина низкий байт
    
    // Запись данных регистров
    for (int i=0; i<Count; i++) {
      MbmByteArray[(i*2)+13] = highByte(MbData[Pos + i]);
      MbmByteArray[(i*2)+14] = lowByte(MbData[Pos + i]);
    }
  }

  //****************** Отправка данных ******************
  if (MbmClient.connect(ServerIp, MB_PORT)) {
    #ifdef DEBUG
      Serial.println("Подключено к ведомому устройству Modbus");
      Serial.print("Запрос ведущего: ");
      for(int i=0; i<MbmByteArray[5]+6; i++) {
        if(MbmByteArray[i] < 16) {Serial.print("0");}
        Serial.print(MbmByteArray[i], HEX);
        if (i != MbmByteArray[5]+5) {Serial.print(".");} else {Serial.println();}
      }
    #endif    
    
    // Отправка данных
    for(int i=0; i<MbmByteArray[5]+6; i++) {
      MbmClient.write(MbmByteArray[i]);
    }
    
    MbmCounter = 0;
    MbmByteArray[7] = 0;
    MbmPos = Pos;
    MbmBitCount = Count;
  } else {
    #ifdef DEBUG
      Serial.println("Ошибка подключения к ведомому устройству");
    #endif    
    MbmClient.stop();
  }
}

//****************** Получение данных для ModBusMaster ****************
void ModbusTCP_RU::MbmRun()
{
  // Чтение из сокета
  while (MbmClient.available()) {
    MbmByteArray[MbmCounter] = MbmClient.read();
    if (MbmCounter > 4) {
      if (MbmCounter == MbmByteArray[5] + 5) { // Получен полный ответ
        MbmClient.stop();
        MbmProcess();
        #ifdef DEBUG
          Serial.println("Прием завершен");
        #endif    
      }
    }
    MbmCounter++;
  }
}

void ModbusTCP_RU::MbmProcess()
{
  MbmFC = SetFC(int(MbmByteArray[7]));
  #ifdef DEBUG
    Serial.print("Ответ ведомого: ");
    for (int i=0; i<MbmByteArray[5]+6; i++) {
      if(MbmByteArray[i] < 16) {Serial.print("0");}
      Serial.print(MbmByteArray[i], HEX);
      if (i != MbmByteArray[5]+5) {Serial.print(".");} else {Serial.println();}
    }
  #endif    

  //****************** Обработка чтения катушек (1) и дискретных входов (2) **********************
  if(MbmFC == MB_FC_READ_COILS || MbmFC == MB_FC_READ_DISCRETE_INPUT) {
    word Count = MbmByteArray[8] * 8;
    if (MbmBitCount < Count) {
      Count = MbmBitCount;
    }
    for (int i=0; i<Count; i++) {
      if (i + MbmPos < MB_DATA_LEN * 16) {
        SetBit(i + MbmPos, bitRead(MbmByteArray[(i/8)+9], i-((i/8)*8)));
      }
    }
  }

  //****************** Обработка чтения регистров (3) и регистров ввода (4) ******************
  if(MbmFC == MB_FC_READ_REGISTERS || MbmFC == MB_FC_READ_INPUT_REGISTER) {
    word Pos = MbmPos;
    for (int i=0; i<MbmByteArray[8]; i=i+2) {
      if (Pos < MB_DATA_LEN) {
        MbData[Pos] = (MbmByteArray[i+9] * 0x100) + MbmByteArray[i+1+9];
        Pos++;
      }
    }
  }
}

//****************** Получение данных для ModBusSlave ****************
void ModbusTCP_RU::MbsRun()    
{  
  // Чтение из сокета
  EthernetClient client = MbServer.available();
  if(client.available()) {
    unsigned long startTime = millis();
    int i = 0;
    
    // Читаем данные с таймаутом
    while(client.available() && i < MB_BUFFER_SIZE) {
      MbsByteArray[i] = client.read();
      i++;
      
      // Если прошло больше MB_PACKET_TIMEOUT мс и нет новых данных, считаем пакет полученным
      if (!client.available() && (millis() - startTime) > MB_PACKET_TIMEOUT) {
        break;
      }
    }
    
    if (i > 0) {
      MbsFC = SetFC(MbsByteArray[7]);  // Байт 7 запроса содержит код функции
    }
  }

  int Start, WordDataLength, ByteDataLength, CoilDataLength, MessageLength;

  //****************** Обработка чтения катушек (1 & 2) **********************
  if(MbsFC == MB_FC_READ_COILS || MbsFC == MB_FC_READ_DISCRETE_INPUT) {
    Start = word(MbsByteArray[8], MbsByteArray[9]);
    CoilDataLength = word(MbsByteArray[10], MbsByteArray[11]);
    ByteDataLength = CoilDataLength / 8;
    if(ByteDataLength * 8 < CoilDataLength) ByteDataLength++;      
    CoilDataLength = ByteDataLength * 8;
    MbsByteArray[5] = ByteDataLength + 3; // Количество байтов после этого
    MbsByteArray[8] = ByteDataLength;     // Количество байтов данных
    
    // Упаковка битов в байты
    for(int i = 0; i < ByteDataLength; i++) {
      MbsByteArray[9 + i] = 0; // Обнуление неиспользуемых битов
      for(int j = 0; j < 8; j++) {
        bitWrite(MbsByteArray[9 + i], j, GetBit(Start + i * 8 + j));
      }
    }
    MessageLength = ByteDataLength + 9;
    client.write(MbsByteArray, MessageLength);
    MbsFC = MB_FC_NONE;
  }

  //****************** Обработка чтения регистров (3 & 4) ******************
  if(MbsFC == MB_FC_READ_REGISTERS || MbsFC == MB_FC_READ_INPUT_REGISTER) {
    Start = word(MbsByteArray[8], MbsByteArray[9]);
    WordDataLength = word(MbsByteArray[10], MbsByteArray[11]);
    ByteDataLength = WordDataLength * 2;
    MbsByteArray[5] = ByteDataLength + 3; // Количество байтов после этого
    MbsByteArray[8] = ByteDataLength;     // Количество байтов данных
    
    // Запись данных регистров
    for(int i = 0; i < WordDataLength; i++) {
      MbsByteArray[9 + i * 2] = highByte(MbData[Start + i]);
      MbsByteArray[10 + i * 2] = lowByte(MbData[Start + i]);
    }
    MessageLength = ByteDataLength + 9;
    client.write(MbsByteArray, MessageLength);
    MbsFC = MB_FC_NONE;
  }

  //****************** Обработка записи катушки (5) **********************
  if(MbsFC == MB_FC_WRITE_COIL) {
    Start = word(MbsByteArray[8], MbsByteArray[9]);
    if (word(MbsByteArray[10], MbsByteArray[11]) == 0xFF00) {SetBit(Start, true);}
    if (word(MbsByteArray[10], MbsByteArray[11]) == 0x0000) {SetBit(Start, false);}
    MbsByteArray[5] = 2; // Количество байтов после этого
    MessageLength = 8;
    client.write(MbsByteArray, MessageLength);
    MbsFC = MB_FC_NONE;
  } 

  //****************** Обработка записи регистра (6) ******************
  if(MbsFC == MB_FC_WRITE_REGISTER) {
    Start = word(MbsByteArray[8], MbsByteArray[9]);
    MbData[Start] = word(MbsByteArray[10], MbsByteArray[11]);
    MbsByteArray[5] = 6; // Количество байтов после этого
    MessageLength = 12;
    client.write(MbsByteArray, MessageLength);
    MbsFC = MB_FC_NONE;
  }

  //****************** Обработка записи нескольких катушек (15) **********************
  if(MbsFC == MB_FC_WRITE_MULTIPLE_COILS) {
    Start = word(MbsByteArray[8], MbsByteArray[9]);
    CoilDataLength = word(MbsByteArray[10], MbsByteArray[11]);
    MbsByteArray[5] = 6;
    for(int i = 0; i < CoilDataLength; i++) {
      SetBit(Start + i, bitRead(MbsByteArray[13 + (i/8)], i-((i/8)*8)));
    }
    MessageLength = 12;
    client.write(MbsByteArray, MessageLength);
    MbsFC = MB_FC_NONE;
  }  

  //****************** Обработка записи нескольких регистров (16) ******************
  if(MbsFC == MB_FC_WRITE_MULTIPLE_REGISTERS) {
    Start = word(MbsByteArray[8], MbsByteArray[9]);
    WordDataLength = word(MbsByteArray[10], MbsByteArray[11]);
    ByteDataLength = WordDataLength * 2;
    MbsByteArray[5] = 6;
    for(int i = 0; i < WordDataLength; i++) {
      MbData[Start + i] = word(MbsByteArray[13 + i * 2], MbsByteArray[14 + i * 2]);
    }
    MessageLength = 12;
    client.write(MbsByteArray, MessageLength);
    MbsFC = MB_FC_NONE;
  }
}

//****************** Преобразование кода функции ******************
MB_FC ModbusTCP_RU::SetFC(int fc)  
{
  MB_FC FC;
  FC = MB_FC_NONE;
  if(fc == 1) FC = MB_FC_READ_COILS;
  if(fc == 2) FC = MB_FC_READ_DISCRETE_INPUT;
  if(fc == 3) FC = MB_FC_READ_REGISTERS;
  if(fc == 4) FC = MB_FC_READ_INPUT_REGISTER;
  if(fc == 5) FC = MB_FC_WRITE_COIL;
  if(fc == 6) FC = MB_FC_WRITE_REGISTER;
  if(fc == 15) FC = MB_FC_WRITE_MULTIPLE_COILS;
  if(fc == 16) FC = MB_FC_WRITE_MULTIPLE_REGISTERS;
  return FC;
}

//****************** Получение длины массива данных ******************
word ModbusTCP_RU::GetDataLen()
{
  return MB_DATA_LEN;
}

//****************** Получение значения бита ******************
boolean ModbusTCP_RU::GetBit(word Number)
{
  int ArrayPos = Number / 16;
  int BitPos = Number - ArrayPos * 16;
  boolean Tmp = bitRead(MbData[ArrayPos], BitPos);
  return Tmp;
}

//****************** Установка значения бита ******************
boolean ModbusTCP_RU::SetBit(word Number, boolean Data)  
{
  int ArrayPos = Number / 16;
  int BitPos = Number - ArrayPos * 16;
  boolean Overrun = ArrayPos > MB_DATA_LEN * 16; // Проверка переполнения
  if (!Overrun) {                 
    bitWrite(MbData[ArrayPos], BitPos, Data);
  } 
  return Overrun;
}