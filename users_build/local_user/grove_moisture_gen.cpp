#include "grove_moisture_gen.h"
#include "rpc_server.h"
#include "rpc_stream.h"

void __grove_moisture_read_moisture(void *class_ptr, void *input)
{
    GroveMoisture *grove = (GroveMoisture *)class_ptr;
    uint8_t *arg_ptr = (uint8_t *)input;
    uint16_t moisture;
    

    if(grove->read_moisture(&moisture))
    {
        writer_print(TYPE_STRING, "{");
        writer_print(TYPE_STRING, "\"moisture\":");
        writer_print(TYPE_UINT16, &moisture);
        writer_print(TYPE_STRING, "}");
    }else
    {
        writer_print(TYPE_STRING, "null");
    }
}

