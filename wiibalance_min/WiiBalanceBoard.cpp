#include "arduino.h"
#include "WiiBalanceBoard.h"
#include <Wire.h>

byte WiiBalanceBoard::init() {
  //I2CのInitialize SDA=4, SCL=5
  Wire.begin(4, 5);
  getcparams();
  for (byte i = 0; i < 4; i++) {
    zeroweights[i] = 0;
  }
  return 0;
}

/*
    byte setZero(float* zero)
    オフセット値を指定
*/
void WiiBalanceBoard::setZero(float* zero) {
  for (byte i = 0; i < 4; i++) {
    zeroweights[i] += zero[i];
  }
}

/*
    byte adjustZero()
    getWeightsでオフセットの値を取得
    戻り値:  1 成功
             その他 何らかのエラー
*/
byte WiiBalanceBoard::adjustZero() {
  byte ret;
  if ((ret = getWeights(zeroweights)) != 0) {
    return ret;
  }
  return 1;
}

/*
    byte getWeights(float* weights)
    getParamsを用いて取得後、weightに変換
    戻り値:  1 成功
             その他 何らかのエラー
    引数：
      weights  取得データの格納先
*/
byte WiiBalanceBoard::getWeights(float* weights) {
  byte ret;
  word P0[4];
  // パラメータ取得
  if ((ret = getParams(P0)) != 1) {
    return ret;
  }
  // 換算
  for (byte i = 0; i < 4; i++) {
    if (P0[i] < cparams[1][i]) {
      weights[i] = 68.0 * ((float)P0[i] - cparams[0][i]) / (cparams[1][i] - cparams[0][i]) - zeroweights[i];
    } else {
      weights[i] = 68.0 * ((float)P0[i] - cparams[1][i]) / (cparams[2][i] - cparams[1][i]) + 68 - zeroweights[i];
    }
  }
  return 1;
}

/*
    byte getParams(word* params)
    WBCRVLに問い合わせパラメータを取得
    戻り値:  1 成功
             その他 何らかのエラー
    引数：
      params  取得データの格納先(4word確保が必要)
*/
byte WiiBalanceBoard::getParams(word* params) {
  byte buf[32];
  byte ret;
  unsigned char i = 0;

  //WBCRVLからパラメータの取得
  Wire.beginTransmission(I2C_WBCRVL_ADDR);
  Wire.write(0x00);
  if ((ret = Wire.endTransmission()) != 0) {
    return ret;
  }

  delayMicroseconds(150);

  Wire.requestFrom(I2C_WBCRVL_ADDR, 19);
  if (Wire.available() < 19) {
    return 8;
  }
  while (Wire.available()) {
    buf[i] =  Wire.read();
    i++;
  }
  //データの整形
  for (i = 0; i < 4; i++) {
    params[i] = (buf[2 * i] << 8) + buf[2 * i + 1];
  }
  return 1;
}

/*
    byte getcparams()
    WBCRVLからキャリブレーションパラメータを取得
    戻り値:  1 成功
             その他 何らかのエラー
*/
byte WiiBalanceBoard::getcparams() {
  byte buf[32];
  byte ret;
  unsigned char i, j;

  for (i = 0; i < 3; i++) {
    //WBCRVLからキャリブレーションパラメータの取得
    Wire.beginTransmission(I2C_WBCRVL_ADDR);
    Wire.write(0x24 + i * 8);
    if ((ret = Wire.endTransmission()) != 0) {
      return ret;
    }

    delayMicroseconds(150);

    Wire.requestFrom(I2C_WBCRVL_ADDR, 8);
    if (Wire.available() < 8) {
      return 8;
    }
    j = 0;
    while (Wire.available()) {
      buf[j] =  Wire.read();
      j++;
    }
    //データの整形
    for (j = 0; j < 4; j++) {
      cparams[i][j] = (buf[2 * j] << 8) + buf[2 * j + 1];
    }
  }
  return 1;
}

