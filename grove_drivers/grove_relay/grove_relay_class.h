


#ifndef __GROVE_RELAY_CLASS_H__
#define __GROVE_RELAY_CLASS_H__

#include "grove_relay.h"

//GROVE_NAME        "Grove_Relay"
//IF_TYPE           GPIO

class GroveRelay
{
public:
    GroveRelay(int pin);
    bool write_onoff(int onoff);
private:
    IO_T *io;
};

#endif
