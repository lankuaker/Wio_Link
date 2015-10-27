/*
 * ota.cpp
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


#include "esp8266.h"
#include "Arduino.h"
#include "rpc_stream.h"
#include "base64.h"
#include "eeprom.h"
#include "network.h"

#define pheadbuffer "Connection: keep-alive\r\n\
Cache-Control: no-cache\r\n\
User-Agent: Mozilla/5.0 (Windows NT 5.1) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/30.0.1599.101 Safari/537.36 \r\n\
Accept: */*\r\n\
Accept-Encoding: gzip,deflate\r\n\
Accept-Language: zh-CN,eb-US;q=0.8\r\n\r\n"

bool ota_fini = false;
bool ota_succ = false;

os_timer_t timer_reboot;

static void timer_reboot_proc(void *arg)
{
    bool res = *(bool *)arg;
    wifi_station_disconnect();
    if (res)
    {
        system_upgrade_reboot();
    } else
    {
        system_restart(); 
    }
}

void ota_response(void *arg)
{
    struct upgrade_server_info *server = arg;

    if(server->upgrade_flag == true)
    {
        Serial1.println("device_upgrade_success\r\n");
        ota_succ = true;
        
        memset(EEPROM.getDataPtr() + EEP_OTA_RESULT_FLAG, 1, 1);
        
        
    } else
    {
        Serial1.println("device_upgrade_failed\r\n");
        ota_succ = false;
        
        memset(EEPROM.getDataPtr() + EEP_OTA_RESULT_FLAG, 2, 1);
    }

    EEPROM.commit();
    
    os_free(server->url);
    server->url = NULL;
    os_free(server);
    server = NULL;

    ota_fini = true;
    
    os_timer_setfn(&timer_reboot, timer_reboot_proc, &ota_succ);
    os_timer_arm(&timer_reboot, 1000, false);
}

void ota_start()
{
    ota_fini = false;

    //uint8_t user_bin[12] = { 0 };
    int bin_num = 1;

    struct upgrade_server_info *upServer = (struct upgrade_server_info *)os_zalloc(sizeof(struct upgrade_server_info));

    upServer->pespconn = NULL;
    char *esp_server_ip = (EEPROM.getDataPtr() + EEP_OTA_SERVER_IP);
    os_memcpy(upServer->ip, esp_server_ip, 4);

    upServer->port = OTA_DOWNLOAD_PORT;

    upServer->check_cb = ota_response;
    upServer->check_times = 180000;

    if(upServer->url == NULL)
    {
        upServer->url = (uint8 *)os_zalloc(1024);
    }

    if(system_upgrade_userbin_check() == UPGRADE_FW_BIN1)
    {
        Serial1.printf("Running user1.bin \r\n\r\n");
        //os_memcpy(user_bin, "user2.bin", 10);
        bin_num = 2;
    } else if(system_upgrade_userbin_check() == UPGRADE_FW_BIN2)
    {
        Serial1.printf("Running user2.bin \r\n\r\n");
        //os_memcpy(user_bin, "user1.bin", 10);
        bin_num = 1;
    }

    char sn[64];
    int len=64;
    if(base64_encode(sn, &len, EEPROM.getDataPtr()+EEP_OFFSET_SN, 32) != 0)
    {
        Serial1.println("base64 encode failed");
        ota_fini = true;
        return;
    }
    os_strcpy(sn + len, "NiNz");
    os_sprintf(upServer->url,
               "GET %s/ota/bin?app=%d&sn=%s HTTP/1.1\r\nHost: " IPSTR ":%d\r\n" pheadbuffer "",
               OTA_SERVER_URL_PREFIX, bin_num, sn, IP2STR(upServer->ip), OTA_DOWNLOAD_PORT);

    Serial1.println("Upgrade started");
    response_msg_open(STREAM_CMD,"ota_status");
    stream_print(STREAM_CMD,TYPE_STRING, "\"started\"");
    response_msg_close(STREAM_CMD);
    
    delay(100);
    
    espconn_disconnect(&tcp_conn[0]);
    espconn_disconnect(&tcp_conn[1]);
        
    if(system_upgrade_start(upServer) == false)
    {
        Serial1.println("Upgrade already started.");
    } 
}
