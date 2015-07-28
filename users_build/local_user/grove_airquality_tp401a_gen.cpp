#include "grove_airquality_tp401a_gen.h"
#include "rpc_server.h"
#include "rpc_stream.h"

void __grove_airquality_tp401a_read_quality(void *class_ptr, void *input)
{
    GroveAirquality *grove = (GroveAirquality *)class_ptr;
    uint8_t *arg_ptr = (uint8_t *)input;
    int quality;
    

    if(grove->read_quality(&quality))
    {
        writer_print(TYPE_STRING, "{");
        writer_print(TYPE_STRING, "\"quality\":");
        writer_print(TYPE_INT, &quality);
        writer_print(TYPE_STRING, "}");
    }else
    {
        writer_print(TYPE_STRING, "null");
    }
}

