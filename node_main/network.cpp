/*
 * network.cpp
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
#include "cbuf.h"
#include "network.h"
#include "eeprom.h"
#include "sha256.h"
#include "aes.h"

enum
{
    WAIT_SMARTCONFIG_DONE, SMARTCONFIG_DONE
};


static uint8_t smartconfig_status = WAIT_SMARTCONFIG_DONE;
uint8_t main_conn_status = WAIT_CONN_DONE;
static uint8_t main_conn_retry_cnt = 0;
static uint8_t get_hello = 0;
static uint8_t confirm_hello_retry_cnt = 0;
static os_event_t update_network_task_q;

const char *device_find_request = "Node?";
const char *device_keysn_write_req = "KeySN: ";
#define KEY_LEN             32
#define SN_LEN              32
static struct espconn udp_conn;
struct espconn main_conn;
static struct _esp_tcp user_tcp;
static os_timer_t timer_main_conn;
static os_timer_t timer_network_check;
static os_timer_t timer_confirm_hello;
static os_timer_t timer_tx;

CircularBuffer *rx_stream_buffer = NULL;
CircularBuffer *tx_stream_buffer = NULL;

static aes_context aes_ctx;
static int iv_offset = 0;
static unsigned char iv[16];

void main_connection_init();


///
///

static void smartconfig_done_callback(sc_status status, void *pdata)
{
    struct station_config *sta_conf;

    switch (status)
    {
    case SC_STATUS_WAIT:
        Serial1.printf("SC_STATUS_WAIT\n");
        break;
    case SC_STATUS_FIND_CHANNEL:
        Serial1.printf("SC_STATUS_FIND_CHANNEL\n");
        break;
    case SC_STATUS_GETTING_SSID_PSWD:
        Serial1.printf("SC_STATUS_GETTING_SSID_PSWD\n");
        break;
    case SC_STATUS_LINK:
        Serial1.printf("SC_STATUS_LINK\n");
        sta_conf = (struct station_config *)pdata;
        wifi_station_set_config(sta_conf);
        wifi_station_disconnect();
        wifi_station_connect();
        break;
    case SC_STATUS_LINK_OVER:
        Serial1.printf("SC_STATUS_LINK_OVER\n");
        smartconfig_stop();
        smartconfig_status = SMARTCONFIG_DONE;
        break;
    default:
        break;
    }
}

static void user_devicefind_recv(void *arg, char *pusrdata, unsigned short length)
{
    struct espconn *conn = (struct espconn *)arg;
    char hwaddr[6];

    struct ip_info ipconfig;

    if (wifi_get_opmode() != STATION_MODE) {
        wifi_get_ip_info(SOFTAP_IF, &ipconfig);
        wifi_get_macaddr(SOFTAP_IF, hwaddr);

        if (!ip_addr_netcmp((struct ip_addr *)conn->proto.udp->remote_ip, &ipconfig.ip, &ipconfig.netmask))
        {
            wifi_get_ip_info(STATION_IF, &ipconfig);
            wifi_get_macaddr(STATION_IF, hwaddr);
        }
    } else {
        wifi_get_ip_info(STATION_IF, &ipconfig);
        wifi_get_macaddr(STATION_IF, hwaddr);
    }

    if (pusrdata == NULL) {
        return;
    }

    /* prevent bad guy from flashing your node without your permission */
    pinMode(SMARTCONFIG_KEY, INPUT_PULLUP);
    if (digitalRead(SMARTCONFIG_KEY) != 0)
    {
        return;
    }

    char *pkey;
    if (length == os_strlen(device_find_request) &&
            os_strncmp(pusrdata, device_find_request, os_strlen(device_find_request)) == 0)
    {
        char *device_desc = new char[128];
        os_sprintf(device_desc, "Node: 0x%08x," MACSTR "," IPSTR "\r\n",
                   system_get_chip_id(), MAC2STR(hwaddr), IP2STR(&ipconfig.ip));

        Serial1.printf("%s", device_desc);
        length = os_strlen(device_desc);
        espconn_sent(conn, device_desc, length);
        delete[] device_desc;
    } else if ((pkey=os_strstr(pusrdata, device_keysn_write_req)) != NULL)
    {
        if ((pusrdata + length - pkey - os_strlen(device_keysn_write_req)) >= (KEY_LEN+SN_LEN))
        {
            pkey += os_strlen(device_keysn_write_req);

            char *keybuf = new char[KEY_LEN *2];

            os_memcpy(keybuf, pkey, KEY_LEN);
            keybuf[KEY_LEN] = 0;
            os_memcpy(EEPROM.getDataPtr() + EEP_OFFSET_KEY, keybuf, KEY_LEN + 1);
            Serial1.printf("write key: %s\n", keybuf);

            pkey += (KEY_LEN + 1);

            os_memcpy(keybuf, pkey, SN_LEN);
            keybuf[SN_LEN] = 0;
            os_memcpy(EEPROM.getDataPtr() + EEP_OFFSET_SN, keybuf, SN_LEN + 1);
            Serial1.printf("write sn: %s\n", keybuf);

            EEPROM.commit();

            delete [] keybuf;
            espconn_sent(conn, "ok\r\n", 4);
        }
    }
}

void user_devicefind_init(void)
{
    udp_conn.type = ESPCONN_UDP;
    udp_conn.proto.udp = (esp_udp *)os_zalloc(sizeof(esp_udp));
    udp_conn.proto.udp->local_port = 1025;
    espconn_regist_recvcb(&udp_conn, user_devicefind_recv);
    espconn_create(&udp_conn);
}

static void main_connection_recv_cb(void *arg, char *pusrdata, unsigned short length)
{
    struct espconn *pespconn = arg;
    char *pstr = NULL;
    size_t room;

    switch (main_conn_status)
    {
    case WAIT_HELLO_DONE:
        if ((pstr = strstr(pusrdata, "sorry")) != NULL)
        {
            get_hello = 2;
        } else
        {
            #if 0
            Serial1.println(length);
            for (int i = 0; i < length;i++)
            {
                Serial1.print(*(pusrdata + i), HEX);
                Serial1.print(" ");
            }
            Serial1.println();
            #endif
            aes_init(&aes_ctx);
            aes_setkey_enc(&aes_ctx, EEPROM.getDataPtr() + EEP_OFFSET_KEY, KEY_LEN*8);
            memcpy(iv, pusrdata, 16);
            aes_crypt_cfb128(&aes_ctx, AES_DECRYPT, length - 16, &iv_offset, iv, pusrdata + 16, pusrdata);
            if (os_strncmp(pusrdata, "hello", 5) == 0)
            {
                get_hello = 1;
            }
        }

        break;
    case KEEP_ALIVE:
        aes_crypt_cfb128(&aes_ctx, AES_DECRYPT, length, &iv_offset, iv, pusrdata, pusrdata);

        Serial1.println(pusrdata);

        if (os_strncmp(pusrdata, "##PING##", 8) == 0 && tx_stream_buffer)
        {
            network_puts("##ALIVE##\r\n", 11);
            return;
        }

        if (!rx_stream_buffer) return;

        //Serial1.printf("recv %d data\n", length);
        room = rx_stream_buffer->capacity()-rx_stream_buffer->size();
        length = os_strlen(pusrdata);  //filter out the padding 0s
        if ( room > 0 )
        {
            rx_stream_buffer->write(pusrdata, (room>length?length:room));
        }
        break;
    default:
        break;
    }
}

static void main_connection_sent_cb(void *arg)
{
    struct espconn *pespconn = arg;
    //Serial1.println("main_connection_sent_cb");
    if (!tx_stream_buffer) return;

    size_t size = tx_stream_buffer->size();

    size_t size2 = size;
    if (size > 0)
    {
        size2 = (((size % 16) == 0)? (size):(size + (16 - size % 16) ));  //damm, the python crypto library only supports 16*n block length
        char *data = (char *)malloc(size2);
        os_memset(data, 0, size2);
        tx_stream_buffer->read(data, size);
        aes_crypt_cfb128(&aes_ctx, AES_ENCRYPT, size2, &iv_offset, iv, data, data);
        espconn_sent(pespconn, data, size2);
        free(data);
    }
}


void network_putc(char c)
{
#if 0
    if (main_conn_status != KEEP_ALIVE) return;
    if (main_conn.state > ESPCONN_NONE)
    {
        espconn_sent(&main_conn, &c, 1);
    }
#endif
    network_puts(&c, 1);
}

void network_puts(char *data, int len)
{
    if (main_conn_status != KEEP_ALIVE || !tx_stream_buffer) return;
    if (main_conn.state > ESPCONN_NONE)
    {
        noInterrupts();
        tx_stream_buffer->write(data, len);
        interrupts();

        if ((strchr(data, '\r') || strchr(data, '\n') || tx_stream_buffer->size() > 512)/* && main_conn.state != ESPCONN_WRITE*/)
        {
            //os_timer_disarm(&timer_tx);
            //os_timer_setfn(&timer_tx, main_connection_sent_cb, &main_conn);
            //os_timer_arm(&timer_tx, 1, 0);
            main_connection_sent_cb(&main_conn);
        }
    }
}



static void confirm_hello(void *arg)
{
    Serial1.printf("confirm hello: %d \n", get_hello);

    if (get_hello == 1)
    {
        Serial1.printf("handshake done, keep-alive\n");
        main_conn_status = KEEP_ALIVE;
    } else if (get_hello == 2)
    {
        Serial1.printf("handshake: sorry from server\n");
        main_conn_status = DIED_IN_HELLO;
    } else
    {
        if (++confirm_hello_retry_cnt > 60)
        {
            main_conn_status = DIED_IN_HELLO;
            return;
        } else
        {
            os_timer_setfn(&timer_confirm_hello, confirm_hello, arg);
            os_timer_arm(&timer_confirm_hello, 1000, 0);
        }
    }
}

/**
 * start the handshake with server
 * node will send itself's sn number and a signature signed with
 * its private key to server
 * @param
 */
void main_connection_send_hello(void *arg)
{
    struct espconn *pespconn = (struct espconn *)arg;
    uint8_t hmac_hash[32];

    uint8_t *buff = os_malloc(SN_LEN + 32);
    if (!buff)
    {
        main_conn_status = DIED_IN_HELLO;
        return;
    }
    //EEPROM.getDataPtr() + EEP_OFFSET_SN
    os_memcpy(buff, EEPROM.getDataPtr() + EEP_OFFSET_SN, SN_LEN);

    sha256_hmac(EEPROM.getDataPtr() + EEP_OFFSET_KEY, KEY_LEN, buff, SN_LEN, hmac_hash, 0);

    os_memcpy(buff + SN_LEN, hmac_hash, 32);

    espconn_sent(pespconn, buff, SN_LEN+32);

    os_free(buff);

    get_hello = 0;
    os_timer_disarm(&timer_confirm_hello);
    os_timer_setfn(&timer_confirm_hello, confirm_hello, arg);
    os_timer_arm(&timer_confirm_hello, 100, 0);
    confirm_hello_retry_cnt = 0;
    main_conn_status = WAIT_HELLO_DONE;
}

void main_connection_connected_callback(void *arg)
{
    struct espconn *pespconn = (struct espconn *)arg;

    Serial1.println("main conn connected");
    main_conn_status = CONNECTED;
    main_conn_retry_cnt = 0;
    espconn_regist_recvcb(pespconn, main_connection_recv_cb);
    espconn_regist_sentcb(pespconn, main_connection_sent_cb);

    espconn_set_opt(pespconn, 0xff);

    /* send hello */
    main_connection_send_hello(arg);

}

void main_connection_reconnect_callback(void *arg, sint8 err)
{
    Serial1.printf("main conn re-conn, err: %d\n", err);

    if (++main_conn_retry_cnt >= 5)
    {
        main_conn_status = DIED_IN_CONN;
        return;
    }
    os_timer_disarm(&timer_main_conn);
    os_timer_setfn(&timer_main_conn, main_connection_init, NULL);
    os_timer_arm(&timer_main_conn, 1000, 0);
    main_conn_status = WAIT_CONN_DONE;
}

void network_check_timer_callback()
{
    static uint8_t last_main_conn_status = 0xff;

    switch (main_conn_status)
    {
    case DIED_IN_HELLO:
        Serial1.printf("No hello ack from server after 5 retry\n");
        Serial1.println("Please check server's ip and port, also check AccessToken\n");
        digitalWrite(STATUS_LED, 0);
        /*while (1)
        {
            delay(100);
            ESP.wdtFeed();
        }*/
        break;
    case DIED_IN_CONN:
        Serial1.printf("The main connection died after 5 retry\n");
        Serial1.println("Please check server's ip and port, also check AccessToken\n");
        digitalWrite(STATUS_LED, 0);
        /*while (1)
        {
            delay(100);
            ESP.wdtFeed();
        }*/
        break;
    case WAIT_CONN_DONE:
    case WAIT_HELLO_DONE:
        if (main_conn_status != last_main_conn_status)
        {
            os_timer_arm(&timer_network_check, 50, false);
            digitalWrite(STATUS_LED, 0);
        }else
        {
            os_timer_arm(&timer_network_check, (digitalRead(STATUS_LED) > 0 ? 50 : 1000), false);
            digitalWrite(STATUS_LED, ~digitalRead(STATUS_LED));
        }
        break;
    case CONNECTED:
    case KEEP_ALIVE:
        os_timer_arm(&timer_network_check, 1000, false);
        digitalWrite(STATUS_LED, ~digitalRead(STATUS_LED));
        break;
    default:
        break;
    }

    last_main_conn_status = main_conn_status;
}

bool check_ascii_char(uint8_t *data, int len)
{
    for (int i = 0; i < len;i++)
    {
        if (*(data+i) > 127)
        {
            return false;
        }
    }
    return true;
}

void main_connection_init()
{
    Serial1.printf("main_connection_init.\r\n");

    main_conn.type = ESPCONN_TCP;
    main_conn.state = ESPCONN_NONE;
    main_conn.proto.tcp = &user_tcp;
    const char server_ip[4] = SERVER_IP;
    os_memcpy(main_conn.proto.tcp->remote_ip, server_ip, 4);
    main_conn.proto.tcp->remote_port = SERVER_PORT;
    main_conn.proto.tcp->local_port = espconn_port();
    espconn_regist_connectcb(&main_conn, main_connection_connected_callback);
    espconn_regist_reconcb(&main_conn, main_connection_reconnect_callback);
    espconn_connect(&main_conn);
    os_timer_disarm(&timer_network_check);
    os_timer_setfn(&timer_network_check, network_check_timer_callback, NULL);
    os_timer_arm(&timer_network_check, 1, false);
    main_conn_status = WAIT_CONN_DONE;
}

void establish_network()
{
#if ENABLE_DEBUG_ON_UART1
    Serial1.begin(74880);
    Serial1.println("\n\n"); //clear the garbage in uart when boot up
#endif

    if (!rx_stream_buffer) rx_stream_buffer = new CircularBuffer(256);
    if (!tx_stream_buffer) tx_stream_buffer = new CircularBuffer(1024);

    Serial1.printf("Node name: %s\n", NODE_NAME);
    Serial1.printf("Chip id: 0x%08x\n", system_get_chip_id());

    /* get key and name */
    EEPROM.begin(4096);

    if (check_ascii_char(EEPROM.getDataPtr() + EEP_OFFSET_KEY, KEY_LEN))
    {
        Serial1.printf("Device key in flash: %s\n", EEPROM.getDataPtr() + EEP_OFFSET_KEY);
    }

    if (check_ascii_char(EEPROM.getDataPtr() + EEP_OFFSET_SN, SN_LEN))
    {
        Serial1.printf("Node SN in flash: %s\n", EEPROM.getDataPtr() + EEP_OFFSET_SN);
    }

    pinMode(SMARTCONFIG_KEY, INPUT_PULLUP);
    pinMode(STATUS_LED, OUTPUT);

    Serial1.printf("start to establish network connection.\r\n");

    if (digitalRead(SMARTCONFIG_KEY) == 0)
    {
        wifi_set_opmode_current(0x01); //smartconfig only support station mode
        smartconfig_status = WAIT_SMARTCONFIG_DONE;
        smartconfig_start(SC_TYPE_ESPTOUCH, smartconfig_done_callback);
        while (smartconfig_status != SMARTCONFIG_DONE)
        {
            delay(100);
            digitalWrite(STATUS_LED, ~digitalRead(STATUS_LED));
        }
    } else
    {
        wifi_set_opmode(0x01); //we're now only using the station mode

        struct station_config config;
        wifi_station_get_config(&config);
        Serial1.printf("connect to ssid %s with passwd %s\r\n", config.ssid, config.password);
        wifi_station_disconnect();
        wifi_station_connect(); //connect with saved config in flash
    }

    /* check IP */
    uint8_t connect_status = wifi_station_get_connect_status();
    while (connect_status != STATION_GOT_IP)
    {
        Serial1.printf("Wait getting ip, state: %d\n", connect_status);
        delay(1000);
        connect_status = wifi_station_get_connect_status();
        digitalWrite(STATUS_LED, 0);
        delay(50);
        digitalWrite(STATUS_LED, 1);
        delay(50);
        digitalWrite(STATUS_LED, 0);
        delay(50);
        digitalWrite(STATUS_LED, 1);
    }

    struct ip_info ip;
    wifi_get_ip_info(STATION_IF, &ip);
    Serial1.printf("Done. IP: " IPSTR "\r\n", IP2STR(&ip.ip));

    /* register a device-find responder at UDP port 1025 */
    user_devicefind_init();

    /* establish the connection with server */
    main_connection_init();

    #if 0
    while (main_conn_status < CONNECTED )
    {
        Serial1.printf("Wait connecting to server.\n");
        delay(2000);
    }

    Serial1.printf("Network established.\n\n\n");
    #endif
}


