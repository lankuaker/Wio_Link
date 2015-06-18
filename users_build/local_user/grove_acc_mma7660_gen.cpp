#include "grove_acc_mma7660_gen.h"
#include "rpc_server.h"
#include "rpc_stream.h"

void __grove_acc_mma7660_read_accelerometer(void *class_ptr, void *input)
{
    GroveAccMMA7660 *grove = (GroveAccMMA7660 *)class_ptr;
    uint8_t *arg_ptr = (uint8_t *)input;
    float ax;
    float ay;
    float az;
    

    if(grove->read_accelerometer(&ax,&ay,&az))
    {
        writer_print(TYPE_STRING, "{");
        writer_print(TYPE_STRING, "\"ax\":");
        writer_print(TYPE_FLOAT, &ax, true);
        writer_print(TYPE_STRING, "\"ay\":");
        writer_print(TYPE_FLOAT, &ay, true);
        writer_print(TYPE_STRING, "\"az\":");
        writer_print(TYPE_FLOAT, &az);
        writer_print(TYPE_STRING, "}");
    }else
    {
        writer_print(TYPE_STRING, "null");
    }
}

void __grove_acc_mma7660_read_shaked(void *class_ptr, void *input)
{
    GroveAccMMA7660 *grove = (GroveAccMMA7660 *)class_ptr;
    uint8_t *arg_ptr = (uint8_t *)input;
    uint8_t shaked;
    

    if(grove->read_shaked(&shaked))
    {
        writer_print(TYPE_STRING, "{");
        writer_print(TYPE_STRING, "\"shaked\":");
        writer_print(TYPE_UINT8, &shaked);
        writer_print(TYPE_STRING, "}");
    }else
    {
        writer_print(TYPE_STRING, "null");
    }
}

