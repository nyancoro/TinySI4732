/*
  eepromにpatch romの内容を書き込む
  patchの容量が大きいので、patch毎に書込みを行う。
  #include "init.h"によって、initパッチを書込みます。
  #include "full.h"によって、fullパッチを書込みます。
  
*/
#include <Wire.h>
#include "init.h" // patch rom init version
//#include "full.h" // patch rom full version

const byte EEADR = 0b1010000;
#define HEADERSIZE  32
union EepromHeader {
  struct{
    byte reserved[8];  // Not used
    byte status[8];    // Note used
    byte patch_id[14]; // Patch name
    word patch_size;  // Patch size (in bytes)
  } refined;
  byte raw[HEADERSIZE];
};

void setup() {
  Serial.begin(115200);
  while(!Serial);
  Wire.begin();

  Serial.println(TITLE);
  int romSize = sizeof(romData) / sizeof(byte);
  print("Rom Size=%d\n", romSize);

  EepromHeader eepromHeader;

  headerInit(eepromHeader, romSize);
  eepromWrite(START_ADDR, romSize, eepromHeader);
  eepromVerify(START_ADDR, romSize, eepromHeader);
}

void loop() {
}

void headerInit(EepromHeader &eepromHeader, int romSize){
  for(byte i = 0; i < HEADERSIZE; ++i)
    eepromHeader.raw[i] = 0;
  strcpy((char *)&eepromHeader.refined.patch_id, patchId);
  eepromHeader.refined.patch_size = romSize;
}
void eepromWrite(word startAddr, word size, const EepromHeader &eepromHeader){
  byte data = 0;

  Serial.println("ROM Write");
  // header write
  print("%04X ", startAddr);
  for(byte i = 0; i < HEADERSIZE; ++i){
    Wire.beginTransmission(EEADR);
    Wire.write((startAddr + i) >> 8);
    Wire.write(startAddr + i);
    data = eepromHeader.raw[i];
    Wire.write(data);
    Wire.endTransmission();
    delay(5);
    print(" %02X", data);
  }
  Serial.println();

  // patch data write
  for(word i = 0; i < size; i += 16){
    word romAddr = startAddr + i + HEADERSIZE;
    print("%04X ", romAddr);
    Wire.beginTransmission(EEADR);
    Wire.write(romAddr >> 8);
    Wire.write(romAddr);
    for(word j = 0; j < 16; ++j){
      if((i + j) < size)
        data = pgm_read_byte(romData + i + j);
      else
        data = 0;
      Wire.write(data);
      print(" %02X", data);     
    }
    Wire.endTransmission();
    delay(5);
    Serial.println();
  }
}

bool eepromVerify(word startAddr, word size, const EepromHeader &eepromHeader){
  byte data = 0;
  bool verifyOk = true;
  
  Serial.println("ROM Verify");
  // header verify
  print("%04X ", startAddr);
  Wire.beginTransmission(EEADR);
  Wire.write(startAddr >> 8);
  Wire.write(startAddr);
  Wire.endTransmission();
  for(byte i = 0; i < HEADERSIZE; ++i){
    Wire.requestFrom(EEADR, (byte)1);
    while(Wire.available() <= 0);
    byte readData = Wire.read();
    if(readData != eepromHeader.raw[i])
      verifyOk = false;
    print(" %02X", readData);
  }
  Serial.println();

  // patch data verify
  for(word i = 0; i < size; i += 16){
    word romAddr = startAddr + i + HEADERSIZE;
    print("%04X ", romAddr);
    Wire.beginTransmission(EEADR);
    Wire.write(romAddr >> 8);
    Wire.write(romAddr);
    Wire.endTransmission();
    Wire.requestFrom(EEADR, (byte)16);
    for(word j = 0; j < 16; ++j){
      while(Wire.available() <= 0);
      byte readData = Wire.read();
      if((i + j) < size)
        data = pgm_read_byte(romData + i + j);
        if(readData != data)
          verifyOk = false;
      else
        data = 0;
      print(" %02X", readData); 
    }
    Serial.println();
  }
  if(verifyOk)
    Serial.println("Verify ok");
  else
    Serial.println("Verify ng");
  return verifyOk;
}

void print(const char *args, ...){  // longを最後に渡すと正しい数値を表示しない
  va_list ap;
  char buf[32+1];
  va_start(ap, args);
  vsprintf(buf, args, ap);
  Serial.print(buf);
  va_end(ap);
}
