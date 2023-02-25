#pragma once

// LCD MODE
#define LCD_16x1  0
#define SII_16x1  1
#define LCD_16x2  2
#define LCD_20x4  3

class Lcd{
  public:
    Lcd(byte lcdMode, byte d7, byte d6, byte d5, byte d4, byte e, byte rs, byte rw = 0xFF);
    void init();
    void command(byte data, bool rs = false);
    void data(byte ch);
    void update();
    void updateAll();
    void locate(byte p);
    void charactor(byte ch);
    void string(const char *ch);
    void string(const byte *ch);
    void clear();
    void printf(const char *args, ...);

  private:
    byte d7,d6,d5,d4, e, rs, rw;
    byte cp;
    byte buf[20*4];
    byte updateP;
    byte charSize;
    byte lcdMode;
    byte lineSize;

    void nipple(byte data);
};

Lcd::Lcd(byte lcdMode, byte d7, byte d6, byte d5, byte d4, byte e, byte rs, byte rw){
  this->lcdMode = lcdMode;
  this->d7 = d7;
  this->d6 = d6;
  this->d5 = d5;
  this->d4 = d4;
  this->e = e;
  this->rs = rs;
  this->rw = rw;
  switch(lcdMode){
    case LCD_16x1:
    case SII_16x1:
      charSize = 16*1;
      lineSize = 16;
      break;
    case LCD_16x2:
      charSize = 16*2;
      lineSize = 16;
      break;
    case LCD_20x4:
      charSize = 20*4;
      lineSize = 20;
      break;
    default:
      charSize = 16;
      lineSize = 16;
  }
}

void Lcd::update(){
  if(updateP == 0){
    command(0x80 + 0x00);
  }else{
    switch(lcdMode){
      case LCD_16x1:
        break;
      case SII_16x1:
        if(updateP == 8)
          command(0x80 + 0x40);
        break;
      case LCD_16x2:
        if(updateP == 16)
          command(0x80 + 0x40);
        break;
      case LCD_20x4:
        if(updateP == 20)
          command(0x80 + 0x40);
        else if(updateP == 40)
          command(0x80 + 0x14);
        else if(updateP == 60)
          command(0x80 + 0x54);
        break;
    }
  }
  data(buf[updateP]);
  if(++updateP >= charSize)
    updateP = 0;
}

void Lcd::printf(const char *args, ...){  // longを最後に渡すと正しい数値を表示しない
  va_list ap;
  char buff[32+1];
  va_start(ap, args);
  vsprintf(buff, args, ap);
  string(buff);
  va_end(ap);
}

void Lcd::init(){
  pinMode(e, OUTPUT);
  digitalWrite(e, LOW);
  pinMode(d7, OUTPUT);
  pinMode(d6, OUTPUT);
  pinMode(d5, OUTPUT);
  pinMode(d4, OUTPUT);
  pinMode(rs, OUTPUT);
  digitalWrite(rs, LOW);
  if(rw != 0xFF){
    pinMode(rw, OUTPUT);
    digitalWrite(rw, LOW);
  }
  // power on init
  delay(45);
  nipple(0b00110000);         // 8bit
  delayMicroseconds(4100);
  nipple(0b00110000);         // 8bit
  delayMicroseconds(100);
  nipple(0b00110000);         // 8bit
  //
  nipple(0b00100000);         // 4bit
  command(0b00101000, 0);     // ファンクションセット 4bit, 1/16duty, 5*7dotmatrix
  command(0b00001100, 0);     // 表示オン
  command(0b00000001, 0);     // 表示クリア
  delayMicroseconds(1600);
  command(0b00000110, 0);     // エントリーモードセット
  updateP = 0;
  clear();

  // // CGRAM SET
  // command(0b01000000 + 0, 0);  // CGRAM ADDR
  // for(byte i=0; i<8*4; ++i){
  //   data(pgm_read_byte(cgramData + i));
  // }
  // command(0b10000000, 0);  // DDRAM ADDR

}

void Lcd::command(byte data, bool rs){
  digitalWrite(this->rs, rs);
  nipple(data);
  delayMicroseconds(1);
  nipple(data<<4);
  delayMicroseconds(40);
}

void Lcd::data(byte ch){
  command(ch, 1);
}

void Lcd::updateAll(){
  for(byte i=0; i<charSize; ++i)
    update();
}

void Lcd::locate(byte p){
  if(p >= charSize) return;
  cp = p;
}

void Lcd::charactor(byte ch){
  if(ch == '\n'){
    if(cp >= charSize - lineSize){
      for(byte i=0; i<charSize-lineSize; ++i)
        buf[i] = buf[i+lineSize];
      cp = charSize-lineSize;
      for(byte i=charSize-lineSize; i<charSize; ++i)
        buf[i] = ' ';
    }else{
      cp = (cp / lineSize + (cp % lineSize? 1 : 0)) * lineSize;
    }
  }else if(cp < charSize){
    buf[cp++] = ch;
  }else{
    for(byte i=0; i<charSize-1; ++i){
      buf[i] = buf[i+1];
    }
    buf[charSize-1] = ch;
  }
}

void Lcd::string(const char *ch){
  string((byte *)ch);
}

void Lcd::string(const byte *ch){
  while(*ch != '\0'){
    charactor(*ch);
    ++ch;
  }
}

void Lcd::clear(){
  for(byte i=0; i<charSize; ++i)
    buf[i] = ' ';
  cp = 0;
}

void Lcd::nipple(byte data){
  digitalWrite(e, HIGH);
  digitalWrite(d7, data & 0x80? HIGH : LOW);
  digitalWrite(d6, data & 0x40? HIGH : LOW);
  digitalWrite(d5, data & 0x20? HIGH : LOW);
  digitalWrite(d4, data & 0x10? HIGH : LOW);
  digitalWrite(e, LOW);
}
