#include "suli2.h"
#include "rpc_server.h"
#include "rpc_stream.h"

#include "grove_relay_gen.h"


void rpc_server_register_resources()
{
    uint8_t arg_types[MAX_INPUT_ARG_LEN];
    
    //Grove_Relay
    GroveRelay *Grove_Relay_ins = new GroveRelay(14);
    memset(arg_types, TYPE_NONE, MAX_INPUT_ARG_LEN);

    memset(arg_types, TYPE_NONE, MAX_INPUT_ARG_LEN);
    arg_types[0] = TYPE_INT;
    rpc_server_register_method("Grove_Relay", "onoff", METHOD_WRITE, __grove_relay_write_onoff, Grove_Relay_ins, arg_types);
}

void print_well_known()
{
    writer_print(TYPE_STRING, "[");
    writer_print(TYPE_STRING, "\"POST " OTA_SERVER_URL_PREFIX "/Grove_Relay/onoff/int_onoff\"");
    writer_print(TYPE_STRING, "]");
}
