#include <Arduino.h>
#include "TinySI4732.h"
#include <Wire.h>
const int SI4732_ADDR = 0x11;  // si4732
const int EEPROM_ADDR = 0x50;  // Extern eeprom

TinySI4732::TinySI4732(byte RESET_PIN) {
  this->RESET_PIN = RESET_PIN;
  for(byte i = 0; i < LABEL_SIZE; ++i)
    radioLabel[i][0] = '\0';
}

void TinySI4732::reset() {
  digitalWrite(RESET_PIN, LOW);
  delay(10);
  digitalWrite(RESET_PIN, HIGH);
  delay(10);
}

void TinySI4732::setup() {
  Wire.begin();
  Wire.setClock(400000);
  pinMode(RESET_PIN, OUTPUT);
  reset();
}

#ifdef DEBUG
void dbOut(byte data, bool wt = true) {
  if (!wt) Serial.print("\t");
  if (data < 16) Serial.print("0");
  Serial.println(data, HEX);
}
#endif
/*
  STATUS 戻り値
  7 CTS 送信可。
    0 = 次のコマンドを送信する前に待機します。
    1 = クリアして次のコマンドを送信します。
  6 ERR エラー。
    0 = エラーなし
    1 = エラー
  5:3 予約済み 値は異なる場合があります。
  2 RDSINT RDS 割り込み。
    0 = RDS 割り込みはトリガーされていません。
    1 = RDS 割り込みがトリガーされました。
  1 ASQINT 信号品質割り込み。
    0 = 信号品質測定はトリガーされていません。
    1 = 信号品質測定が開始されました。
  0 STCINT シーク/チューン完了割り込み。
    0 = Tune Complete はトリガーされていません。
    1 = チューニング完了がトリガーされました。    
*/
template<typename T1, typename T2>
byte TinySI4732::commandOut(const T1 &cmd, T2 &response) {  // コマンド出力
  commandOut(cmd);

  byte *resAry = (byte *)&response;
  Wire.requestFrom(SI4732_ADDR, sizeof(T2));
  for (byte i = 0; i < sizeof(T2); ++i) {
    resAry[i] = Wire.read();
    #ifdef DEBUG
    dbOut(resAry[i], false);
    #endif
  }
  return resAry[0];  // STATUS
}

template<typename T1>
byte TinySI4732::commandOut(const T1 &cmd) {  // コマンド出力
  const byte *cmdAry = (const byte *)&cmd;
  Wire.beginTransmission(SI4732_ADDR);
  Wire.write(cmdAry, sizeof(T1));
#ifdef DEBUG
  for (byte i = 0; i < sizeof(T1); ++i)
    dbOut(cmdAry[i]);
#endif

  Wire.endTransmission();
  if (cmdAry[0] == POWER_UP)
    delay(110);  // tCTS
  else
    delayMicroseconds(300);  // tCTS

  Wire.requestFrom(SI4732_ADDR, 1);
  byte status = Wire.read();
  #ifdef DEBUG
  dbOut(status, false);
  #endif
  return status;  // STATUS
}

byte TinySI4732::setProperty(word property, word data) {
  byte cmd[] = {
    SET_PROPERTY,
    0,
    highByte(property),
    lowByte(property),
    highByte(data),
    lowByte(data),
  };
  byte status = commandOut(cmd);
  delay(10);  // tCOMP
  return status;
}

word TinySI4732::getProperty(word property) {
  byte cmd[] = {
    GET_PROPERTY,
    0,
    highByte(property),
    lowByte(property),
  };
  byte res[4];
  commandOut(cmd, res);
  return (res[2] << 8) | res[3];
}

byte TinySI4732::powerUp(byte func) {
  byte cmd[] = {
    POWER_UP,
    (byte)(0b00010000 | func),
    0b00000101,
  };
  return commandOut(cmd);
}

byte TinySI4732::powerDown() {
  return commandOut((const byte[]){ POWER_DOWN });
}

byte TinySI4732::getRev(tGetRev &rev) {
  byte status = commandOut((const byte[]){GET_REV}, rev);
  rev.PATCH = (rev.PATCH << 8) | (rev.PATCH >> 8);
  return status;
}

void TinySI4732::setRadio(tRadio *radio) {
  rx = radio;
  byte pastMode = mode;
  mode = rx->mode = rx->mode > USB ? FM : rx->mode;
  strcpy(radioLabel[L_MODE], (const char *[]){ "FM", "AM", "LSB", "USB" }[mode]);
  if (mode == FM) {
    powerDown();
    powerUp(0b00010000);  // FM
    setProperty(FM_SEEK_BAND_TOP, rx->maxFreq);
    setProperty(FM_SEEK_BAND_BOTTOM, rx->minFreq);
    setProperty(FM_SEEK_FREQ_SPACING, rx->stepFreq);
    setProperty(FM_DEEMPHASIS, 1);  // 50us
    setStereo(rx->stereo);
    setFreq(rx->freq, rx->fmAmAntCap);
    setFilter(rx->fmAmFilter);
  } else if (mode == AM) {
    powerDown();
    powerUp(0b00010001);  // AM
    setProperty(AM_SEEK_BAND_TOP, rx->maxFreq);
    setProperty(AM_SEEK_BAND_BOTTOM, rx->minFreq);
    setProperty(AM_SEEK_FREQ_SPACING, rx->stepFreq);
    setFreq(rx->freq, rx->fmAmAntCap);
    setFilter(rx->fmAmFilter);
  } else {
    if (pastMode < LSB) {  // FM or AM
      powerDown();
      #ifdef FLASHROMPATCH
      patchFlashRomLoad();  // SSB patch download
      #else
      patchExtEepRomLoad(0x0000);  // SSB patch download 0x4000
      #endif
    }
    setFreq(rx->freq, rx->ssbAntCap);
    setBfoFreq(rx->bfoFreq);
    setFilter(rx->ssbFilter);
  }
  setAgcGain(rx->agcOn, rx->agcGain);
  //updateRsqStatus();
  setVolume(volume);
  setMute(mute);
  seek = false;
}

byte TinySI4732::setFreq(word freq) {
  return setFreq(freq, mode <= AM ? rx->fmAmAntCap : rx->ssbAntCap);
}

byte TinySI4732::setFreq(word freq, word antCap) {
  byte state;
  if (mode == FM) {
    freq = constrain(freq, 6400, 10800);
    antCap = constrain((int)antCap, 0, 191);
    byte fmCmd[] = {
      FM_TUNE_FREQ,     0,
      highByte(freq),   lowByte(freq),
      (byte)antCap,
    };
    state = commandOut(fmCmd);
    rx->fmAmAntCap = antCap;
    sprintf(radioLabel[L_FREQ], "%d.%0dM", freq / 100, freq % 100 / 10);
  } else if (mode == AM) {
    freq = constrain(freq, 149, 23000);
    antCap = constrain((int)antCap, 0, 6143);
    byte amCmd[] = {
      AM_TUNE_FREQ,     0,
      highByte(freq),   lowByte(freq),
      highByte(antCap), lowByte(antCap),
    };
    state = commandOut(amCmd);
    rx->fmAmAntCap = antCap;
    sprintf(radioLabel[L_FREQ], "%d.0k", freq);
  } else {  // SSB
    freq = constrain(freq, 520, 30000);
    if (freq < 2300)
      antCap = constrain((int)antCap, 0, 6143);
    else
      antCap = constrain((int)antCap, 1, 6143);
    byte ssbCmd[] = {
      SSB_TUNE_FREQ,
      (byte)(mode == USB ? 0b10000000 : 0b01000000),
      highByte(freq),   lowByte(freq),
      highByte(antCap), lowByte(antCap),
    };
    state = commandOut(ssbCmd);
    rx->ssbAntCap = antCap;
    sprintf(radioLabel[L_FREQ], "%d.%01dk", freq, rx->bfoFreq / 100);
  }
  rx->freq = freq;
  return state;
}

byte TinySI4732::setBfoFreq(int bfoFreq) {
  rx->bfoFreq = constrain(bfoFreq, -16383, 16383);
  sprintf(radioLabel[L_FREQ], "%d.%01dk", rx->freq, rx->bfoFreq / 100);
  return setProperty(SSB_BFO, bfoFreq);
}

void TinySI4732::addFreq(int addFreq){  // UNIT FM:0.01MHz, AM:1kHz, SSB:1Hz
  if(mode < LSB){ // FM, AM
    rx->freq += addFreq;
    rx->freq = constrain(rx->freq, rx->minFreq, rx->maxFreq);
  }else{  // LSB, USB
    do{
      if(addFreq >= 1000){
        addFreq -= 1000;
        ++rx->freq;
      }else if(addFreq <= -1000){
        addFreq += 1000;
        --rx->freq;
      }else{
        rx->bfoFreq += addFreq;
        if(rx->bfoFreq >= 1000){
          rx->bfoFreq -= 1000;
          ++rx->freq;
        }else if(rx->bfoFreq < 0){
          rx->bfoFreq += 1000;
          --rx->freq;
        }
        addFreq = 0;
      }
    }while(addFreq);
    if(rx->freq >= rx->maxFreq){
      rx->freq = rx->maxFreq;
      rx->bfoFreq = 0;
    }else if(rx->freq < rx->minFreq){
      rx->freq = rx->minFreq;
      rx->bfoFreq = 0;
    }
    setBfoFreq(rx->bfoFreq);
  }
  setFreq(rx->freq);
}

byte TinySI4732::getIntStatus(){
  return commandOut((const byte[]){GET_INT_STATUS});  
}

byte TinySI4732::getTuneStatus(bool cancel, tTuneStatus &status) {
  byte cmd[] = {
    FM_TUNE_STATUS,
    (byte)(cancel ? 2 : 0)
  };
  if (mode == FM) {
    //
  } else if (mode == AM) {
    cmd[0] = AM_TUNE_STATUS;
  } else {
    cmd[0] = SSB_TUNE_STATUS;
  }
  byte cmdStatus = commandOut(cmd, status);
  status.FREQ = (status.FREQ << 8) | (status.FREQ >> 8);
  status.ANTCAP = (status.ANTCAP << 8) | (status.ANTCAP >> 8);
  return cmdStatus;
}

byte TinySI4732::seekStart(bool seekup) {
  byte cmd[] = {
    FM_SEEK_START,
    (byte)(seekup ? 0b00001100 : 0b00000100)
  };
  if (mode == FM) {
    //
  } else if (mode == AM) {
    cmd[0] = AM_SEEK_START;
  } else {
    return 0b11000000;  // ERROR
  }
  intervalTime = millis();
  seek = true;
  return commandOut(cmd);
}

bool TinySI4732::seekNow(bool cancel){
  tTuneStatus status;

  if(mode >= LSB || !seek)
    return seek = false;  // シーク終了

  if(cancel) {  // シーク中止要求あり
    getTuneStatus(cancel, status);
    delay(80);
  }
  if((word)millis() - intervalTime > 100 || cancel){
    intervalTime += 100;
    if(getTuneStatus(cancel, status) & 1){  // シーク完了
      setFreq(status.FREQ, 0);
      //updateRsqStatus();
      seek = false;
    }else{  // シーク中の周波数更新
      if(mode == FM){
        sprintf(radioLabel[L_FREQ], "%d.%0dM", status.FREQ / 100, status.FREQ % 100 / 10);
      }else{
        sprintf(radioLabel[L_FREQ], "%d.0k", status.FREQ);
      }
    }
  }
  return seek;
}

void TinySI4732::setStereo(bool stereo) {
  if (stereo) {
    setProperty(FM_BLEND_RSSI_STEREO_THRESHOLD, 49);  // 49udB ステレオ強制する時は0を設定する
    setProperty(FM_BLEND_RSSI_MONO_THRESHOLD, 30);    // 30udB ステレオ強制は0
    setProperty(FM_BLEND_SNR_STEREO_THRESHOLD, 27);   // 27dB ステレオ強制は0
    setProperty(FM_BLEND_SNR_MONO_THRESHOLD, 14);     // 14dB ステレオ強制は0
    //setProperty(FM_BLEND_MULTIPATH_STEREO_THRESHOLD, 20); // ステレオ強制は100
    //setProperty(FM_BLEND_MULTIPATH_MONO_THRESHOLD, 60); // ステレオ強制は100
  } else {
    setProperty(FM_BLEND_RSSI_STEREO_THRESHOLD, 127);  // モノラル強制
    setProperty(FM_BLEND_RSSI_MONO_THRESHOLD, 127);
    setProperty(FM_BLEND_SNR_STEREO_THRESHOLD, 127);
    setProperty(FM_BLEND_SNR_MONO_THRESHOLD, 127);
    //setProperty(FM_BLEND_MULTIPATH_STEREO_THRESHOLD, 0);
    //setProperty(FM_BLEND_MULTIPATH_MONO_THRESHOLD, 0);
  }
}

byte TinySI4732::getRsqStatus(tRsqStatus &rsqStatus) {
  byte cmd[] = { FM_RSQ_STATUS, 0 };
  if (mode == FM) {
    //
  } else if (mode == AM) {
    cmd[0] = AM_RSQ_STATUS;
  } else {
    cmd[0] = SSB_RSQ_STATUS;
  }
  return commandOut(cmd, rsqStatus);
}

byte TinySI4732::setFilter(byte filter) {
  if (mode == FM) {
    rx->fmAmFilter = filter = filter > 4 ? 4 : filter;
    strcpy(radioLabel[L_FILTER], (const char *[]){ "AUTO", "110k", "84k", "60k", "40k" }[filter]);
    return setProperty(FM_CHANNEL_FILTER, filter);
  } else if (mode == AM) {
    rx->fmAmFilter = filter = filter > 6 ? 6 : filter;
    strcpy(radioLabel[L_FILTER], (const char *[]){ "6.0k", "4.0k", "3.0k", "2.5kG", "2.0k", "1.8k", "1.0k" }[filter]);
    word amFilterTable[] = { 0x0000, 0x0001, 0x0002, 0x0006, 0x0003, 0x0005, 0x0004 };
    return setProperty(AM_CHANNEL_FILTER, amFilterTable[filter]);
  } else {
    rx->ssbFilter = filter = filter > 5 ? 5 : filter;
    strcpy(radioLabel[L_FILTER], (const char *[]){ "4.0k", "3.0k", "2.2k", "1.2k", "1.0k", "0.5k" }[filter]);
    word ssbfilterTable[] = { 0x9013, 0x9012, 0x9011, 0x9000, 0x9005, 0x9004 };
    return setProperty(SSB_MODE, ssbfilterTable[filter]);
  }
}

byte TinySI4732::getFilterSize() {
  return (const byte[]){ 5, 7, 6, 6 }[mode];
}

byte TinySI4732::setAgcOn(bool agcOn) {
  return setAgcGain(agcOn, rx->agcGain);
}

byte TinySI4732::setAgcGain(byte gain) {
  return setAgcGain(rx->agcOn, gain);
}

byte TinySI4732::setAgcGain(bool agcOn, byte gain) {
  byte cmd[3];
  rx->agcOn = agcOn;
  if (mode == FM) {
    gain = gain > 26 ? 26 : gain;
    cmd[0] = FM_AGC_OVERRIDE;
  } else if (mode == AM) {
    gain = gain > 37 ? 37 : gain;
    cmd[0] = AM_AGC_OVERRIDE;
  } else {
    gain = gain > 37 ? 37 : gain;
    cmd[0] = SSB_AGC_OVERRIDE;
  }
  cmd[1] = agcOn ? 0 : 1;
  cmd[2] = gain;
  rx->agcOn = agcOn;
  rx->agcGain = gain;
  if (agcOn) {
    strcpy(radioLabel[L_AGC], "AGC");
  } else {
    sprintf(radioLabel[L_AGC], "%d", gain);
  }
  return commandOut(cmd);
}

byte TinySI4732::getAgcGainSize() {
  return (const byte[]){ 27, 38, 38, 38 }[mode];
}

byte TinySI4732::setVolume(byte volume) {
  if (volume > 63) volume = 63;
  this->volume = volume;
  if (mute) {
    strcpy(radioLabel[L_VOLUME], "MUTE");
  } else {
    sprintf(radioLabel[L_VOLUME], "%d", volume);
  }
  return setProperty(RX_VOLUME, volume);
}

byte TinySI4732::setMute(bool mute) {
  this->mute = mute;
  if (mute) {
    strcpy(radioLabel[L_VOLUME], "MUTE");
  } else {
    sprintf(radioLabel[L_VOLUME], "%d", volume);
  }
  return setProperty(RX_HARD_MUTE, mute ? 0b11 : 0b00);
}

char *TinySI4732::getLabel(tLabelname labelNo) {
  return radioLabel[labelNo];
}

#ifdef FLASHROMPATCH
bool TinySI4732::patchFlashRomLoad() {
  commandOut((const byte[]){ POWER_UP, 0b00110001, 0b00000101 });  // patch

  byte buf[8];
  byte count = 0;
  const byte *patchArgsAddr = patchArgs;
  const byte *patchDataAddr = patchData;
  for(word addr = 0; addr < sizeof(patchData); addr += 7){
    if(count && --count){
      buf[0] = 0x16;  // PATCH DATA
    }else{
      buf[0] = 0x15;  // PATCH ARGS
      count = pgm_read_byte(patchArgsAddr++);
    }
    for(byte i = 1; i < 8; ++i)
      buf[i] = pgm_read_byte(patchDataAddr++);

    Wire.beginTransmission(SI4732_ADDR);
    Wire.write(buf, 8);
    Wire.endTransmission();

    delayMicroseconds(300);  // tCTS 200us=739ms, 300us=851ms
    Wire.requestFrom(SI4732_ADDR, 1);
    if (Wire.read() & 0x040)
      return false;  // ERROR
  }
  delay(10);
  return true;
}
#else

bool TinySI4732::patchExtEepRomLoad(word startAddr) {
  byte buf[32];

  Wire.beginTransmission(EEPROM_ADDR);  // eepromのリードアドレスをセット
  Wire.write(startAddr >> 8);
  Wire.write(startAddr);
  Wire.endTransmission();

  Wire.requestFrom(EEPROM_ADDR, 32);  // headerの読込
  for (byte i = 0; i < 32; ++i) {
    while (Wire.available() <= 0);
    buf[i] = Wire.read();
  }
  word dataSize = (buf[31] << 8) + buf[30];  // ptach data size

  commandOut((const byte[]){ POWER_UP, 0b00110001, 0b00000101 });  // patch

  for (word addr = 0; addr < dataSize; addr += 32) {
    Wire.requestFrom(EEPROM_ADDR, 32);
    for (byte i = 0; i < 32; ++i) {
      buf[i] = Wire.read();
    }

    for (word addrStep = addr; addrStep < addr + 32 && addrStep < dataSize; addrStep += 8) {
      Wire.beginTransmission(SI4732_ADDR);
      Wire.write(&buf[addrStep - addr], 8);
      Wire.endTransmission();

      delayMicroseconds(300);  // tCTS
      Wire.requestFrom(SI4732_ADDR, 1);
      if (Wire.read() & 0x40)
        return false;  // ERROR
    }
  }
  delay(10);
  return true;
}
#endif
