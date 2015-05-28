#include "grove_example_gen.h"
#include "rpc_server.h"
#include "rpc_stream.h"

void __grove_example_read_temp(void *class_ptr, void *input)
{
    GroveExample *grove = (GroveExample *)class_ptr;
    uint8_t *arg_ptr = (uint8_t *)input;
    int temp;
    

    if(grove->read_temp(&temp))
    {
        writer_print(TYPE_INT, &temp);
    }else
    {
        writer_print(TYPE_STRING, "Failed");
    }
}

void __grove_example_read_uint8_value(void *class_ptr, void *input)
{
    GroveExample *grove = (GroveExample *)class_ptr;
    uint8_t *arg_ptr = (uint8_t *)input;
    uint8_t value;
    

    if(grove->read_uint8_value(&value))
    {
        writer_print(TYPE_UINT8, &value);
    }else
    {
        writer_print(TYPE_STRING, "Failed");
    }
}

void __grove_example_read_with_arg(void *class_ptr, void *input)
{
    GroveExample *grove = (GroveExample *)class_ptr;
    uint8_t *arg_ptr = (uint8_t *)input;
    float cx;
    float cy;
    float cz;
    int degree;
    int arg;
    
    arg = *((int *)arg_ptr); arg_ptr += sizeof(int);

    if(grove->read_with_arg(&cx,&cy,&cz,&degree,arg))
    {
        writer_print(TYPE_FLOAT, &cx, true);
        writer_print(TYPE_FLOAT, &cy, true);
        writer_print(TYPE_FLOAT, &cz, true);
        writer_print(TYPE_INT, &degree);
    }else
    {
        writer_print(TYPE_STRING, "Failed");
    }
}

void __grove_example_read_compass(void *class_ptr, void *input)
{
    GroveExample *grove = (GroveExample *)class_ptr;
    uint8_t *arg_ptr = (uint8_t *)input;
    float cx;
    float cy;
    float cz;
    int degree;
    

    if(grove->read_compass(&cx,&cy,&cz,&degree))
    {
        writer_print(TYPE_FLOAT, &cx, true);
        writer_print(TYPE_FLOAT, &cy, true);
        writer_print(TYPE_FLOAT, &cz, true);
        writer_print(TYPE_INT, &degree);
    }else
    {
        writer_print(TYPE_STRING, "Failed");
    }
}

void __grove_example_read_acc(void *class_ptr, void *input)
{
    GroveExample *grove = (GroveExample *)class_ptr;
    uint8_t *arg_ptr = (uint8_t *)input;
    float ax;
    float ay;
    float az;
    

    if(grove->read_acc(&ax,&ay,&az))
    {
        writer_print(TYPE_FLOAT, &ax, true);
        writer_print(TYPE_FLOAT, &ay, true);
        writer_print(TYPE_FLOAT, &az);
    }else
    {
        writer_print(TYPE_STRING, "Failed");
    }
}

void __grove_example_read_humidity(void *class_ptr, void *input)
{
    GroveExample *grove = (GroveExample *)class_ptr;
    uint8_t *arg_ptr = (uint8_t *)input;
    float humidity;
    

    if(grove->read_humidity(&humidity))
    {
        writer_print(TYPE_FLOAT, &humidity);
    }else
    {
        writer_print(TYPE_STRING, "Failed");
    }
}

void __grove_example_write_multi_value(void *class_ptr, void *input)
{
    GroveExample *grove = (GroveExample *)class_ptr;
    uint8_t *arg_ptr = (uint8_t *)input;
    int a;
    float b;
    uint32_t c;
    
    a = *((int *)arg_ptr); arg_ptr += sizeof(int);
    b = *((float *)arg_ptr); arg_ptr += sizeof(float);
    c = *((uint32_t *)arg_ptr); arg_ptr += sizeof(uint32_t);

    if(grove->write_multi_value(a,b,c))
        writer_print(TYPE_STRING, "OK");
    else
        writer_print(TYPE_STRING, "Failed");
}

void __grove_example_write_acc_mode(void *class_ptr, void *input)
{
    GroveExample *grove = (GroveExample *)class_ptr;
    uint8_t *arg_ptr = (uint8_t *)input;
    uint8_t mode;
    
    mode = *((uint8_t *)arg_ptr); arg_ptr += sizeof(uint8_t);

    if(grove->write_acc_mode(mode))
        writer_print(TYPE_STRING, "OK");
    else
        writer_print(TYPE_STRING, "Failed");
}

void __grove_example_write_float_value(void *class_ptr, void *input)
{
    GroveExample *grove = (GroveExample *)class_ptr;
    uint8_t *arg_ptr = (uint8_t *)input;
    float f;
    
    f = *((float *)arg_ptr); arg_ptr += sizeof(float);

    if(grove->write_float_value(f))
        writer_print(TYPE_STRING, "OK");
    else
        writer_print(TYPE_STRING, "Failed");
}

