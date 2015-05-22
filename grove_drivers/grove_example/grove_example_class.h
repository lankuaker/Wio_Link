


#ifndef __GROVE_EXAMPLE_CLASS_H__
#define __GROVE_EXAMPLE_CLASS_H__

#include "grove_example.h"

//GROVE_NAME        "Grove_Example"
//IF_TYPE           I2C

class GroveExample
{
public:
    GroveExample(int pinsda, int pinscl);
    bool read_temp(int *temp);
    bool read_uint8_value(uint8_t *value);
    bool read_humidity(float *humidity);
    bool read_acc(float *ax, float *ay, float *az);
    bool read_compass(float *cx, float *cy, float *cz, int *degree);
    bool write_acc_mode(uint8_t mode);
    bool write_float_value(float f);
    bool write_multi_value(int a, float b, int8_t c);
    bool attach_event_handler(CALLBACK_T handler);

private:
    I2C_T *i2c;
};

#endif
