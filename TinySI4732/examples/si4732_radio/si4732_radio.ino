/*
  Arduino Leonardo, SI4732-A10M-002モジュール, L2034モジュールで動作確認

  ・SI4732-A10M-002モジュール回路図
  　http://7777777777.net/manual/si4732a10m-002.pdf
  
  ・si4732データシート
    FM, AM関係: AN332_programming_guide.pdf
    ssb関係: si4735_SSB_NBFM_programming_guide_20120831.pdf

  ・SSB受信用パッチデータを外部EEPROM(SI4732-A10M-002モジュール)または、Arduino FLASHのいずれから
  　ダウンロードするか選択すること。※デフォルトでは、Arduino FLASHからダウンロード。
    TinySi4732.hで、#include "patch.h"を記載するとFLASHからのダウンロードみとなる。
    記載なしだと外部EEPROMからのダウンロードとなる。
    外部EEPROMからダウンロードする場合は、Si4732_eepromでinitパッチデータを書き込んでおくこと。
    ※fullパッチは未対応。    

  ・ロータリーSWの仕様に応じて、rotaryEncoder()の判定値を変更すること。

  ・初回起動時、バックアップ初期化時、bandTable[]変更時は、バンド切替SWを押下しながら起動させること。

*/
#include "TinySI4732.h"
#include "Lcd.h"
#include <EEPROM.h>

#define RESET_PIN     10    // リセット

#define ENCODER_GND   A0    // ロータリーエンコーダ
#define ENCODER_PIN_A A1
#define ENCODER_PIN_B A2
#define SWA           A3    // バンド切替SW
#define SWB           A4    // 設定SW

// LCD20x4 PINs R/WピンはGND固定
#define LCD_D7    7
#define LCD_D6    6
#define LCD_D5    5
#define LCD_D4    4
#define LCD_E     9
#define LCD_RS    8

#define TICKTIME  2   // 2ms
#define SWON      5   // 5*TICKTIMEでオン

TinySI4732 rx(RESET_PIN);
Lcd lcd(LCD_20x4, LCD_D7, LCD_D6, LCD_D5, LCD_D4, LCD_E, LCD_RS);  // D7~D3, RW, E, RS
char encoderCount;      //
byte swa, swb;          // swa = BAND SELECT SW, swb = FUNCTION SELECT SW
byte band;              // 
byte volume;            // 0:min ~ 63:max
byte funcSelect;        // 0:FREQ, 1:MODE, 2:FILTER, 3:ATT, 4:VOLUME
word startTime;         //
word funcSelectTime;    // FUNCTION SELECT SWの有効時間
byte updataTime;        // 256*TICKTIME毎にRSSIとSNRを更新
word updataEeprom;      // 2048*TICKTIME毎にeepromを更新
const char *selectName[] = {"FREQ", "MODE", "FILTER", "ATT ", "VOLUME", "SEEK"};
const byte funcSelectSize = sizeof(selectName) / sizeof(char *);
tRsqStatus rsqStatus;   // RSSI, SNR

struct tBandTable{
  char name[8];   // バンド名称
  tRadio radio;   // 設定情報
};
// struct tRadio{
//   byte mode;        // 0:FM, 1:AM, 2:LSB, 3:USB
//   word freq;        // 受信周波数 FM:6400~10800MHz, AM:149k~23000kHz, LSB/USB:520k~30000kHz
//   word fmAmAntCap;  // FM:0(auto),1~191, AM:0(auto),1~6143
//   word ssbAntCap;   // LSB/USB:1~6143
//   word minFreq;     // シーク下限周波数
//   word maxFreq;     // シーク上限周波数
//   byte stepFreq;    // 10:FM 100kHz, 1:AM 1kHz, 5:AM 5kHz, 9:AM 9kHz
//   bool stereo;      // true:stereo, false:mono
//   bool agcOn;       // true:AGC=ON ATT=OFF, false AGC=OFF ATT=ON
//   byte agcGain;     // 0~255
//   byte fmAmFilter;  // FM:0:AUTO 1:110k, 2:84k 3:60k 4:40k
//                     // AM:0:6.0k, 1:4.0k, 2:3.0k, 3:2.5kG, 4:2.0k, 5:1.8k, 6:1.0k
//   byte ssbFilter;   // LSB/USB:0:4.0k, 1:3.0k, 2:2.2k, 3:1.2k, 4:1.0k, 5:0.5k
//   int  bfoFreq;     // -16383kHz~16383kHz
// };
tBandTable bandTable[] = {  // 上記のtRadioを参考に設定すること
  {"MW",   {AM,    729, 0, 0,   522,  1710,  9, false, true, 0, 0, 0, 0}},
  {"VHF",  {FM,   8250, 0, 0,  7600, 10800, 10, true, true, 0, 0, 0, 0}},
//  {"1.9M", {LSB,  1800, 0, 1,  1800,  1913,  1, false, true, 0, 0, 0, 0}},
//  {"3.5M", {LSB,  3500, 0, 1,  3500,  3687,  1, false, true, 0, 0, 0, 0}},
//  {"3.8M", {LSB,  3702, 0, 1,  3702,  3805,  1, false, true, 0, 0, 0, 0}},
  {"7M",   {LSB,  7000, 0, 1,  7000,  7200,  1, false, true, 0, 0, 0, 0}},
  {"10M",  {LSB, 10100, 0, 1, 10100, 10150,  1, false, true, 0, 0, 0, 0}},
  {"14M",  {LSB, 14000, 0, 1, 14000, 14350,  1, false, true, 0, 0, 0, 0}},
//  {"18M",  {LSB, 18000, 0, 1, 18000, 18168,  1, false, true, 0, 0, 0, 0}},
  {"49m",  {AM,   5730, 0, 1,  5730,  6295,  5, false, true, 0, 0, 0, 0}},
  {"31m",  {AM,   9250, 0, 1,  9250,  9900,  5, false, true, 0, 0, 0, 0}},
  {"25m",  {AM,  11600, 0, 1, 11600, 12100,  5, false, true, 0, 0, 0, 0}},
  {"19m",  {AM,  15030, 0, 1, 15030, 15800,  5, false, true, 0, 0, 0, 0}},
//  {"16m",  {AM,  17480, 0, 1, 17480, 17900,  5, false, true, 0, 0, 0, 0}},
};
const byte bandTableSize = sizeof(bandTable) / sizeof(tBandTable);

void setup() {
  pinMode(SWA, INPUT_PULLUP);
  pinMode(SWB, INPUT_PULLUP);
  pinMode(ENCODER_GND, OUTPUT);
  digitalWrite(ENCODER_GND, LOW);
  pinMode(ENCODER_PIN_A, INPUT_PULLUP);
  pinMode(ENCODER_PIN_B, INPUT_PULLUP);

  Serial.begin(115200);
  if(!Serial) delay(2000);

  readEeeprom();
  rx.setup();
  rx.setRadio(&bandTable[band].radio);
  rx.setVolume(volume);
  rx.setMute(false);

  tGetRev rev;
  rx.getRev(rev);
  lcd.init();
  lcd.printf("si47%02d radio\n", rev.PN);
  lcd.printf(" ChipRev: %c FW:%c%c\n", rev.CHIPREV, rev.FIRMWARE[0], rev.FIRMWARE[1]);
  lcd.printf(" CompRev:%c%c\n", rev.COMPONET[0], rev.COMPONET[1]);
  lcd.printf(" Patch:%04X", rev.PATCH);
  startTime = millis();
}

void loop() {
  for( ; (word)millis() - startTime < 2000; ) // 初回2000ms,以後はTICKTIME毎に抜ける
    lcd.update(); // LCD表示更新
  startTime += TICKTIME;
  tRadio *p = &bandTable[band].radio;
  
  rotaryEncoder();
  swRead();

  if(swa == SWON){ // バンド切替
    if(++band >= bandTableSize) band = 0;
    p = &bandTable[band].radio;
    rx.setRadio(p);
    funcSelectTime = 0;
    funcSelect = 0;
  }

  if(swb == SWON){ // 設定切替
    if(++funcSelect >= funcSelectSize) // 0:freq, 1:mode(am,usb,lsb), 2:filter, 3:rfGain 4:volume　5:seek
      funcSelect = 1;
    funcSelectTime = 3000 / TICKTIME;
  }

  if(funcSelectTime){
    if(encoderCount)
      funcSelectTime = 3000 / TICKTIME;
    if(--funcSelectTime == 0)
      funcSelect = 0;
  }

  if(encoderCount || funcSelect == 5){
    switch(funcSelect){
      byte gain;
      word filter;
      case 0:         // 周波数変更
        rx.addFreq(p->mode <= AM? encoderCount * p->stepFreq : encoderCount * 100);
        break;
      case 1:        // モード変更
        if(p->mode != FM)
          p->mode = constrain(p->mode + encoderCount, AM, USB);
        rx.setRadio(p);
        break;
      case 2:         // フィルタ変更
        if(p->mode <= AM)
          filter = p->fmAmFilter = constrain(p->fmAmFilter + encoderCount, 0, rx.getFilterSize() - 1);
        else
          filter = p->ssbFilter = constrain(p->ssbFilter + encoderCount, 0, rx.getFilterSize() - 1);
        rx.setFilter(filter);
        break;
      case 3:             // RFゲイン変更
        gain = p->agcGain;
        p->agcGain = constrain(p->agcGain + encoderCount, 0, rx.getAgcGainSize() - 1);
        if(gain + p->agcGain == 0){
          p->agcOn = true;
        }else if(p->agcGain == 1 && p->agcOn){
          p->agcOn = false;
          p->agcGain = 0;
        }
        rx.setAgcGain(p->agcOn, p->agcGain);
        break;
      case 4:             // 音量変更
        volume = constrain(volume + encoderCount, 0, 63);
        rx.setVolume(volume);
        break;
      case 5:             // シーク選局
        if(rx.seekNow(false)){
          funcSelectTime = 3000 / TICKTIME;
          if(encoderCount)
            rx.seekNow(true); // シーク中止設定
        }else{
          if(encoderCount)
            rx.seekStart(encoderCount > 0); // シーク開始設定
        }
        break;
    }
  }
  display();
  if((++updataEeprom & 0x7FF) == 0)
    writeEeprom();
}

void display(){
  if(++updataTime == 1)  // 256*TICKTIME毎に更新
    rx.getRsqStatus(rsqStatus);

  lcd.clear();
  lcd.printf("%s %s %s\n", bandTable[band].name, rx.getLabel(L_FREQ), selectName[funcSelect]);
  lcd.printf("%-3s FL:%-5s ATT:%s\n", rx.getLabel(L_MODE), rx.getLabel(L_FILTER), rx.getLabel(L_AGC));  // FM, AM, LSB, USB
  lcd.printf("VOL:%s\n", rx.getLabel(L_VOLUME));
  lcd.printf("RSSI:%d SNR:%d", rsqStatus.RSSI, rsqStatus.SNR);
}

void rotaryEncoder(){
  static byte swBuf = 0;
  encoderCount = 0;
  byte sw = (digitalRead(ENCODER_PIN_A) << 1) | digitalRead(ENCODER_PIN_B);
  if(sw != (swBuf & 0b11)){ // 変化あり
    swBuf = (swBuf << 2) | sw;
    //Serial.println(swBuf);  // SW仕様確認時はコメントアウト
    if(swBuf == 227){       // 時計回り、SWの仕様により判定値変更
      ++encoderCount;
    }else if(swBuf == 203){ // 反時計回り、SWの仕様により判定値変更
      --encoderCount;
    }
  }
}

void swRead(){
  swa = digitalRead(SWA) == HIGH? 0 : swa < 200? swa + 1 : swa;
  swb = digitalRead(SWB) == HIGH? 0 : swb < 200? swb + 1 : swb;
}

#define CHECKDIGIT  0x5A

void writeEeprom(){
  word addr = 0x0001;
  EEPROM.update(addr++, volume);
  for(byte i = 0; i < bandTableSize; ++i){
    EEPROM.put(addr, bandTable[i].radio);
    addr += sizeof(tRadio);
  }
  EEPROM.update(0x0000, CHECKDIGIT);
}

void readEeeprom(){
  if(digitalRead(SWA) == LOW || EEPROM.read(0x0000) != CHECKDIGIT){
    volume = 60;
    writeEeprom();
  }

  word addr = 0x0001;
  volume = EEPROM.read(addr++);
  for(byte i = 0; i < bandTableSize; ++i){
    EEPROM.get(addr, bandTable[i].radio);
    addr += sizeof(tRadio);
  }
}
