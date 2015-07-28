#include "grove_magnetic_switch_gen.h"
#include "rpc_server.h"
#include "rpc_stream.h"

void __grove_magnetic_switch_read_approach(void *class_ptr, void *input)
{
    GroveMagneticSwitch *grove = (GroveMagneticSwitch *)class_ptr;
    uint8_t *arg_ptr = (uint8_t *)input;
    uint8_t mag_approach;
    

    if(grove->read_approach(&mag_approach))
    {
        writer_print(TYPE_STRING, "{");
        writer_print(TYPE_STRING, "\"mag_approach\":");
        writer_print(TYPE_UINT8, &mag_approach);
        writer_print(TYPE_STRING, "}");
    }else
    {
        writer_print(TYPE_STRING, "null");
    }
}

