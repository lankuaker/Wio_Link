/*
 * network.h
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


#ifndef __NETWORK111_H__
#define __NETWORK111_H__

#include "Arduino.h"
#include "circular_buffer.h"

enum
{
    WAIT_CONFIG, WAIT_GET_IP, DIED_IN_GET_IP, WAIT_CONN_DONE, DIED_IN_CONN, CONNECTED, WAIT_HELLO_DONE, KEEP_ALIVE, DIED_IN_HELLO
};
extern uint8_t conn_status[2];
extern struct espconn tcp_conn[2];

void network_setup();
void network_normal_mode(int config_flag);
void network_config_mode();
void network_putc(CircularBuffer *tx_buffer, char c);
void network_puts(CircularBuffer *tx_buffer, char *data, int len);

extern CircularBuffer *data_stream_rx_buffer;
extern CircularBuffer *data_stream_tx_buffer;
extern CircularBuffer *ota_stream_rx_buffer ;
extern CircularBuffer *ota_stream_tx_buffer ;

extern uint32_t keepalive_last_recv_time[2];


#endif
