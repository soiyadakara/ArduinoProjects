#ifndef _WiiBalanceBoard_h
#define _WiiBalanceBoard_h
#include <arduino.h>

#define I2C_WBCRVL_ADDR (0x52)

class WiiBalanceBoard{
  public:
    byte init();
    void setZero(float* zero);
    byte adjustZero();
    byte getParams(word* params);
    byte getWeights(float* weights);
  private:
    byte getcparams();
    //Zero設定した時のweight
    float zeroweights[4];
    // キャリブレーションパラメータ
    word cparams[3][4];
};

#endif
