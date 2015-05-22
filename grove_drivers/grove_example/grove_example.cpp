

#include "suli2.h"
#include "grove_example.h"

void grove_example_init(I2C_T *i2c, int pinsda, int pinscl)
{
    suli_i2c_init(i2c, pinsda, pinscl);
}

bool grove_example_read_temp(I2C_T *i2c, int *temp)
{
    *temp = 30;
    return true;
}

bool grove_example_read_uint8_value(I2C_T *i2c, uint8_t *value)
{
    *value = 255;
    return true;
}

bool grove_example_read_humidity(I2C_T *i2c, float *humidity)
{
    *humidity = 52.5;
    return true;
}

bool grove_example_read_acc(I2C_T *i2c, float *ax, float *ay, float *az)
{
    *ax = 12.3; *ay = 45.6; *az = 78.0;
    return true;
}
bool grove_example_read_compass(I2C_T *i2c, float *cx, float *cy, float *cz, int *degree)
{
    *cx = 12.3; *cy = 45.6; *cz = 78.0; *degree = 90;
    return true;
}

bool grove_example_write_acc_mode(I2C_T *i2c, uint8_t mode)
{
    suli_i2c_write(i2c, 0x00, &mode, 1);
    return true;
}

bool grove_example_write_float_value(I2C_T *i2c, float f)
{
    return false;
}

bool grove_example_write_multi_value(I2C_T *i2c, int a, float b, int8_t c)
{
    _grove_example_internal_function(i2c, b);
    return true;
}

EVENT_T event1;

bool grove_example_attach_event_handler(CALLBACK_T handler)
{
    suli_event_init(&event1, handler);
}

void _grove_example_internal_function(I2C_T *i2c, float x)
{
    suli_event_trigger(&event1, "fire", 250);
}
