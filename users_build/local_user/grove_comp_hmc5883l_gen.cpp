#include "grove_comp_hmc5883l_gen.h"
#include "rpc_server.h"
#include "rpc_stream.h"

void __grove_comp_hmc5883l_read_compass_heading(void *class_ptr, void *input)
{
    GroveCompass *grove = (GroveCompass *)class_ptr;
    uint8_t *arg_ptr = (uint8_t *)input;
    float heading_deg;
    

    if(grove->read_compass_heading(&heading_deg))
    {
        writer_print(TYPE_STRING, "{");
        writer_print(TYPE_STRING, "\"heading_deg\":");
        writer_print(TYPE_FLOAT, &heading_deg);
        writer_print(TYPE_STRING, "}");
    }else
    {
        writer_print(TYPE_STRING, "null");
    }
}

