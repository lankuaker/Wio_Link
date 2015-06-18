#include "suli2.h"
#include "rpc_server.h"
#include "rpc_stream.h"

#include "grove_acc_mma7660_gen.h"

#include "grove_ir_distance_intr_gen.h"

#include "grove_example_gen.h"

#include "grove_moisture_gen.h"

#include "grove_baro_bmp085_gen.h"

#include "grove_digital_light_gen.h"

#include "grove_gyro_itg3200_gen.h"


void rpc_server_register_resources()
{
    uint8_t arg_types[MAX_INPUT_ARG_LEN];
    

    //GroveAccMMA7660
    GroveAccMMA7660 *GroveAccMMA7660_ins = new GroveAccMMA7660(4,5);
    memset(arg_types, TYPE_NONE, MAX_INPUT_ARG_LEN);
    rpc_server_register_method("GroveAccMMA7660", "accelerometer", METHOD_READ, __grove_acc_mma7660_read_accelerometer, GroveAccMMA7660_ins, arg_types);
    memset(arg_types, TYPE_NONE, MAX_INPUT_ARG_LEN);
    rpc_server_register_method("GroveAccMMA7660", "shaked", METHOD_READ, __grove_acc_mma7660_read_shaked, GroveAccMMA7660_ins, arg_types);


    //GroveIRDistanceInterrupter111
    GroveIRDistanceInterrupter *GroveIRDistanceInterrupter111_ins = new GroveIRDistanceInterrupter(14);
    memset(arg_types, TYPE_NONE, MAX_INPUT_ARG_LEN);
    rpc_server_register_method("GroveIRDistanceInterrupter111", "approach", METHOD_READ, __grove_ir_distance_intr_read_approach, GroveIRDistanceInterrupter111_ins, arg_types);


    GroveIRDistanceInterrupter111_ins->attach_event_reporter(rpc_server_event_report);

    //Grove_Example1
    GroveExample *Grove_Example1_ins = new GroveExample(4,5);
    memset(arg_types, TYPE_NONE, MAX_INPUT_ARG_LEN);
    rpc_server_register_method("Grove_Example1", "temp", METHOD_READ, __grove_example_read_temp, Grove_Example1_ins, arg_types);
    memset(arg_types, TYPE_NONE, MAX_INPUT_ARG_LEN);
    rpc_server_register_method("Grove_Example1", "uint8_value", METHOD_READ, __grove_example_read_uint8_value, Grove_Example1_ins, arg_types);
    memset(arg_types, TYPE_NONE, MAX_INPUT_ARG_LEN);
    arg_types[0] = TYPE_INT;
    rpc_server_register_method("Grove_Example1", "with_arg", METHOD_READ, __grove_example_read_with_arg, Grove_Example1_ins, arg_types);
    memset(arg_types, TYPE_NONE, MAX_INPUT_ARG_LEN);
    rpc_server_register_method("Grove_Example1", "compass", METHOD_READ, __grove_example_read_compass, Grove_Example1_ins, arg_types);
    memset(arg_types, TYPE_NONE, MAX_INPUT_ARG_LEN);
    rpc_server_register_method("Grove_Example1", "acc", METHOD_READ, __grove_example_read_acc, Grove_Example1_ins, arg_types);
    memset(arg_types, TYPE_NONE, MAX_INPUT_ARG_LEN);
    rpc_server_register_method("Grove_Example1", "humidity", METHOD_READ, __grove_example_read_humidity, Grove_Example1_ins, arg_types);

    memset(arg_types, TYPE_NONE, MAX_INPUT_ARG_LEN);
    arg_types[0] = TYPE_INT;
    arg_types[1] = TYPE_FLOAT;
    arg_types[2] = TYPE_UINT32;
    rpc_server_register_method("Grove_Example1", "multi_value", METHOD_WRITE, __grove_example_write_multi_value, Grove_Example1_ins, arg_types);
    memset(arg_types, TYPE_NONE, MAX_INPUT_ARG_LEN);
    arg_types[0] = TYPE_UINT8;
    rpc_server_register_method("Grove_Example1", "acc_mode", METHOD_WRITE, __grove_example_write_acc_mode, Grove_Example1_ins, arg_types);
    memset(arg_types, TYPE_NONE, MAX_INPUT_ARG_LEN);
    arg_types[0] = TYPE_FLOAT;
    rpc_server_register_method("Grove_Example1", "float_value", METHOD_WRITE, __grove_example_write_float_value, Grove_Example1_ins, arg_types);

    Grove_Example1_ins->attach_event_reporter(rpc_server_event_report);

    //GroveMoisture
    GroveMoisture *GroveMoisture_ins = new GroveMoisture(17);
    memset(arg_types, TYPE_NONE, MAX_INPUT_ARG_LEN);
    rpc_server_register_method("GroveMoisture", "moisture", METHOD_READ, __grove_moisture_read_moisture, GroveMoisture_ins, arg_types);


    //GroveBaroBMP085
    GroveBaroBMP085 *GroveBaroBMP085_ins = new GroveBaroBMP085(4,5);
    memset(arg_types, TYPE_NONE, MAX_INPUT_ARG_LEN);
    rpc_server_register_method("GroveBaroBMP085", "temperature", METHOD_READ, __grove_baro_bmp085_read_temperature, GroveBaroBMP085_ins, arg_types);
    memset(arg_types, TYPE_NONE, MAX_INPUT_ARG_LEN);
    rpc_server_register_method("GroveBaroBMP085", "altitude", METHOD_READ, __grove_baro_bmp085_read_altitude, GroveBaroBMP085_ins, arg_types);
    memset(arg_types, TYPE_NONE, MAX_INPUT_ARG_LEN);
    rpc_server_register_method("GroveBaroBMP085", "pressure", METHOD_READ, __grove_baro_bmp085_read_pressure, GroveBaroBMP085_ins, arg_types);


    //Grove_DigitalLight1
    GroveDigitalLight *Grove_DigitalLight1_ins = new GroveDigitalLight(4,5);
    memset(arg_types, TYPE_NONE, MAX_INPUT_ARG_LEN);
    rpc_server_register_method("Grove_DigitalLight1", "lux", METHOD_READ, __grove_digital_light_read_lux, Grove_DigitalLight1_ins, arg_types);


    //Grove_Gyro_ITG3200
    GroveGyroITG3200 *Grove_Gyro_ITG3200_ins = new GroveGyroITG3200(4,5);
    memset(arg_types, TYPE_NONE, MAX_INPUT_ARG_LEN);
    rpc_server_register_method("Grove_Gyro_ITG3200", "temperature", METHOD_READ, __grove_gyro_itg3200_read_temperature, Grove_Gyro_ITG3200_ins, arg_types);
    memset(arg_types, TYPE_NONE, MAX_INPUT_ARG_LEN);
    rpc_server_register_method("Grove_Gyro_ITG3200", "gyro", METHOD_READ, __grove_gyro_itg3200_read_gyro, Grove_Gyro_ITG3200_ins, arg_types);

    memset(arg_types, TYPE_NONE, MAX_INPUT_ARG_LEN);
    rpc_server_register_method("Grove_Gyro_ITG3200", "zerocalibrate", METHOD_WRITE, __grove_gyro_itg3200_write_zerocalibrate, Grove_Gyro_ITG3200_ins, arg_types);
}

void print_well_known()
{
    writer_print(TYPE_STRING, "[");
    writer_print(TYPE_STRING, "\"GET " OTA_SERVER_URL_PREFIX "/node/GroveAccMMA7660/accelerometer -> float: *ax, float: *ay, float: *az\",");
    writer_print(TYPE_STRING, "\"GET " OTA_SERVER_URL_PREFIX "/node/GroveAccMMA7660/shaked -> uint8_t: *shaked\",");
    writer_print(TYPE_STRING, "\"GET " OTA_SERVER_URL_PREFIX "/node/GroveIRDistanceInterrupter111/approach -> uint8_t: *approach\",");
    writer_print(TYPE_STRING, "\"HasEvent GroveIRDistanceInterrupter111\",");
    writer_print(TYPE_STRING, "\"GET " OTA_SERVER_URL_PREFIX "/node/Grove_Example1/temp -> int: *temp\",");
    writer_print(TYPE_STRING, "\"GET " OTA_SERVER_URL_PREFIX "/node/Grove_Example1/uint8_value -> uint8_t: *value\",");
    writer_print(TYPE_STRING, "\"GET " OTA_SERVER_URL_PREFIX "/node/Grove_Example1/with_arg/int_arg -> float: *cx, float: *cy, float: *cz, int: *degree\",");
    writer_print(TYPE_STRING, "\"GET " OTA_SERVER_URL_PREFIX "/node/Grove_Example1/compass -> float: *cx, float: *cy, float: *cz, int: *degree\",");
    writer_print(TYPE_STRING, "\"GET " OTA_SERVER_URL_PREFIX "/node/Grove_Example1/acc -> float: *ax, float: *ay, float: *az\",");
    writer_print(TYPE_STRING, "\"GET " OTA_SERVER_URL_PREFIX "/node/Grove_Example1/humidity -> float: *humidity\",");
    writer_print(TYPE_STRING, "\"POST " OTA_SERVER_URL_PREFIX "/node/Grove_Example1/multi_value <- int a, float b, uint32_t c\",");
    writer_print(TYPE_STRING, "\"POST " OTA_SERVER_URL_PREFIX "/node/Grove_Example1/acc_mode <- uint8_t mode\",");
    writer_print(TYPE_STRING, "\"POST " OTA_SERVER_URL_PREFIX "/node/Grove_Example1/float_value <- float f\",");
    writer_print(TYPE_STRING, "\"HasEvent Grove_Example1\",");
    writer_print(TYPE_STRING, "\"GET " OTA_SERVER_URL_PREFIX "/node/GroveMoisture/moisture -> uint16_t: *moisture\",");
    writer_print(TYPE_STRING, "\"GET " OTA_SERVER_URL_PREFIX "/node/GroveBaroBMP085/temperature -> float: *temperature\",");
    writer_print(TYPE_STRING, "\"GET " OTA_SERVER_URL_PREFIX "/node/GroveBaroBMP085/altitude -> float: *altitude\",");
    writer_print(TYPE_STRING, "\"GET " OTA_SERVER_URL_PREFIX "/node/GroveBaroBMP085/pressure -> int32_t: *pressure\",");
    writer_print(TYPE_STRING, "\"GET " OTA_SERVER_URL_PREFIX "/node/Grove_DigitalLight1/lux -> uint32_t: *lux\",");
    writer_print(TYPE_STRING, "\"GET " OTA_SERVER_URL_PREFIX "/node/Grove_Gyro_ITG3200/temperature -> float: *temp\",");
    writer_print(TYPE_STRING, "\"GET " OTA_SERVER_URL_PREFIX "/node/Grove_Gyro_ITG3200/gyro -> float: *gx, float: *gy, float: *gz\",");
    writer_print(TYPE_STRING, "\"POST " OTA_SERVER_URL_PREFIX "/node/Grove_Gyro_ITG3200/zerocalibrate <- void\"");
    writer_print(TYPE_STRING, "]");
}
