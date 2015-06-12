#include "grove_digital_light_gen.h"
#include "rpc_server.h"
#include "rpc_stream.h"

void __grove_digital_light_read_lux(void *class_ptr, void *input)
{
    GroveDigitalLight *grove = (GroveDigitalLight *)class_ptr;
    uint8_t *arg_ptr = (uint8_t *)input;
    uint32_t lux;
    

    if(grove->read_lux(&lux))
    {
        writer_print(TYPE_STRING, "{");
        writer_print(TYPE_STRING, "\"lux\":");
        writer_print(TYPE_UINT32, &lux);
        writer_print(TYPE_STRING, "}");
    }else
    {
        writer_print(TYPE_STRING, "null");
    }
}

void __grove_digital_light_write_setup(void *class_ptr, void *input)
{
    GroveDigitalLight *grove = (GroveDigitalLight *)class_ptr;
    uint8_t *arg_ptr = (uint8_t *)input;
    void;
    
