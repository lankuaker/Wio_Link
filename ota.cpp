


#include "esp8266.h"
#include "Arduino.h"
#include "rpc_stream.h"

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
    wifi_station_disconnect();
    system_upgrade_reboot();
}

void ota_response(void *arg)
{
    struct upgrade_server_info *server = arg;

    if(server->upgrade_flag == true)
    {
        Serial1.println("device_upgrade_success\r\n");
        ota_succ = true;
        os_timer_setfn(&timer_reboot, timer_reboot_proc, NULL);
        os_timer_arm(&timer_reboot, 1000, false);
        response_msg_open("ota_result");
        writer_print(TYPE_STRING, "success");
        response_msg_close();
        //wifi_station_disconnect();
        //system_upgrade_reboot();
    } else
    {
        Serial1.println("device_upgrade_failed\r\n");
        response_msg_open("ota_result");
        writer_print(TYPE_STRING, "fail");
        response_msg_close();
        ota_succ = false;
    }

    os_free(server->url);
    server->url = NULL;
    os_free(server);
    server = NULL;

    ota_fini = true;
}

void ota_start()
{

    uint8_t user_bin[12] = { 0 };

    struct upgrade_server_info *upServer = (struct upgrade_server_info *)os_zalloc(sizeof(struct upgrade_server_info));

    upServer->pespconn = NULL;
    const char esp_server_ip[4] = OTA_SERVER_IP;
    os_memcpy(upServer->ip, esp_server_ip, 4);

    upServer->port = OTA_SERVER_PORT;

    upServer->check_cb = ota_response;
    upServer->check_times = 60000;

    if(upServer->url == NULL)
    {
        upServer->url = (uint8 *)os_zalloc(1024);
    }

    if(system_upgrade_userbin_check() == UPGRADE_FW_BIN1)
    {
        Serial1.printf("Running user1.bin \r\n\r\n");
        os_memcpy(user_bin, "user2.bin", 10);
    } else if(system_upgrade_userbin_check() == UPGRADE_FW_BIN2)
    {
        Serial1.printf("Running user2.bin \r\n\r\n");
        os_memcpy(user_bin, "user1.bin", 10);
    }

    os_sprintf(upServer->url,
               "GET /%s HTTP/1.1\r\nHost: " IPSTR ":%d\r\n" pheadbuffer "",
               user_bin, IP2STR(upServer->ip), OTA_SERVER_PORT);


    if(system_upgrade_start(upServer) == false)
    {
        Serial1.println("Upgrade already started.");
    } else
    {
        Serial1.println("Upgrade started");
    }
}
