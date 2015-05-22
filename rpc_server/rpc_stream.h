
#ifndef __STREAMxxx_H__
#define __STREAMxxx_H__

#include "suli2.h"
#include "rpc_server.h"

void stream_init();

char stream_read();

bool stream_write(char c);

void writer_print(type_t type, const void *data, bool append_comma = false);

void response_msg_open(char *msg_type);

void response_msg_close();

#endif
