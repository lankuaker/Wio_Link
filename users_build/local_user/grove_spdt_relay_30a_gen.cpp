#include "grove_spdt_relay_30a_gen.h"
#include "rpc_server.h"
#include "rpc_stream.h"

void __grove_spdt_relay_30a_write_onoff(void *class_ptr, void *input)
{
    GroveSPDTRelay30A *grove = (GroveSPDTRelay30A *)class_ptr;
    uint8_t *arg_ptr = (uint8_t *)input;
    int onoff;
    
    onoff = *((int *)arg_ptr); arg_ptr += sizeof(int);

    if(grove->write_onoff(onoff))
        writer_print(TYPE_STRING, "\"OK\"");
    else
        writer_print(TYPE_STRING, "\"Failed\"");
}

