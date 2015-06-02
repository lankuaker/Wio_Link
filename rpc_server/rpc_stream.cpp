

#include "rpc_stream.h"
#include "network.h"

//extern Serial pc;

void stream_init()
{

}

char stream_read()
{
    if (rx_stream_buffer->size() > 0)
    {
        char c;
        noInterrupts();
        rx_stream_buffer->read(&c,1);
        interrupts();
        return c;
    } else return NULL;
}

bool stream_write(char c)
{
    network_putc(c);
    return true;
}

int stream_available()
{
    noInterrupts();
    size_t sz = rx_stream_buffer->size();
    interrupts();
    return sz;
}

bool stream_write_string(char *str, int len)
{
    /*for (int i = 0; str[i] && i < len; i++)
    {
        stream_write(str[i]);
    }*/
    network_puts(str, len);
    return true;
}

void writer_print(type_t type, const void *data, bool append_comma)
{
    char buff[32];
    switch (type)
    {
        case TYPE_BOOL:
        case TYPE_UINT8:
            sprintf(buff, "%hhu", *(uint8_t *)data);
            break;
        case TYPE_UINT16:
            sprintf(buff, "%u", *(uint16_t *)data);
            break;
        case TYPE_UINT32:
            sprintf(buff, "%lu", *(uint32_t *)data);
            break;
        case TYPE_INT8:
            sprintf(buff, "%hhd", *(int8_t *)data);
            break;
        case TYPE_INT:
            sprintf(buff, "%d", *(int *)data);
            break;
        case TYPE_INT16:
            sprintf(buff, "%d", *(int16_t *)data);
            break;
        case TYPE_INT32:
            sprintf(buff, "%ld", *(int32_t *)data);
            break;
        case TYPE_FLOAT:
            //sprintf(buff, "%f", *(float *)data);
            dtostrf((*(float *)data), NULL, 2, buff);
            break;
        case TYPE_STRING:
            stream_write_string((char *)data, strlen(data));
            return;
        default:
            break;
    }
    stream_write_string(buff, strlen(buff));
    if(append_comma) stream_write(',');
}

void response_msg_open(char *msg_type)
{
    char *msg1 = "{\"msg_type\":\"";
    char *msg2 = "\", \"msg\":";

    stream_write_string(msg1, strlen(msg1));
    stream_write_string(msg_type, strlen(msg_type));
    stream_write_string(msg2, strlen(msg2));
}


void response_msg_close()
{
    char *msg3 = "}\r\n";

    stream_write_string(msg3, strlen(msg3));
}

