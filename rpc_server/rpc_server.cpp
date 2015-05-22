

#include "stdlib.h"
#include "rpc_stream.h"
#include "rpc_server.h"


resource_t *p_first_resource;
resource_t *p_cur_resource;

event_t *p_event_queue_head;
event_t *p_event_queue_tail;

static int parse_stage;

void rpc_server_init()
{
    //init rpc stream
    stream_init();
    //init rpc server
    p_first_resource = p_cur_resource = NULL;
    p_event_queue_head = p_event_queue_tail = NULL;
    parse_stage = PARSE_REQ_TYPE;

    rpc_server_register_resources();
    printf("rpc server init done!\n");

}


void rpc_server_register_method(char *grove_name, char *method_name, method_dir_t rw, method_ptr_t ptr, void *class_ptr, uint8_t *arg_types)
{
    resource_t *p_res = (resource_t*)malloc(sizeof(resource_t));
    if (!p_res) return;

    if (p_first_resource == NULL)
    {
        p_first_resource = p_res;
        p_cur_resource = p_res;
    } else
    {
        p_cur_resource->next = p_res;
        p_cur_resource = p_res;
    }

    p_cur_resource->grove_name  = grove_name;
    p_cur_resource->method_name = method_name;
    p_cur_resource->rw          = rw;
    p_cur_resource->method_ptr  = ptr;
    p_cur_resource->class_ptr   = class_ptr;
    p_cur_resource->next = NULL;
    memcpy(p_cur_resource->arg_types, arg_types, sizeof(p_cur_resource->arg_types));

}

resource_t* __find_resource(char *name, char *method, int req_type)
{
    resource_t *ptr;
    for (ptr = p_first_resource; ptr; ptr = ptr->next)
    {
        if (strncmp(name, ptr->grove_name, 33) == 0 && strncmp(method, ptr->method_name, 33) == 0
            && req_type == ptr->rw)
        {
            return ptr;
        }
    }
    return NULL;
}

int __convert_arg(uint8_t *arg_buff, char **buff, int type)
{
    int i;
    uint32_t ui;
    switch (type)
    {
        case TYPE_BOOL:
            {
                i = atoi((const char *)(*buff));
                ui = abs(i);
                memcpy(arg_buff, &ui, sizeof(bool));
                return sizeof(bool);
                break;
            }
        case TYPE_UINT8:
            {
                i = atoi((const char *)(*buff));
                ui = abs(i);
                memcpy(arg_buff, &ui, 1);
                return 1;
                break;
            }
        case TYPE_UINT16:
            {
                i = atoi((const char *)(*buff));
                ui = abs(i);
                memcpy(arg_buff, &ui, 2);
                return 2;
                break;
            }
        case TYPE_UINT32:
            {
                int32_t l = atol((const char *)(*buff));
                ui = abs(l);
                memcpy(arg_buff, &ui, 4);
                return 4;
                break;
            }
        case TYPE_INT8:
            {
                i = atoi((const char *)(*buff));
                char c = i;
                memcpy(arg_buff, &c, 1);
                return 1;
                break;
            }
        case TYPE_INT16:
            {
                i = atoi((const char *)(*buff));
                memcpy(arg_buff, &i, 2);
                return 2;
                break;
            }
        case TYPE_INT32:
            {
                int32_t l = atol((const char *)(*buff));
                memcpy(arg_buff, &l, 4);
                return 4;
                break;
            }
        case TYPE_INT:
            {
                int l = atol((const char *)(*buff));
                memcpy(arg_buff, &l, sizeof(int));
                return sizeof(int);
                break;
            }
        case TYPE_FLOAT:
            {
                float f = atof((const char *)(*buff));
                memcpy(arg_buff, &f, sizeof(float));
                return sizeof(float);
                break;
            }
        case TYPE_STRING:
            {
                memcpy(arg_buff, buff, 4);
                return 4;
                break;
            }
        default:
            break;
    }
    return 0;
}

static int req_type;

static char buff[33];
static int  offset = 0;
static int  arg_index = 0;
static char grove_name[33];
static char method_name[33];
static char ch;
static uint8_t arg_buff[4 * MAX_INPUT_ARG_LEN];
static int arg_offset;
static resource_t *p_resource;
static event_t event;

void rpc_server_loop()
{
    /* report event if event queue is not empty */
    while (rpc_server_event_queue_pop(&event))
    {
        response_msg_open("event");
        writer_print(TYPE_STRING, "{\"");
        writer_print(TYPE_STRING, event.event_name);
        writer_print(TYPE_STRING, "\":\"");
        writer_print(TYPE_UINT32, &event.event_data);
        writer_print(TYPE_STRING, "\"}");
        response_msg_close();
    }

    //writer_print(TYPE_INT, &parse_stage);
    switch (parse_stage)
    {
        case PARSE_REQ_TYPE:
            {
                bool parsed_req_type = false;

                buff[0] = buff[1]; buff[1] = buff[2]; buff[2] = buff[3];
                buff[3] = stream_read();
                //printf(" %s\r\n", buff);
                if (memcmp(buff, "GET", 3) == 0 || memcmp(buff, "get", 3) == 0)
                {
                    req_type = REQ_GET;
                    parsed_req_type = true;
                    response_msg_open("resp_get");
                }
                if (memcmp(buff, "POST", 4) == 0 || memcmp(buff, "post", 4) == 0)
                {
                    req_type = REQ_POST;
                    parsed_req_type = true;
                    stream_read();  //read " " out
                    response_msg_open("resp_post");
                }
                if (memcmp(buff, "OTA", 3) == 0 || memcmp(buff, "ota", 3) == 0)
                {
                    req_type = REQ_OTA;
                    parsed_req_type = true;
                    parse_stage = DIVE_INTO_OTA;
                    response_msg_open("ota_trig_ack");
                    response_msg_close();
                    break;
                }
                if (parsed_req_type)
                {
                    ch = stream_read();
                    if (ch != '/')
                    {
                        //error request format
                        writer_print(TYPE_STRING, "BAD REQUEST: missing root:'/'.");
                        response_msg_close();
                    } else
                    {
                        parse_stage = PARSE_GROVE_NAME;
                        p_resource = NULL;
                        offset = 0;
                    }
                }
                break;
            }
        case PARSE_GROVE_NAME:
            {
                ch = stream_read();
                if(ch == '\r' || ch == '\n')
                {
                    //TODO: get /.well-known
                    buff[offset] = '\0';
                    if(strcmp(buff, ".well-known") == 0)
                    {
                        writer_print(TYPE_STRING, "/.well-known is not implemented");
                        response_msg_close();
                        parse_stage = PARSE_REQ_TYPE;
                    }else
                    {
                        writer_print(TYPE_STRING, "BAD REQUEST: missing method name.");
                        response_msg_close();
                        parse_stage = PARSE_REQ_TYPE;
                    }
                }
                else if (ch != '/' && offset <= 31)
                {
                    buff[offset++] = ch;
                }else
                {
                    buff[offset] = '\0';
                    memcpy(grove_name, buff, offset+1);
                    while (ch != '/')
                    {
                        ch = stream_read();
                    }
                    parse_stage = PARSE_METHOD;
                    offset = 0;
                }
                break;
            }
        case PARSE_METHOD:
            {
                ch = stream_read();
                if (ch == '\r' || ch == '\n')
                {
                    buff[offset] = '\0';
                    memcpy(method_name, buff, offset + 1);
                    if(req_type == REQ_POST)
                        parse_stage = CHECK_POST_ARGS;
                    else
                        parse_stage = PARSE_CALL;
                }
                else if (ch == '/')
                {
                    buff[offset] = '\0';
                    memcpy(method_name, buff, offset + 1);
                    parse_stage = PRE_PARSE_ARGS;
                }
                else if (offset >= 32)
                {
                    buff[offset] = '\0';
                    memcpy(method_name, buff, offset + 1);
                    while (ch != '/' && ch != '\r' && ch != '\n')
                    {
                        ch = stream_read();
                    }
                    if (ch == '\r' || ch == '\n') parse_stage = PARSE_CALL;
                    else parse_stage = PRE_PARSE_ARGS;
                }
                else
                {
                    buff[offset++] = ch;
                }
                break;
            }
        case CHECK_POST_ARGS:
            {
                p_resource = __find_resource((char *)grove_name, (char *)method_name, req_type);
                if (!p_resource)
                {
                    writer_print(TYPE_STRING, "METHOD NOT FOUND");
                    response_msg_close();
                    parse_stage = PARSE_REQ_TYPE;
                    break;
                }
                if (p_resource->arg_types[0] != TYPE_NONE)
                {
                    writer_print(TYPE_STRING, "MISSING ARGS");
                    response_msg_close();
                    parse_stage = PARSE_REQ_TYPE;
                    break;
                }
                parse_stage = PARSE_CALL;
                break;
            }
        case PRE_PARSE_ARGS:
            {
                p_resource = __find_resource((char *)grove_name, (char *)method_name, req_type);
                if (!p_resource)
                {
                    writer_print(TYPE_STRING, "METHOD NOT FOUND");
                    response_msg_close();
                    parse_stage = PARSE_REQ_TYPE;
                    break;
                }
                parse_stage = PARSE_ARGS;
                arg_index = 0;
                arg_offset = 0;
                offset = 0;
                break;
            }
        case PARSE_ARGS:
            {
                ch = stream_read();
                if (ch == '\r' || ch == '\n' || ch == '/')
                {
                    buff[offset] = '\0';
                } else if (offset >= 32)
                {
                    buff[offset] = '\0';
                    while (ch != '/' && ch != '\r' && ch != '\n')
                    {
                        ch = stream_read();
                    }

                } else
                {
                    buff[offset++] = ch;
                }

                if (ch == '/')
                {
                    char *p = buff;
                    int len = __convert_arg(arg_buff + arg_offset, &p, p_resource->arg_types[arg_index++]);
                    arg_offset += len;
                    offset = 0;
                }
                if (ch == '\r' || ch == '\n')
                {
                    if ((arg_index<3 && p_resource->arg_types[arg_index+1] != TYPE_NONE) ||
                        (arg_index<=3 && p_resource->arg_types[arg_index] != TYPE_NONE && strlen(buff)<1))
                    {
                        writer_print(TYPE_STRING, "MISSING ARGS");
                        response_msg_close();
                        parse_stage = PARSE_REQ_TYPE;
                        break;
                    }
                    char *p = buff;
                    int len = __convert_arg(arg_buff + arg_offset, &p, p_resource->arg_types[arg_index++]);
                    arg_offset += len;
                    offset = 0;
                    parse_stage = PARSE_CALL;
                }
                break;
            }
        case PARSE_CALL:
            {
                if(!p_resource)
                    p_resource = __find_resource((char *)grove_name, (char *)method_name, req_type);

                if (!p_resource)
                {
                    writer_print(TYPE_STRING, "METHOD NOT FOUND");
                    response_msg_close();
                    parse_stage = PARSE_REQ_TYPE;
                    break;
                }
                writer_print(TYPE_STRING, "[");
                p_resource->method_ptr(p_resource->class_ptr, arg_buff);
                writer_print(TYPE_STRING, "]");
                response_msg_close();

                parse_stage = PARSE_REQ_TYPE;
                break;
            }
        case DIVE_INTO_OTA:
            {
                //TODO: refer to the ota related code here
                break;
            }
        default:
            break;
    }

}


void rpc_server_event_report(char *event_name, uint32_t event_data)
{
    __disable_irq();
    event_t *ev = (event_t *)malloc(sizeof(event_t));
    ev->event_name = event_name;
    ev->event_data = event_data;
    ev->prev = p_event_queue_tail;
    ev->next = NULL;
    p_event_queue_tail = ev;
    if (ev->prev == NULL)
    {
        p_event_queue_head = ev;
    }
    __enable_irq();
}

bool rpc_server_event_queue_pop(event_t *event)
{
    bool ret = false;
    __disable_irq();
    if (p_event_queue_head != NULL)
    {
        memcpy(event, p_event_queue_head, sizeof(event_t));
        event_t *tmp = p_event_queue_head;
        p_event_queue_head = tmp->next;
        free(tmp);
        ret = true;
    }
    __enable_irq();
    return ret;
}
