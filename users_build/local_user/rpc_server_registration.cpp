#include "suli2.h"
#include "rpc_server.h"

#include "grove_example_gen.h"


void rpc_server_register_resources()
{
    uint8_t arg_types[MAX_INPUT_ARG_LEN];
    
    //Grove_Example1
    GroveExample *Grove_Example1 = new GroveExample(2,4);
    memset(arg_types, TYPE_NONE, MAX_INPUT_ARG_LEN);
    memset(arg_types, TYPE_NONE, MAX_INPUT_ARG_LEN);
    rpc_server_register_method("Grove_Example1", "temp", METHOD_READ, __grove_example_read_temp, Grove_Example1, arg_types);
    memset(arg_types, TYPE_NONE, MAX_INPUT_ARG_LEN);
    rpc_server_register_method("Grove_Example1", "uint8_value", METHOD_READ, __grove_example_read_uint8_value, Grove_Example1, arg_types);
    memset(arg_types, TYPE_NONE, MAX_INPUT_ARG_LEN);
    arg_types[0] = TYPE_INT;
    rpc_server_register_method("Grove_Example1", "with_arg", METHOD_READ, __grove_example_read_with_arg, Grove_Example1, arg_types);
    memset(arg_types, TYPE_NONE, MAX_INPUT_ARG_LEN);
    rpc_server_register_method("Grove_Example1", "compass", METHOD_READ, __grove_example_read_compass, Grove_Example1, arg_types);
    memset(arg_types, TYPE_NONE, MAX_INPUT_ARG_LEN);
    rpc_server_register_method("Grove_Example1", "acc", METHOD_READ, __grove_example_read_acc, Grove_Example1, arg_types);
    memset(arg_types, TYPE_NONE, MAX_INPUT_ARG_LEN);
    rpc_server_register_method("Grove_Example1", "humidity", METHOD_READ, __grove_example_read_humidity, Grove_Example1, arg_types);

    memset(arg_types, TYPE_NONE, MAX_INPUT_ARG_LEN);
    arg_types[0] = TYPE_INT;
    arg_types[1] = TYPE_FLOAT;
    arg_types[2] = TYPE_UINT32;
    rpc_server_register_method("Grove_Example1", "multi_value", METHOD_WRITE, __grove_example_write_multi_value, Grove_Example1, arg_types);
    memset(arg_types, TYPE_NONE, MAX_INPUT_ARG_LEN);
    arg_types[0] = TYPE_UINT8;
    rpc_server_register_method("Grove_Example1", "acc_mode", METHOD_WRITE, __grove_example_write_acc_mode, Grove_Example1, arg_types);
    memset(arg_types, TYPE_NONE, MAX_INPUT_ARG_LEN);
    arg_types[0] = TYPE_FLOAT;
    rpc_server_register_method("Grove_Example1", "float_value", METHOD_WRITE, __grove_example_write_float_value, Grove_Example1, arg_types);

    Grove_Example1->attach_event_handler(rpc_server_event_report);
}
