#include "grove_gyro_itg3200_gen.h"
#include "rpc_server.h"
#include "rpc_stream.h"

void __grove_gyro_itg3200_read_temperature(void *class_ptr, void *input)
{
    GroveGyroITG3200 *grove = (GroveGyroITG3200 *)class_ptr;
    uint8_t *arg_ptr = (uint8_t *)input;
    float temp;
    

    if(grove->read_temperature(&temp))
    {
        writer_print(TYPE_STRING, "{");
        writer_print(TYPE_STRING, "\"temp\":");
        writer_print(TYPE_FLOAT, &temp);
        writer_print(TYPE_STRING, "}");
    }else
    {
        writer_print(TYPE_STRING, "null");
    }
}

void __grove_gyro_itg3200_read_gyro(void *class_ptr, void *input)
{
    GroveGyroITG3200 *grove = (GroveGyroITG3200 *)class_ptr;
    uint8_t *arg_ptr = (uint8_t *)input;
    float gx;
    float gy;
    float gz;
    

    if(grove->read_gyro(&gx,&gy,&gz))
    {
        writer_print(TYPE_STRING, "{");
        writer_print(TYPE_STRING, "\"gx\":");
        writer_print(TYPE_FLOAT, &gx, true);
        writer_print(TYPE_STRING, "\"gy\":");
        writer_print(TYPE_FLOAT, &gy, true);
        writer_print(TYPE_STRING, "\"gz\":");
        writer_print(TYPE_FLOAT, &gz);
        writer_print(TYPE_STRING, "}");
    }else
    {
        writer_print(TYPE_STRING, "null");
    }
}

void __grove_gyro_itg3200_write_zerocalibrate(void *class_ptr, void *input)
{
    GroveGyroITG3200 *grove = (GroveGyroITG3200 *)class_ptr;
    uint8_t *arg_ptr = (uint8_t *)input;
    

    if(grove->write_zerocalibrate())
        writer_print(TYPE_STRING, "\"OK\"");
    else
        writer_print(TYPE_STRING, "\"Failed\"");
}

