/*
  シリアル通信でSI4732-A10M-002モジュールをコマンド制御する

  Arduino Leonardo, SI4732-A10M-002モジュールで動作確認

  ・SI4732-A10M-002モジュール回路図
  　http://7777777777.net/manual/si4732a10m-002.pdf
  
  ・si4732データシート
    FM, AM関係: AN332_programming_guide.pdf
    ssb関係: si4735_SSB_NBFM_programming_guide_20120831.pdf

  ・SSB受信用パッチデータを外部EEPROM(SI4732-A10M-002モジュール)または、Arduino FLASHのいずれから
  　ダウンロードするか選択すること。※デフォルトでは、Arduino FLASHからダウンロード。
    TinySi4732.hで、#include "patch.h"を記載するとFLASHからのダウンロードとなる。記載なしだと
    外部EEPROMからのダウンロードとなる。
    外部EEPROMからダウンロードする場合は、Si4732_eepromでinitパッチデータを書き込んでおくこと。
    ※fullパッチは未対応。    
  ・初回起動時、バックアップ初期化時、bandTable[]変更時は、eコマンドを実行して再起動すること。

  ・command一覧
    f  n  現周波数に加算する。UNIT FM:0.01MHz, AM SSB:1kHz
    F  n  周波数をnで指定する。UNIT FM:0.01MHz, AM:1kHz, SSB:1Hz
    a     自動周波数上昇
    A     自動周波数下降
    b  n  バンド切替 0 ～ バンド数 -1
    m  n  受信モード切替 0:FM, 1:AM, 2:LSB, 3:USB
    l  n  フィルタ切替 0 ～ フィルタ数 -1
    a  n  アッテネータ切替 マイナス値:AGC ON, 0～ATT数 -1:ATT設定値
    v  n  音量をnで指定する。0(min) ～ 63(max)
    s     seek up
    S     seek down
    e     eeprom reset。再起動後有効になる。
    w     現在の状態をArduino内蔵EEPROMに書込む

*/
#include "TinySI4732.h"
#include <EEPROM.h>

#define RESET_PIN     10    // リセット

TinySI4732 rx(RESET_PIN);
byte band;              // 
byte volume;            // 0:min ~ 63:max
word updataEeprom;      // 2048*TICKTIME毎にeepromを更新

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
  Serial.begin(115200);
  while(!Serial);

  readEeeprom();
  rx.setup();
  rx.setRadio(&bandTable[band].radio);
  rx.setVolume(volume);
  rx.setMute(false);

  tGetRev rev;
  rx.getRev(rev);
  xprintf("DSP RADIO\nChip:si47%02d\n", rev.PN);
  xprintf("ChipRev: %c FW:%c%c\n", rev.CHIPREV, rev.FIRMWARE[0], rev.FIRMWARE[1]);
  xprintf("CompRev:%c%c\n", rev.COMPONET[0], rev.COMPONET[1]);
  xprintf("Patch:%04X\n", rev.PATCH);

  display();
}

void loop() {
  char lineBuf[64];

  getLine(lineBuf);
  char *command = strtok(lineBuf, " ");
  int parameter = atoi(strtok(nullptr, " "));
  xprintf("%s %d\n", command, parameter);

  tRadio *p = &bandTable[band].radio;
  
  if(!strcmp(command, "f")){  // 周波数を加算
    rx.addFreq(parameter);
  
  }else if(!strcmp(command, "F")){  // 周波数設定
    rx.setFreq(parameter);

  }else if(!strcmp(command, "a")){  // 自動周波数上昇
    while(true){
      if(Serial.available() > 0)
        break;
      if(p->freq == p->maxFreq){
        Serial.println("\nmax freq!");
        rx.setFreq(p->minFreq);
      }else{
        rx.addFreq(p->mode < LSB? p->stepFreq : 100);
      }
      xprintf("%sHz\n", rx.getLabel(L_FREQ));
      delay(200);
    }

  }else if(!strcmp(command, "A")){  // 自動周波数下降
    while(true){
      if(Serial.available() > 0)
        break;
      if(p->freq == p->minFreq){
        Serial.println("\nmin freq!");
        rx.setFreq(p->maxFreq);
      }else{
        rx.addFreq(p->mode < LSB? -p->stepFreq : -100);
      }
      xprintf("%sHz\n", rx.getLabel(L_FREQ));
      delay(1000);
    }

  }else if(!strcmp(command, "b")){  // バンド切替
    band = constrain(parameter, 0, bandTableSize - 1);
    p = &bandTable[band].radio;
    rx.setRadio(p);

  }else if(!strcmp(command, "m")){  // 受信モードの切替
    if(p->mode != FM){
      p->mode = constrain(parameter, AM, USB);
      rx.setRadio(p);
    }
  
  }else if(!strcmp(command, "l")){  // フィルタの切替
    word filter;
    if(p->mode <= AM)
      filter = p->fmAmFilter = constrain(parameter, 0, rx.getFilterSize() - 1);
    else
      filter = p->ssbFilter = constrain(parameter, 0, rx.getFilterSize() - 1);
    rx.setFilter(filter);
  
  }else if(!strcmp(command, "a")){  // アッテネータの切替
    if(parameter < 0){  // AGC ON
      p->agcOn = true;
      p->agcGain = 0;
    }else{  // att on
      p->agcGain = constrain(parameter, 0, rx.getAgcGainSize() - 1);
      p->agcOn = false;
    }
    rx.setAgcGain(p->agcOn, p->agcGain);
  
  }else if(!strcmp(command, "v")){  // 音量の設定
    volume = constrain(parameter, 0, 63);
    rx.setVolume(volume);
  
  }else if(!strcmp(command, "s")){  // seek up
    rx.seekStart(true); // seek up
    while(rx.seekNow(false)){
      if(Serial.available() > 0)
        rx.seekNow(true); // seek stop
      delay(150);
      xprintf("%sHz\n", rx.getLabel(L_FREQ));
    }
  
  }else if(!strcmp(command, "S")){  // seek down
    rx.seekStart(false); // seek down
    while(rx.seekNow(false)){
      if(Serial.available() > 0)
        rx.seekNow(true); // seek stop
      delay(150);
      xprintf("%sHz\n", rx.getLabel(L_FREQ));
    }
  
  }else if(!strcmp(command, "e")){  // eeprom reset
    EEPROM.update(0x0000, 0);
    Serial.println("please restart!");
  
  }else if(!strcmp(command, "w")){  // write parameter
    writeEeprom();
    Serial.println("EEPROM parameter write!");
  
  }else{
    if(command != nullptr)
      Serial.println("undefined command!");
  }

  display();
  if((++updataEeprom & 0x7FF) == 0)
    writeEeprom();
}

void getLine(char *lineBuf){  // シリアル文字列の入力
  byte p = 0;
  do{
    while(Serial.available() > 0){
      char ch = Serial.read();
      if(ch == '\r' || ch == '\n') ch = '\0';
      lineBuf[p++] = ch;
      delay(1);
    }
  }while(p == 0);
  lineBuf[p] = '\0';
}

void display(){
  tRsqStatus rsqStatus;   // RSSI, SNR
  rx.getRsqStatus(rsqStatus);

  xprintf("%s %s ", bandTable[band].name, rx.getLabel(L_FREQ));
  xprintf("%-3s FL:%-5s ATT:%s ", rx.getLabel(L_MODE), rx.getLabel(L_FILTER), rx.getLabel(L_AGC));  // FM, AM, LSB, USB
  xprintf("VOL:%s ", rx.getLabel(L_VOLUME));
  xprintf("RSSI:%d SNR:%d\n\n", rsqStatus.RSSI, rsqStatus.SNR);
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
  if(EEPROM.read(0x0000) != CHECKDIGIT){
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

void xprintf(const char *args, ...){  // longを最後に渡すと正しい数値を表示しない
  va_list ap;
  char buff[32+1];
  va_start(ap, args);
  vsprintf(buff, args, ap);
  Serial.print(buff);
  va_end(ap);
}
