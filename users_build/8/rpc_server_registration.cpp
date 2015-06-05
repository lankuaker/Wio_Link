#include "suli2.h"
#include "rpc_server.h"
#include "rpc_stream.h"

#include "grove_relay_gen.h"

#include "grove_example_gen.h"


void rpc_server_register_resources()
{
    uint8_t arg_types[MAX_INPUT_ARG_LEN];
    
    //Grove_Example
    GroveRelay *Grove_Relay_ins = new GroveRelay(14);
    memset(arg_types, TYPE_NONE, MAX_INPUT_ARG_LEN);

    memset(arg_types, TYPE_NONE, MAX_INPUT_ARG_LEN);
    arg_types[0] = TYPE_INT;
    rpc_server_register_method("Grove_Relay", "onoff", METHOD_WRITE, __grove_relay_write_onoff, Grove_Relay_ins, arg_types);
    GroveExample *Grove_Example_ins = new GroveExample(4,5);
    memset(arg_types, TYPE_NONE, MAX_INPUT_ARG_LEN);
    memset(arg_types, TYPE_NONE, MAX_INPUT_ARG_LEN);
    rpc_server_register_method("Grove_Example", "temp", METHOD_READ, __grove_example_read_temp, Grove_Example_ins, arg_types);
    memset(arg_types, TYPE_NONE, MAX_INPUT_ARG_LEN);
    rpc_server_register_method("Grove_Example", "uint8_value", METHOD_READ, __grove_example_read_uint8_value, Grove_Example_ins, arg_types);
    memset(arg_types, TYPE_NONE, MAX_INPUT_ARG_LEN);
    arg_types[0] = TYPE_INT;
    rpc_server_register_method("Grove_Example", "with_arg", METHOD_READ, __grove_example_read_with_arg, Grove_Example_ins, arg_types);
    memset(arg_types, TYPE_NONE, MAX_INPUT_ARG_LEN);
    rpc_server_register_method("Grove_Example", "compass", METHOD_READ, __grove_example_read_compass, Grove_Example_ins, arg_types);
    memset(arg_types, TYPE_NONE, MAX_INPUT_ARG_LEN);
    rpc_server_register_method("Grove_Example", "acc", METHOD_READ, __grove_example_read_acc, Grove_Example_ins, arg_types);
    memset(arg_types, TYPE_NONE, MAX_INPUT_ARG_LEN);
    rpc_server_register_method("Grove_Example", "humidity", METHOD_READ, __grove_example_read_humidity, Grove_Example_ins, arg_types);

    memset(arg_types, TYPE_NONE, MAX_INPUT_ARG_LEN);
    arg_types[0] = TYPE_INT;
    arg_types[1] = TYPE_FLOAT;
    arg_types[2] = TYPE_UINT32;
    rpc_server_register_method("Grove_Example", "multi_value", METHOD_WRITE, __grove_example_write_multi_value, Grove_Example_ins, arg_types);
    memset(arg_types, TYPE_NONE, MAX_INPUT_ARG_LEN);
    arg_types[0] = TYPE_UINT8;
    rpc_server_register_method("Grove_Example", "acc_mode", METHOD_WRITE, __grove_example_write_acc_mode, Grove_Example_ins, arg_types);
    memset(arg_types, TYPE_NONE, MAX_INPUT_ARG_LEN);
    arg_types[0] = TYPE_FLOAT;
    rpc_server_register_method("Grove_Example", "float_value", METHOD_WRITE, __grove_example_write_float_value, Grove_Example_ins, arg_types);

    Grove_Example_ins->attach_event_handler(rpc_server_event_report);
}

void print_well_known()
{
    writer_print(TYPE_STRING, "[");
    writer_print(TYPE_STRING, "\"POST " OTA_SERVER_URL_PREFIX "/Grove_Relay/onoff/int_onoff\"");
    writer_print(TYPE_STRING, "\"GET " OTA_SERVER_URL_PREFIX "/Grove_Example/temp -> int : temp\",");
    writer_print(TYPE_STRING, "\"GET " OTA_SERVER_URL_PREFIX "/Grove_Example/uint8_value -> uint8_t : value\",");
    writer_print(TYPE_STRING, "\"GET " OTA_SERVER_URL_PREFIX "/Grove_Example/with_arg/int_arg -> float : cx, float : cy, float : cz, int : degree\",");
    writer_print(TYPE_STRING, "\"GET " OTA_SERVER_URL_PREFIX "/Grove_Example/compass -> float : cx, float : cy, float : cz, int : degree\",");
    writer_print(TYPE_STRING, "\"GET " OTA_SERVER_URL_PREFIX "/Grove_Example/acc -> float : ax, float : ay, float : az\",");
    writer_print(TYPE_STRING, "\"GET " OTA_SERVER_URL_PREFIX "/Grove_Example/humidity -> float : humidity\",");
    writer_print(TYPE_STRING, "\"POST " OTA_SERVER_URL_PREFIX "/Grove_Example/multi_value/int_a/float_b/uint32_t_c\",");
    writer_print(TYPE_STRING, "\"POST " OTA_SERVER_URL_PREFIX "/Grove_Example/acc_mode/uint8_t_mode\",");
    writer_print(TYPE_STRING, "\"POST " OTA_SERVER_URL_PREFIX "/Grove_Example/float_value/float_f\"");
    writer_print(TYPE_STRING, "]");
}
