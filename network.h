


#ifndef __NETWORK111_H__
#define __NETWORK111_H__

#include "Arduino.h"
#include "circular_buffer.h"

enum
{
    WAIT_CONN_DONE, DIED_IN_CONN, CONNECTED, WAIT_HELLO_DONE, KEEP_ALIVE, DIED_IN_HELLO
};
extern uint8_t main_conn_status;

void establish_network();
void network_putc(char c);
void network_puts(char *data, int len);

extern CircularBuffer *rx_stream_buffer;


#endif
