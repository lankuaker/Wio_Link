


#include "grove_relay_class.h"

GroveRelay::GroveRelay(int pin)
{
    this->io = (IO_T *)malloc(sizeof(IO_T));
    grove_relay_init(io, pin);
}

bool GroveRelay::write_onoff(int onoff)
{
    return grove_relay_write_onoff(io, onoff);
}

