/*
 * grove_gyro_itg3200.cpp
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


#include "suli2.h"
#include "grove_multichannel_gas_mics6814.h"

#define USE_DEBUG (1)

GroveMultiChannelGas::GroveMultiChannelGas(int pinsda, int pinscl)
{
    this->i2c = (I2C_T *)malloc(sizeof(I2C_T));
    suli_i2c_init(this->i2c, pinsda, pinscl);
    
    //get the default address of device. user should use his own address he set to the device previously.
    i2cAddress = MULTICHANNEL_GAS_ADDRESS;
    //read out the R0 value of each channel
    while(read_res0(&res0[0], &res0[1], &res0[2]) != true)
    {
#if USE_DEBUG
        Serial1.println("wait...");
#endif
        delay(1000);
    }

#if USE_DEBUG
    Serial1.print("Res0[0]: ");
    Serial1.println(res0[0]);
    Serial1.print("Res0[1]: ");
    Serial1.println(res0[1]);
    Serial1.print("Res0[2]: ");
    Serial1.println(res0[2]);
#endif    
    powerOn();
}

/*********************************************************************************************************
** Function name:           sendCmd
** Descriptions:            send one byte to I2C Wire
*********************************************************************************************************/
void GroveMultiChannelGas::sendCmd(uint8_t dta)
{
    suli_i2c_write(this->i2c, i2cAddress, &dta, 1);
}

/*********************************************************************************************************
** Function name:           readData
** Descriptions:            read 4 bytes from I2C slave
*********************************************************************************************************/
int16_t GroveMultiChannelGas::readData(uint8_t cmd)
{
    uint16_t timeout = 0;
    uint8_t buffer[4];
    uint8_t checksum = 0;
    int16_t rtnData = 0;

    //send command
    sendCmd(cmd);
    //wait for a while, because the device is a MCU, it need some time to handle the request.
    delay(2);
    //read sensor data
    int length = suli_i2c_read(this->i2c, i2cAddress, buffer, 4);
    if(length != 4)
    {
         return -3;//rtnData length wrong
    }
    //checksum
    checksum = (uint8_t)(buffer[0] + buffer[1] + buffer[2]);
    if(checksum != buffer[3])
        return -4;//checksum wrong
    rtnData = ((buffer[1] << 8) + buffer[2]);
    
    return rtnData;//successful
}

/*********************************************************************************************************
** Function name:           readR0
** Descriptions:            read R0 stored in slave MCU
*********************************************************************************************************/
bool GroveMultiChannelGas::read_res0(uint16_t *res0_ch1, uint16_t *res0_ch2, uint16_t *res0_ch3)
{
    int16_t rtnData = 0;

    rtnData = readData(READ_R0_CH1);
    if(rtnData >= 0)
        res0[0] = rtnData;
    else
        return false;//unsuccessful

    rtnData = readData(READ_R0_CH2);
    if(rtnData >= 0)
        res0[1] = rtnData;
    else
        return false;//unsuccessful

    rtnData = readData(READ_R0_CH3);
    if(rtnData >= 0)
        res0[2] = rtnData;
    else
        return false;//unsuccessful
    
    *res0_ch1 = res0[0];
    *res0_ch2 = res0[1];
    *res0_ch3 = res0[2];
    
    return true;//successful
}

/*********************************************************************************************************
** Function name:           readRS
** Descriptions:            read resistance value of each channel from slave MCU
*********************************************************************************************************/
bool GroveMultiChannelGas::read_res(uint16_t *res_ch1, uint16_t *res_ch2, uint16_t *res_ch3)
{
    int16_t rtnData = 0;

    rtnData = readData(READ_RS_CH1);
    if(rtnData >= 0)
        res[0] = rtnData;
    else
        return false;//unsuccessful

    rtnData = readData(READ_RS_CH2);
    if(rtnData >= 0)
        res[1] = rtnData;
    else
        return false;//unsuccessful

    rtnData = readData(READ_RS_CH3);
    if(rtnData >= 0)
        res[2] = rtnData;
    else
        return false;//unsuccessful

    *res_ch1 = res[0];
    *res_ch2 = res[1];
    *res_ch3 = res[2];
    
    return true;//successful
}

/*********************************************************************************************************
** Function name:           calcGas
** Descriptions:            calculate gas concentration of each channel from slave MCU
*********************************************************************************************************/
bool GroveMultiChannelGas::read_concentration(float *nh3, float *co, float *no2)
{
    
    float ratio0 = (float)res[0] / res0[0];
    if(ratio0 < 0.04) ratio0 = 0.04;
    if(ratio0 > 0.8) ratio0 = 0.8;
    float ratio1 = (float)res[1] / res0[1];
    if(ratio1 < 0.01) ratio1 = 0.01;
    if(ratio1 > 3) ratio1 = 3;
    float ratio2 = (float)res[2] / res0[2];
    if(ratio2 < 0.07) ratio2 = 0.07;
    if(ratio2 > 40) ratio2 = 40;
    
    *nh3 = 1 / (ratio0 * ratio0 * pow(10, 0.4));
    *co = pow(10, 0.6) / pow(ratio1, 1.2);
    *no2 = ratio2 / pow(10, 0.8);
    
    return true;
}

/*********************************************************************************************************
** Function name:           changeI2cAddr
** Descriptions:            change I2C address of the slave MCU, and this address will be stored in EEPROM of slave MCU
*********************************************************************************************************/
bool GroveMultiChannelGas::changeI2cAddr(uint8_t newAddr)
{
    Wire.beginTransmission(i2cAddress); // transmit to device
    Wire.write(CHANGE_I2C_ADDR);              // sends one byte
    Wire.write(newAddr);              // sends one byte
    Wire.endTransmission();    // stop transmitting
    i2cAddress = newAddr;
}

/*********************************************************************************************************
** Function name:           doCalibrate
** Descriptions:            tell slave to do a calibration, it will take about 8~10s
*********************************************************************************************************/
bool GroveMultiChannelGas::doCalibrate(void)
{
    sendCmd(DO_CALIBARTE);
}

/*********************************************************************************************************
** Function name:           powerOn
** Descriptions:            power on sensor heater
*********************************************************************************************************/
bool GroveMultiChannelGas::powerOn(void)
{
    sendCmd(POWER_ON);
}

/*********************************************************************************************************
** Function name:           powerOff
** Descriptions:            power off sensor heater
*********************************************************************************************************/
bool GroveMultiChannelGas::powerOff(void)
{
    sendCmd(POWER_OFF);
}




