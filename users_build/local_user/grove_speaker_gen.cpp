#include "grove_speaker_gen.h"
#include "rpc_server.h"
#include "rpc_stream.h"

void __grove_speaker_write_sound(void *class_ptr, void *input)
{
    GroveSpeaker *grove = (GroveSpeaker *)class_ptr;
    uint8_t *arg_ptr = (uint8_t *)input;
    int freq;
    int duration;
    
    freq = *((int *)arg_ptr); arg_ptr += sizeof(int);
    duration = *((int *)arg_ptr); arg_ptr += sizeof(int);

    if(grove->write_sound(freq,duration))
        writer_print(TYPE_STRING, "\"OK\"");
    else
        writer_print(TYPE_STRING, "\"Failed\"");
}

