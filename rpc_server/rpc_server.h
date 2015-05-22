

#ifndef __RPC_SERVER_H__
#define __RPC_SERVER_H__

#include "suli2.h"

typedef enum
{
    TYPE_NONE, TYPE_BOOL, TYPE_UINT8, TYPE_INT8, TYPE_UINT16, TYPE_INT16, TYPE_INT, TYPE_UINT32, TYPE_INT32, TYPE_FLOAT, TYPE_STRING
}type_t;

typedef enum
{
    METHOD_READ, METHOD_WRITE
}method_dir_t;

enum
{
    PARSE_REQ_TYPE, PARSE_GROVE_NAME, PARSE_METHOD, CHECK_POST_ARGS, PRE_PARSE_ARGS, PARSE_ARGS, PARSE_CALL, DIVE_INTO_OTA
};

enum
{
    REQ_GET, REQ_POST, REQ_OTA
};

typedef void (*method_ptr_t)(void *class_ptr, void *input);

struct resource_s;

#define MAX_INPUT_ARG_LEN               4
typedef struct resource_s
{
    char                   *grove_name;
    char                   *method_name;
    method_dir_t           rw;
    method_ptr_t           method_ptr;
    void                   *class_ptr;
    uint8_t                arg_types[MAX_INPUT_ARG_LEN];
    struct resource_s      *next;
}resource_t;

struct event_s;
typedef struct event_s
{
    struct event_s          *prev;
    struct event_s          *next;
    char                    *event_name;
    uint32_t                event_data;
}event_t;

void rpc_server_init();

void rpc_server_register_method(char *grove_name, char *method_name, method_dir_t rw, method_ptr_t ptr, void *class_ptr, uint8_t *arg_types);

void rpc_server_register_resources();

void rpc_server_loop();

void rpc_server_event_report(char *event_name, uint32_t event_data);
bool rpc_server_event_queue_pop(event_t *event);

#endif
