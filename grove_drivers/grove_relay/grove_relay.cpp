

#include "suli2.h"
#include "grove_relay.h"

void grove_relay_init(IO_T *io, int pin)
{
    suli_pin_init(io, pin, SULI_OUTPUT);
}

bool grove_relay_write_onoff(IO_T *io, int onoff)
{
    suli_pin_write(io, onoff);
    return true;
}

