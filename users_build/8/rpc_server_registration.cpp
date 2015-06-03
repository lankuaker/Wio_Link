#include "suli2.h"
#include "rpc_server.h"
#include "rpc_stream.h"

#include "grove_relay_gen.h"


void rpc_server_register_resources()
{
    uint8_t arg_types[MAX_INPUT_ARG_LEN];
    
    //GroveRelay
    GroveRelay *GroveRelay_ins = new GroveRelay(4);
    memset(arg_types, TYPE_NONE, MAX_INPUT_ARG_LEN);

    memset(arg_types, TYPE_NONE, MAX_INPUT_ARG_LEN);
    arg_types[0] = TYPE_INT;
    rpc_server_register_method("GroveRelay", "onoff", METHOD_WRITE, __grove_relay_write_onoff, GroveRelay_ins, arg_types);
}

void print_well_known()
{
    writer_print(TYPE_STRING, "[");
    writer_print(TYPE_STRING, "\"POST " OTA_SERVER_URL_PREFIX "/GroveRelay/onoff/int_onoff\"");
    writer_print(TYPE_STRING, "]");
}
