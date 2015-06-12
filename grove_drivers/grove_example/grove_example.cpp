

#include "suli2.h"
#include "grove_example.h"



GroveExample::GroveExample(int pinsda, int pinscl)
{
    this->i2c = (I2C_T *)malloc(sizeof(I2C_T));
    this->pin = (IO_T *)malloc(sizeof(IO_T));

    suli_i2c_init(i2c, pinsda, pinscl);
    suli_pin_init(pin, 13, INPUT_PULLUP);
}

bool GroveExample::read_temp(int *temp)
{
    *temp = 30;
    return true;
}

bool GroveExample::read_uint8_value(uint8_t *value)
{
    *value = 255;
    return true;
}

bool GroveExample::read_humidity(float *humidity)
{
    *humidity = 52.5;
    return true;
}

bool GroveExample::read_acc(float *ax, float *ay, float *az)
{
    *ax = 12.3; *ay = 45.6; *az = 78.0;
    return true;
}
bool GroveExample::read_compass(float *cx, float *cy, float *cz, int *degree)
{
    *cx = 12.3; *cy = 45.6; *cz = 78.0; *degree = 90;
    return true;
}

bool GroveExample::read_with_arg(float *cx, float *cy, float *cz, int *degree, int arg)
{
    *cx = 12.3; *cy = 45.6; *cz = 78.0; *degree = arg;
    return true;
}

bool GroveExample::write_acc_mode(uint8_t mode)
{
    //suli_i2c_write(i2c, 0x00, &mode, 1);
    return true;
}

bool GroveExample::write_float_value(float f)
{
    String str(f);
    Serial1.print("get float: ");
    Serial1.println(str);
    return false;
}

bool GroveExample::write_multi_value(int a, float b, uint32_t c)
{
    //_GroveExample::internal_function(i2c, b);
    Serial1.print("get uint32: ");
    Serial1.println(c);
    return true;
}


bool GroveExample::attach_event_reporter(CALLBACK_T reporter)
{
    this->event1 = (EVENT_T *)malloc(sizeof(EVENT_T));

    suli_event_init(event1, reporter);

    suli_pin_attach_interrupt_handler(pin, &pin_interrupt_handler, SULI_RISE, this);

    return true;
}

void GroveExample::_internal_function(float x)
{

}

static void pin_interrupt_handler(void *para)
{
    GroveExample *g = (GroveExample *)para;

    suli_event_trigger(g->event1, "fire", *(g->pin));
    Serial1.println("triggered");
}
