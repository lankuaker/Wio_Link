/*
 * grove_gyro_itg3200.h
 *
 * Copyright (c) 2012 seeed technology inc.
 * Website    : www.seeed.cc
 * Author     : Jacky Zhang
 *
 * The MIT License (MIT)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */



#ifndef __GROVE_MULTICHANNEL_GAS_MICS6814_H__
#define __GROVE_MULTICHANNEL_GAS_MICS6814_H__

//GROVE_NAME        "Grove - Multichannel Gas Sensor"
//IF_TYPE           I2C
//IMAGE_URL         http://www.seeedstudio.com/wiki/images/2/28/Multi_sensor1.png


#include "suli2.h"

#define MULTICHANNEL_GAS_ADDRESS (0x04<<1)

// Register Defines
#define READ_R0_CH1         0x11
#define READ_R0_CH2         0x12
#define READ_R0_CH3         0x13

#define READ_RS_CH1         0x01
#define READ_RS_CH2         0x02
#define READ_RS_CH3         0x03

#define CHANGE_I2C_ADDR     0x23
#define DO_CALIBARTE        0x22
#define POWER_ON            0x21
#define POWER_OFF           0x20


class GroveMultiChannelGas
{
public:
    GroveMultiChannelGas(int pinsda, int pinscl);
    uint16_t res0[3];//sensors res0
    uint16_t res[3];//sensors res
    
    bool begin(int address);
    bool read_res0(uint16_t *res0_ch1, uint16_t *res0_ch2, uint16_t *res0_ch3);
    bool read_res(uint16_t *res_ch1, uint16_t *res_ch2, uint16_t *res_ch3);
    bool read_concentration(float *nh3, float *co, float *no2);
    bool changeI2cAddr(uint8_t newAddr);
    bool doCalibrate(void);
    bool powerOn(void);
    bool powerOff(void);

private:
    I2C_T *i2c;
    uint8_t i2cAddress;//I2C address of the sensor
    void sendCmd(uint8_t dta);
    int16_t readData(uint8_t cmd);

};


#endif
