#include "grove_baro_bmp085_gen.h"
#include "rpc_server.h"
#include "rpc_stream.h"

void __grove_baro_bmp085_read_temperature(void *class_ptr, void *input)
{
    GroveBaroBMP085 *grove = (GroveBaroBMP085 *)class_ptr;
    uint8_t *arg_ptr = (uint8_t *)input;
    float temperature;
    

    if(grove->read_temperature(&temperature))
    {
        writer_print(TYPE_STRING, "{");
        writer_print(TYPE_STRING, "\"temperature\":");
        writer_print(TYPE_FLOAT, &temperature);
        writer_print(TYPE_STRING, "}");
    }else
    {
        writer_print(TYPE_STRING, "null");
    }
}

void __grove_baro_bmp085_read_altitude(void *class_ptr, void *input)
{
    GroveBaroBMP085 *grove = (GroveBaroBMP085 *)class_ptr;
    uint8_t *arg_ptr = (uint8_t *)input;
    float altitude;
    

    if(grove->read_altitude(&altitude))
    {
        writer_print(TYPE_STRING, "{");
        writer_print(TYPE_STRING, "\"altitude\":");
        writer_print(TYPE_FLOAT, &altitude);
        writer_print(TYPE_STRING, "}");
    }else
    {
        writer_print(TYPE_STRING, "null");
    }
}

void __grove_baro_bmp085_read_pressure(void *class_ptr, void *input)
{
    GroveBaroBMP085 *grove = (GroveBaroBMP085 *)class_ptr;
    uint8_t *arg_ptr = (uint8_t *)input;
    int32_t pressure;
    

    if(grove->read_pressure(&pressure))
    {
        writer_print(TYPE_STRING, "{");
        writer_print(TYPE_STRING, "\"pressure\":");
        writer_print(TYPE_INT32, &pressure);
        writer_print(TYPE_STRING, "}");
    }else
    {
        writer_print(TYPE_STRING, "null");
    }
}

