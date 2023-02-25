#pragma once
//#define DEBUG // コマンド入出力をシリアル出力
#include "patch.h"  // patchをFlashROMから読み込む。コメント時は外部EEPROMから読み込む。

#define FM  0
#define AM  1
#define LSB 2
#define USB 3
#define POWER_UP        0x01  // Power up device and mode selection. 
#define GET_REV         0x10  // Returns revision information on the device. 
#define POWER_DOWN      0x11  // Power down device. 
#define SET_PROPERTY    0x12  // Sets the value of a property. 
#define GET_PROPERTY    0x13  // Retrieves a property’s value. 
#define GET_INT_STATUS  0x14  // Reads interrupt status bits. 
#define SSB_TUNE_FREQ   0x40  // Selects the SSB tuning frequency. 
#define SSB_TUNE_STATUS 0x42  // Queries the status of previous SSB_TUNE_FREQ 
#define SSB_RSQ_STATUS  0x43  // Queries the status of the Received Signal Quality (RSQ) of the current channel 
#define SSB_AGC_STATUS  0x47  // Queries the current AGC settings. 
#define SSB_AGC_OVERRIDE 0x48 // Override AGC setting by disabling and forcing it to a fixed value. 

#define AM_TUNE_FREQ    0x40  // Tunes to a given AM frequency.
#define AM_SEEK_START   0x41  // Begins searching for a valid frequency.
#define AM_TUNE_STATUS  0x42  // Queries the status of the already issued AM_TUNE_FREQ or AM_SEEK_START command.
#define AM_RSQ_STATUS   0x43  // Queries the status of the Received Signal Quality (RSQ) for the current channel.
#define AM_AGC_STATUS   0x47  // Queries the current AGC settings.
#define AM_AGC_OVERRIDE 0x48  // Overrides AGC settings by disabling and forcing it to a fixed value.

#define FM_TUNE_FREQ    0x20  // Selects the FM tuning frequency.
#define FM_SEEK_START   0x21  // Begins searching for a valid frequency. All
#define FM_TUNE_STATUS  0x22  // Queries the status of previous FM_TUNE_FREQ or FM_SEEK_START command.
#define FM_RSQ_STATUS   0x23  // Queries the status of the Received Signal Quality (RSQ) of the current channel.
#define FM_RDS_STATUS   0x24  // Returns RDS information for current channel and reads an entry from RDS FIFO.
#define FM_AGC_STATUS   0x27  // Queries the current AGC settings All
#define FM_AGC_OVERRIDE 0x28  // Override AGC setting by disabling and forcing it to a fixed value All

#define AM_CHANNEL_FILTER               0x3102 
#define SSB_BFO                         0x0100 
#define SSB_MODE                        0x0101  
#define FM_DEEMPHASIS                   0x1100 
#define FM_CHANNEL_FILTER               0x1102
#define FM_SEEK_BAND_BOTTOM             0x1400 
#define FM_SEEK_BAND_TOP                0x1401 
#define FM_SEEK_FREQ_SPACING            0x1402 
#define FM_BLEND_RSSI_STEREO_THRESHOLD  0x1800 
#define FM_BLEND_RSSI_MONO_THRESHOLD    0x1801 
#define FM_BLEND_SNR_STEREO_THRESHOLD   0x1804 
#define FM_BLEND_SNR_MONO_THRESHOLD     0x1805 
#define AM_SEEK_BAND_BOTTOM             0x3400 
#define AM_SEEK_BAND_TOP                0x3401 
#define AM_SEEK_FREQ_SPACING            0x3402 
#define RX_VOLUME                       0x4000
#define RX_HARD_MUTE                    0x4001



struct tGetRev{
  byte STATUS;      // 
  byte PN;          // 2桁のパーツナンバー
  char FIRMWARE[2]; // ファームウェアリビジョン
  word PATCH;       // パッチID
  char COMPONET[2]; // コンポーネントリビジョン
  char CHIPREV;     // チップリビジョン
};
struct tTuneStatus{
  byte STATUS;      //
  byte RESP1;       //
  word FREQ;        //
  byte RSSI;        //
  byte SNR;         //
  word ANTCAP;      // AM:ANTCAP, FM:MULT
};
struct tRsqStatus{
  byte STATUS;      //
  byte RESP1;       //
  byte RESP2;       //
  byte RESP3;       //
  byte RSSI;        //
  byte SNR;         //
  byte MULT;        // FM only
  byte FREQOFF;     // FM only
};
struct tAgcStatus{
  byte STATUS;      //
  byte RESP1;       //
  byte RESP2;       //
};

struct tRadio{
  byte mode;        // 0:FM, 1:AM, 2:LSB, 3:USB
  word freq;        // 受信周波数 FM:6400~10800MHz, AM:149k~23000kHz, LSB/USB:520k~30000kHz
  word fmAmAntCap;  // FM:0(auto),1~191, AM:0(auto),1~6143
  word ssbAntCap;   // LSB/USB:1~6143
  word minFreq;     // シーク下限周波数
  word maxFreq;     // シーク上限周波数
  byte stepFreq;    // 10:FM 100kHz, 1:AM 1kHz, 5:AM 5kHz, 9:AM 9kHz
  bool stereo;      // true:stereo, false:mono
  bool agcOn;       // true:AGC=ON ATT=OFF, false AGC=OFF ATT=ON
  byte agcGain;     // 0~255
  byte fmAmFilter;  // FM:0:AUTO 1:110k, 2:84k 3:60k 4:40k
                    // AM:0:6.0k, 1:4.0k, 2:3.0k, 3:2.5kG, 4:2.0k, 5:1.8k, 6:1.0k
  byte ssbFilter;   // LSB/USB:0:4.0k, 1:3.0k, 2:2.2k, 3:1.2k, 4:1.0k, 5:0.5k
  int  bfoFreq;     // -16383kHz~16383kHz
};
enum tLabelname {L_MODE, L_FREQ, L_STEREO, L_FILTER, L_AGC, L_VOLUME, LABEL_SIZE};  // getLabel()での引数

class TinySI4732{
  public:
  TinySI4732(byte RESET_PIN);
  void reset();
  void setup();
  byte powerUp(byte func);
  byte powerDown();

  void setRadio(tRadio *radio);           // rRadioを通して総括的に設定をする
  byte getRev(tGetRev &rev);              // チップリビジョンを取得
  byte setFreq(word freq);                // 受信周波数の設定
  byte setFreq(word freq, word antCap);   // 受信周波数とアンテナキャパシタンスの設定
  byte setBfoFreq(int bfoFreq);           // BFOの設定
  void addFreq(int addFreq);              // 受信周波数を加算する UNIT FM:0.01MHz, AM:1kHz, SSB:1Hz
  void setStereo(bool stereo);            // FMステレオ受信有無の設定 true:auto stereo, false:mono
  byte getRsqStatus(tRsqStatus &rsqStatus);  // rsqステータスの更新（RSSI、SNR）
  byte getIntStatus();                    // ステータスの取得
  byte seekStart(bool seekup);            // シーク開始（SSBを除く）
  bool seekNow(bool cancel);              // シーク中は定期的に呼び出す。
  byte getTuneStatus(bool cancel, tTuneStatus &status);        // tuneStatusの更新

  byte setFilter(byte filter);            // 受信フィルタの設定
  byte getFilterSize();                   // 受信フィルタの数量取得
  byte setAgcOn(bool agcOn);              // AGCオン・オフの設定 true:AGC ON,ATT OFF, false:AGC OFF,ATT ON
  byte setAgcGain(byte att);              // AGCオフ時のATT設定
  byte setAgcGain(bool agcOn, byte gain); // AGCのオンオフ、ATT値の設定
  byte getAgcGainSize();                  // ATTゲイン設定数を取得
  byte setVolume(byte volume);            // 音量設定
  byte setMute(bool muteOn);              // 消音設定
  char *getLabel(tLabelname labelNo);           // MODE, FREQ, STEREO, FILTER, AGC, VOLUMEの文字列を取得

  template <typename T1> byte commandOut(const T1 &cmd); // コマンド出力
  template <typename T1, typename T2> byte commandOut(const T1 &cmd, T2 &response); // コマンド出力
  byte setProperty(word property, word data);   // プロパティ値の設定
  word getProperty(word property);              // プロパティ値の取得
  #ifdef FLASHROMPATCH
  bool patchFlashRomLoad();   // FLSH ROMからpatchを読み込む
  #else
  bool patchExtEepRomLoad(word startAddr);  // 外部EEPROMからpatchを読み込む
  #endif

//  tRsqStatus rsqStatus;     //

  private:
  byte RESET_PIN;           // リセットピン番号
  byte mode;                // 0:FM, 1:AM, 2:LSB, 3:USB
  tRadio *rx;               //
  byte volume;              // 0:min - 63:max
  bool mute;                // true:mute
  char radioLabel[LABEL_SIZE][12];        //
  word intervalTime;        //
  bool seek;                //

};

