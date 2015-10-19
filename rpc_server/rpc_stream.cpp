/*
 * rpc_stream.cpp
 *
 * Copyright (c) 2012 seeed technology inc.
 * Website    : www.seeed.cc
 * Author     : Jack Shao (jacky.shaoxg@gmail.com)
 *
 * The MIT License (MIT)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "rpc_stream.h"
#include "network.h"

//extern Serial pc;

void stream_init(int stream_num)
{

}

char stream_read(int stream_num)
{
    CircularBuffer *rx_buffer = (stream_num == STREAM_DATA) ? data_stream_rx_buffer : ota_stream_rx_buffer;
    
    if (rx_buffer->size() > 0)
    {
        char c;
        noInterrupts();
        rx_buffer->read(&c,1);
        interrupts();
        return c;
    } else return NULL;
}

bool stream_write(int stream_num, char c)
{
    CircularBuffer *tx_buffer = (stream_num == STREAM_DATA) ? data_stream_tx_buffer : ota_stream_tx_buffer;
    network_putc(tx_buffer, c);
    return true;
}

int stream_available(int stream_num)
{
    CircularBuffer *rx_buffer = (stream_num == STREAM_DATA) ? data_stream_rx_buffer : ota_stream_rx_buffer;
    
    noInterrupts();
    size_t sz = rx_buffer->size();
    interrupts();
    return sz;
}

bool stream_write_string(int stream_num, char *str, int len)
{
    CircularBuffer *tx_buffer = (stream_num == STREAM_DATA) ? data_stream_tx_buffer : ota_stream_tx_buffer;
    
    /*for (int i = 0; str[i] && i < len; i++)
    {
        stream_write(str[i]);
    }*/
    network_puts(tx_buffer, str, len);
    return true;
}

void stream_print(int stream_num, type_t type, const void *data, bool append_comma)
{
    char buff[32];
    switch (type)
    {
        case TYPE_BOOL:
        case TYPE_UINT8:
            sprintf(buff, "%u", *(uint8_t *)data);
            break;
        case TYPE_UINT16:
            sprintf(buff, "%u", *(uint16_t *)data);
            break;
        case TYPE_UINT32:
            sprintf(buff, "%lu", *(uint32_t *)data);
            break;
        case TYPE_INT8:
            sprintf(buff, "%d", *(int8_t *)data);
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
            stream_write_string(stream_num, (char *)data, strlen(data));
            return;
        default:
            break;
    }
    stream_write_string(stream_num, buff, strlen(buff));
    if(append_comma) stream_write(stream_num, ',');
}

void writer_print(type_t type, const void *data, bool append_comma)
{
    stream_print(STREAM_DATA, type, data, append_comma);
}

void response_msg_open(int stream_num, char *msg_type)
{
    char *msg1 = "{\"msg_type\":\"";
    char *msg2 = "\", \"msg\":";

    stream_write_string(stream_num, msg1, strlen(msg1));
    stream_write_string(stream_num, msg_type, strlen(msg_type));
    stream_write_string(stream_num, msg2, strlen(msg2));
}

void response_msg_append_205(int stream_num)
{
    char *msg = ", \"status\": 205";
    stream_write_string(stream_num, msg, strlen(msg));
}

void response_msg_close(int stream_num)
{
    char *msg3 = "}\r\n";

    stream_write_string(stream_num, msg3, strlen(msg3));
}

