/*
 * rpc_server.cpp
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

#include "stdlib.h"
#include "rpc_stream.h"
#include "rpc_server.h"
#include "Arduino.h"
#include "network.h"
#include "ota.h"


resource_t *p_first_resource;
resource_t *p_cur_resource;

event_t *p_event_queue_head;
event_t *p_event_queue_tail;

static int parse_stage_data;
static int parse_stage_cmd;

extern void print_well_known();
void drain_event_queue();

void rpc_server_init()
{
    //init rpc stream
    stream_init(STREAM_DATA);
    stream_init(STREAM_CMD);
    //init rpc server
    p_first_resource = p_cur_resource = NULL;
    p_event_queue_head = p_event_queue_tail = NULL;
    parse_stage_data = PARSE_REQ_TYPE;
    parse_stage_cmd = PARSE_REQ_TYPE;

    rpc_server_register_resources();
    //printf("rpc server init done!\n");

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

int __convert_arg(uint8_t *arg_buff, void *buff, int type)
{
    int i;
    uint32_t ui;
    switch (type)
    {
        case TYPE_BOOL:
            {
                i = atoi((const char *)(buff));
                ui = abs(i);
                memcpy(arg_buff, &ui, sizeof(bool));
                return sizeof(bool);
                break;
            }
        case TYPE_UINT8:
            {
                i = atoi((const char *)(buff));
                ui = abs(i);
                memcpy(arg_buff, &ui, 1);
                return 1;
                break;
            }
        case TYPE_UINT16:
            {
                i = atoi((const char *)(buff));
                ui = abs(i);
                memcpy(arg_buff, &ui, 2);
                return 2;
                break;
            }
        case TYPE_UINT32:
            {
                int32_t l = atol((const char *)(buff));
                ui = abs(l);
                memcpy(arg_buff, &ui, 4);
                return 4;
                break;
            }
        case TYPE_INT8:
            {
                i = atoi((const char *)(buff));
                char c = i;
                memcpy(arg_buff, &c, 1);
                return 1;
                break;
            }
        case TYPE_INT16:
            {
                i = atoi((const char *)(buff));
                memcpy(arg_buff, &i, 2);
                return 2;
                break;
            }
        case TYPE_INT32:
            {
                int32_t l = atol((const char *)(buff));
                memcpy(arg_buff, &l, 4);
                return 4;
                break;
            }
        case TYPE_INT:
            {
                int l = atol((const char *)(buff));
                memcpy(arg_buff, &l, sizeof(int));
                return sizeof(int);
                break;
            }
        case TYPE_FLOAT:
            {
                float f = atof((const char *)(buff));
                memcpy(arg_buff, &f, sizeof(float));
                return sizeof(float);
                break;
            }
        case TYPE_STRING:
            {
                uint32_t ptr = (uint32_t)buff;
                memcpy(arg_buff, &ptr, 4);
                return 4;
                break;
            }
        default:
            break;
    }
    return 0;
}



static int req_type;

static char buff[ARG_BUFFER_LEN+1];
static int  offset = 0;
static int  arg_index = 0;
static char grove_name[33];
static char method_name[33];
static char ch;
static uint8_t arg_buff[4 * MAX_INPUT_ARG_LEN];
static int arg_offset;
static resource_t *p_resource;

void process_data_stream()
{
    //writer_print(TYPE_INT, &parse_stage_data);
    while (stream_available(STREAM_DATA) > 0 || parse_stage_data == PARSE_CALL)
    {
        switch (parse_stage_data)
        {
            case PARSE_REQ_TYPE:
                {
                    bool parsed_req_type = false;

                    buff[0] = buff[1]; buff[1] = buff[2]; buff[2] = buff[3];
                    buff[3] = stream_read(STREAM_DATA);

                    if (memcmp(buff, "GET", 3) == 0 || memcmp(buff, "get", 3) == 0)
                    {
                        req_type = REQ_GET;
                        parsed_req_type = true;
                        response_msg_open(STREAM_DATA, "resp_get");
                    }

                    if (memcmp(buff, "POST", 4) == 0 || memcmp(buff, "post", 4) == 0)
                    {
                        req_type = REQ_POST;
                        parsed_req_type = true;
                        stream_read(STREAM_DATA);  //read " " out
                        response_msg_open(STREAM_DATA,"resp_post");
                    }

                    if (parsed_req_type)
                    {
                        ch = stream_read(STREAM_DATA);
                        if (ch != '/')
                        {
                            //error request format
                            writer_print(TYPE_STRING, "\"BAD REQUEST: missing root:'/'.\"");
                            response_msg_close(STREAM_DATA);
                        } else
                        {
                            parse_stage_data = PARSE_GROVE_NAME;
                            p_resource = NULL;
                            offset = 0;
                        }
                    }
                    break;
                }
            case PARSE_GROVE_NAME:
                {
                    ch = stream_read(STREAM_DATA);
                    if (ch == '\r' || ch == '\n')
                    {
                        buff[offset] = '\0';
                        if (strcmp(buff, ".well-known") == 0)
                        {
                            //writer_print(TYPE_STRING, "\"/.well-known is not implemented\"");
                            print_well_known();
                            response_msg_close(STREAM_DATA);
                            parse_stage_data = PARSE_REQ_TYPE;
                        } else
                        {
                            writer_print(TYPE_STRING, "\"BAD REQUEST: missing method name.\"");
                            response_msg_close(STREAM_DATA);
                            parse_stage_data = PARSE_REQ_TYPE;
                        }
                    } else if (ch != '/' && offset < ARG_BUFFER_LEN)
                    {
                        buff[offset++] = ch;
                    } else
                    {
                        buff[offset] = '\0';
                        memcpy(grove_name, buff, offset + 1);
                        while (ch != '/')
                        {
                            ch = stream_read(STREAM_DATA);
                        }
                        parse_stage_data = PARSE_METHOD;
                        offset = 0;
                    }
                    break;
                }
            case PARSE_METHOD:
                {
                    ch = stream_read(STREAM_DATA);
                    if (ch == '\r' || ch == '\n')
                    {
                        buff[offset] = '\0';
                        memcpy(method_name, buff, offset + 1);
                        parse_stage_data = CHECK_ARGS;  //to check if req missing arg
                    } else if (ch == '/')
                    {
                        buff[offset] = '\0';
                        memcpy(method_name, buff, offset + 1);
                        parse_stage_data = PRE_PARSE_ARGS;
                    } else if (offset >= ARG_BUFFER_LEN)
                    {
                        buff[offset] = '\0';
                        memcpy(method_name, buff, offset + 1);
                        while (ch != '/' && ch != '\r' && ch != '\n')
                        {
                            ch = stream_read(STREAM_DATA);
                        }
                        if (ch == '\r' || ch == '\n') parse_stage_data = CHECK_ARGS;
                        else parse_stage_data = PRE_PARSE_ARGS;
                    } else
                    {
                        buff[offset++] = ch;
                    }
                    break;
                }
            case CHECK_ARGS:
                {
                    p_resource = __find_resource((char *)grove_name, (char *)method_name, req_type);
                    if (!p_resource)
                    {
                        writer_print(TYPE_STRING, "\"GROVE OR METHOD NOT FOUND WHEN CHECK POST ARGS\"");
                        response_msg_close(STREAM_DATA);
                        parse_stage_data = PARSE_REQ_TYPE;
                        break;
                    }
                    if (p_resource->arg_types[0] != TYPE_NONE)
                    {
                        writer_print(TYPE_STRING, "\"MISSING ARGS\"");
                        response_msg_close(STREAM_DATA);
                        parse_stage_data = PARSE_REQ_TYPE;
                        break;
                    }
                    parse_stage_data = PARSE_CALL;
                    break;
                }
            case PRE_PARSE_ARGS:
                {
                    p_resource = __find_resource((char *)grove_name, (char *)method_name, req_type);
                    if (!p_resource)
                    {
                        writer_print(TYPE_STRING, "\"GROVE OR METHOD NOT FOUND WHEN PRE-PARSE ARGS\"");
                        response_msg_close(STREAM_DATA);
                        parse_stage_data = PARSE_REQ_TYPE;
                        break;
                    }
                    parse_stage_data = PARSE_ARGS;
                    arg_index = 0;
                    arg_offset = 0;
                    offset = 0;
                    break;
                }
            case PARSE_ARGS:
                {
                    bool overlen = false;
                    ch = stream_read(STREAM_DATA);
                    if (ch == '\r' || ch == '\n' || ch == '/')
                    {
                        buff[offset] = '\0';
                    } else if (offset >= ARG_BUFFER_LEN)
                    {
                        buff[offset] = '\0';
                        while (ch != '/' && ch != '\r' && ch != '\n')
                        {
                            ch = stream_read(STREAM_DATA);
                        }
                        overlen = true;
                    } else
                    {
                        buff[offset++] = ch;
                    }

                    if (ch == '/')
                    {
                        char *p = buff;
                        int len = __convert_arg(arg_buff + arg_offset, p, p_resource->arg_types[arg_index++]);
                        arg_offset += len;
                        offset = 0;
                    }
                    if (ch == '\r' || ch == '\n' || overlen)
                    {
                        if ((arg_index < 3 && p_resource->arg_types[arg_index + 1] != TYPE_NONE) ||
                            (arg_index <= 3 && p_resource->arg_types[arg_index] != TYPE_NONE && strlen(buff) < 1))
                        {
                            writer_print(TYPE_STRING, "\"MISSING ARGS\"");
                            response_msg_close(STREAM_DATA);
                            parse_stage_data = PARSE_REQ_TYPE;
                            break;
                        }
                        char *p = buff;
                        int len = __convert_arg(arg_buff + arg_offset, p, p_resource->arg_types[arg_index++]);
                        arg_offset += len;
                        offset = 0;
                        parse_stage_data = PARSE_CALL;
                    }
                    break;
                }
            case PARSE_CALL:
                {
                    if (!p_resource) p_resource = __find_resource((char *)grove_name, (char *)method_name, req_type);

                    if (!p_resource)
                    {
                        writer_print(TYPE_STRING, "\"GROVE OR METHOD NOT FOUND WHEN CALL\"");
                        response_msg_close(STREAM_DATA);
                        parse_stage_data = PARSE_REQ_TYPE;
                        break;
                    }
                    //writer_print(TYPE_STRING, "{");
                    if (false == p_resource->method_ptr(p_resource->class_ptr, arg_buff)) response_msg_append_205(STREAM_DATA);
                    //writer_print(TYPE_STRING, "}");
                    response_msg_close(STREAM_DATA);

                    parse_stage_data = PARSE_REQ_TYPE;
                    break;
                }

            default:
                break;
        }
        delay(0);  //yield the cpu for critical use or the watch dog will be hungry enough to trigger the reset.
    }
}

static char buff_cmd[8];

void process_cmd_stream()
{
    //writer_print(TYPE_INT, &parse_stage);
    while (stream_available(STREAM_CMD) > 0)
    {
        switch (parse_stage_cmd)
        {
            case PARSE_REQ_TYPE:
                {
                    buff_cmd[0] = buff_cmd[1]; buff_cmd[1] = buff_cmd[2]; buff_cmd[2] = buff_cmd[3];
                    buff_cmd[3] = stream_read(STREAM_CMD);

                    if (memcmp(buff_cmd, "OTA", 3) == 0 || memcmp(buff_cmd, "ota", 3) == 0)
                    {
                        parse_stage_cmd = DIVE_INTO_OTA;
                        response_msg_open(STREAM_CMD, "ota_trig_ack");
                        stream_print(STREAM_CMD, TYPE_STRING, "null");
                        response_msg_close(STREAM_CMD);
                        break;
                    }

                    if (memcmp(buff_cmd, "APP", 3) == 0 || memcmp(buff_cmd, "app", 3) == 0)
                    {
                        parse_stage_cmd = GET_APP_NUM;
                        response_msg_open(STREAM_CMD, "resp_app");
                        break;
                    }

                    break;
                }

            case DIVE_INTO_OTA:
                {
                    //TODO: refer to the ota related code here
                    ch = stream_read(STREAM_CMD);
                    while (ch != '\r' && ch != '\n')
                    {
                        ch = stream_read(STREAM_CMD);
                    }
                    //espconn_disconnect(&tcp_conn[0]);
                    parse_stage_cmd = PARSE_REQ_TYPE;
                    ota_start();
                    while (!ota_fini)
                    {
                        digitalWrite(STATUS_LED, ~digitalRead(STATUS_LED));
                        delay(100);
                        keepalive_last_recv_time[0] = keepalive_last_recv_time[1] = millis();  //to prevent online check and offline-reconnect during ota
                    }
                    break;
                }
            case GET_APP_NUM:
                {
                    ch = stream_read(STREAM_CMD);
                    while (ch != '\r' && ch != '\n')
                    {
                        ch = stream_read(STREAM_CMD);
                    }
                    parse_stage_cmd = PARSE_REQ_TYPE;
                    
                    int bin_num = 1;
                    if(system_upgrade_userbin_check() == UPGRADE_FW_BIN1)
                    {
                        Serial1.printf("Running user1.bin \r\n\r\n");
                        //os_memcpy(user_bin, "user2.bin", 10);
                        bin_num = 1;
                    } else if(system_upgrade_userbin_check() == UPGRADE_FW_BIN2)
                    {
                        Serial1.printf("Running user2.bin \r\n\r\n");
                        //os_memcpy(user_bin, "user1.bin", 10);
                        bin_num = 2;
                    }
                    stream_print(STREAM_CMD, TYPE_INT, &bin_num);
                    response_msg_close(STREAM_CMD);
                    
                    break;
                }

            default:
                break;
        }
        delay(0);  //yield the cpu for critical use or the watch dog will be hungry enough to trigger the reset.
    }
}

void rpc_server_loop()
{
    drain_event_queue();
    process_data_stream();
    process_cmd_stream();
}


static event_t event;

void drain_event_queue()
{
    /* report event if event queue is not empty */
    while (rpc_server_event_queue_pop(&event))
    {
        response_msg_open(STREAM_DATA,"event");
        writer_print(TYPE_STRING, "{\"");
        writer_print(TYPE_STRING, event.event_name);
        writer_print(TYPE_STRING, "\":\"");
        writer_print(TYPE_UINT32, &event.event_data);
        writer_print(TYPE_STRING, "\"}");
        response_msg_close(STREAM_DATA);
        optimistic_yield(100);
    }
}

void rpc_server_event_report(char *event_name, uint32_t event_data)
{
    noInterrupts();
    int cnt = 0;
    event_t *tmp = p_event_queue_head;
    while (tmp)
    {
        tmp = tmp->next;
        cnt++;
    }
    if (cnt <= 100)
    {
        event_t *ev = (event_t *)malloc(sizeof(event_t));
        if (ev)
        {
            ev->event_name = event_name;
            ev->event_data = event_data;
            if (p_event_queue_tail == NULL)
            {
                p_event_queue_head = ev;
            } else
            {
                p_event_queue_tail->next = ev;
            }
            ev->prev = p_event_queue_tail;
            ev->next = NULL;
            p_event_queue_tail = ev;
        }
    }
    interrupts();
}

bool rpc_server_event_queue_pop(event_t *event)
{
    bool ret = false;
    noInterrupts();
    if (p_event_queue_head != NULL)
    {
        memcpy(event, p_event_queue_head, sizeof(event_t));
        event_t *tmp = p_event_queue_head;
        p_event_queue_head = tmp->next;
        if (p_event_queue_head == NULL) p_event_queue_tail = NULL;
        free(tmp);
        ret = true;
    }
    interrupts();
    return ret;
}
