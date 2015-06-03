


#ifndef __GROVE_RELAY_H__
#define __GROVE_RELAY_H__

#include "suli2.h"

void grove_relay_init(IO_T *io, int pin);

bool grove_relay_write_onoff(IO_T *io, int onoff);

#endif
