/*
  Blink
  Turns on an LED on for one second, then off for one second, repeatedly.

  Most Arduinos have an on-board LED you can control. On the Uno and
  Leonardo, it is attached to digital pin 13. If you're unsure what
  pin the on-board LED is connected to on your Arduino model, check
  the documentation at http://arduino.cc

  This example code is in the public domain.

  modified 8 May 2014
  by Scott Fitzgerald
 */

#include "esp8266.h"
#include "Arduino.h"
#include "suli2.h"


#define pheadbuffer "Connection: keep-alive\r\n\
Cache-Control: no-cache\r\n\
User-Agent: Mozilla/5.0 (Windows NT 5.1) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/30.0.1599.101 Safari/537.36 \r\n\
Accept: */*\r\n\
Accept-Encoding: gzip,deflate\r\n\
Accept-Language: zh-CN,eb-US;q=0.8\r\n\r\n"

I2C_T i2c;

void ICACHE_FLASH_ATTR
at_upDate_rsp(void *arg)
{
    struct upgrade_server_info *server = arg;


    if(server->upgrade_flag == true)
    {
        Serial.println("device_upgrade_success\r\n");
        wifi_station_disconnect();
        delay(1000);
        system_upgrade_reboot();
    } else
    {
        Serial.println("device_upgrade_failed\r\n");
    }

    os_free(server->url);
    server->url = NULL;
    os_free(server);
    server = NULL;
}

void ICACHE_FLASH_ATTR
at_exeCmdCiupdate(uint8_t id)
{

    uint8_t user_bin[12] = { 0 };

    struct upgrade_server_info *upServer = (struct upgrade_server_info *)os_zalloc(sizeof(struct upgrade_server_info));

    //upServer->upgrade_version[5] = '\0';

    upServer->pespconn = NULL;
    const char esp_server_ip[4] = { 192, 168, 31, 126 };
    os_memcpy(upServer->ip, esp_server_ip, 4);

    upServer->port = 80;

    upServer->check_cb = at_upDate_rsp;
    upServer->check_times = 120000;

    if(upServer->url == NULL)
    {
        upServer->url = (uint8 *)os_zalloc(1024);
    }

    if(system_upgrade_userbin_check() == UPGRADE_FW_BIN1)
    {
        os_memcpy(user_bin, "user2.bin", 10);
    } else if(system_upgrade_userbin_check() == UPGRADE_FW_BIN2)
    {
        os_memcpy(user_bin, "user1.bin", 10);
    }

    os_sprintf(upServer->url,
               "GET /%s HTTP/1.1\r\nHost: " IPSTR ":%d\r\n" pheadbuffer "",
               user_bin, IP2STR(upServer->ip), 80);


    if(system_upgrade_start(upServer) == false)
    {
        Serial.println("Upgrade already started.");
    } else
    {
        Serial.println("Upgrade started");
    }
}


struct station_config config;


// the setup function runs once when you press reset or power the board
void setup()
{
    // initialize digital pin 13 as an output.
    pinMode(12, OUTPUT);
    Serial.begin(9600);
    //suli_i2c_init(&i2c, 0, 0);
    wifi_set_opmode_current(0x01);
    delay(3000);
    memcpy(config.ssid, "Xiaomi_Blindeggb", 17);
    memcpy(config.password, "~375837~",9);
    wifi_station_set_config_current(&config);
    wifi_station_disconnect();
    wifi_station_connect();

}

// the loop function runs over and over again forever
void loop()
{
    Serial.println(wifi_station_get_connect_status());
    Serial.flush();
    digitalWrite(12, HIGH);   // turn the LED on (HIGH is the voltage level)
    delay(1000);              // wait for a second
    digitalWrite(12, LOW);    // turn the LED off by making the voltage LOW
    delay(200);              // wait for a second
    Serial.println("hello");
    //suli_i2c_write(&i2c, 0, "adfasdfasd", 5);
    if(Serial.available()>0)
    {
        char c = Serial.read();
        Serial.println(c);
        if(c == 'u')
        {
            at_exeCmdCiupdate(0);
        }
    }
}

