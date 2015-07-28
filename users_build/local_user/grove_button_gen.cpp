#include "grove_button_gen.h"
#include "rpc_server.h"
#include "rpc_stream.h"

void __grove_button_read_pressed(void *class_ptr, void *input)
{
    GroveButton *grove = (GroveButton *)class_ptr;
    uint8_t *arg_ptr = (uint8_t *)input;
    uint8_t pressed;
    

    if(grove->read_pressed(&pressed))
    {
        writer_print(TYPE_STRING, "{");
        writer_print(TYPE_STRING, "\"pressed\":");
        writer_print(TYPE_UINT8, &pressed);
        writer_print(TYPE_STRING, "}");
    }else
    {
        writer_print(TYPE_STRING, "null");
    }
}

