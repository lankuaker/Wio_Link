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
uint8_t main_conn_status = WAIT_GET_IP;
static int main_conn_retry_cnt = 0;
static uint8_t get_hello = 0;
static uint8_t confirm_hello_retry_cnt = 0;
uint32_t keepalive_last_recv_time = 0;

const char *device_find_request = "Node?";
const char *blank_device_find_request = "Blank?";
const char *device_keysn_write_req = "KeySN: ";
const char *ap_config_req = "APCFG: ";
#define KEY_LEN             32
#define SN_LEN              32
static struct espconn udp_conn;
struct espconn main_conn;
static struct _esp_tcp user_tcp;
static os_timer_t timer_main_conn;
static os_timer_t timer_main_conn_reconn;
static os_timer_t timer_network_status_indicate;
static os_timer_t timer_confirm_hello;
static os_timer_t timer_tx;
static os_timer_t timer_keepalive_check;

CircularBuffer *rx_stream_buffer = NULL;
CircularBuffer *tx_stream_buffer = NULL;

static aes_context aes_ctx;
static int iv_offset = 0;
static unsigned char iv[16];
static bool txing = false;

extern "C" struct rst_info* system_get_rst_info(void);
void main_connection_init(void *arg);
void main_connection_send_hello(void *arg);
void main_connection_reconnect_callback(void *arg, int8_t err);
void network_status_indicate_timer_fn(void *arg);

//////////////////////////////////////////////////////////////////////////////////////////
 
/**
 * format the SN string into safe-printing string when the node is first used 
 * because the contents in Flash is random before valid SN is written. 
 * 
 * @param input 
 * @param output 
 */
static void format_sn(uint8_t *input, uint8_t *output)
{
    for (int i = 0; i < 32;i++)
    {
        if (*(input + i) < 33 || *(input + i) > 126)
        {
            *(output + i) = '#';
        } else *(output + i) = *(input + i);
    }
    *(output + 32) = '\0';
}

/**
 * extract a subset of a string which is cutted with a \n charactor
 * 
 * @param input 
 * @param len 
 * @param output 
 */
static uint8_t *extract_substr(uint8_t *input, uint8_t *output)
{
    uint8_t *ptr = os_strstr(input, "\t");
    if (ptr == NULL)
    {
        return NULL;
    }
    
    os_memcpy(output, input, (ptr - input));
    
    char c = *(output + (ptr - input) - 1);
    if (c == '\r')
    {
        os_memset(output + (ptr - input) -1, 0, 1);
    } else
    {
        os_memset(output + (ptr - input), 0, 1);
    }
    return ptr;
}

/**
 * perform a reboot
 */
static void fire_reboot(void *arg)
{
    Serial1.println("fired reboot...");
    digitalWrite(STATUS_LED, 0);
    delay(100);
    digitalWrite(STATUS_LED, 1);
    delay(500);
    digitalWrite(STATUS_LED, 0);
    system_restart();
}

/**
 * UDP packet recv callback
 * 
 * @param arg - the pointer to espconn struct
 * @param pusrdata - recved data
 * @param length - length of the recved data
 */
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
    
    //Serial1.println(pusrdata);
    
    uint8_t config_flag = *(EEPROM.getDataPtr() + EEP_OFFSET_CFG_FLAG);

    char *pkey;
    
    if ((length == os_strlen(device_find_request) && os_strncmp(pusrdata, device_find_request, os_strlen(device_find_request)) == 0) ||
        (length == os_strlen(blank_device_find_request) && os_strncmp(pusrdata, blank_device_find_request, os_strlen(blank_device_find_request)) == 0 && config_flag == 2))
    {
        char *device_desc = new char[128];
        char *buff_sn = new char[33];
        format_sn(EEPROM.getDataPtr() + EEP_OFFSET_SN, (uint8_t *)buff_sn);
        os_sprintf(device_desc, "Node: %s," MACSTR "," IPSTR "\r\n",
                   buff_sn, MAC2STR(hwaddr), IP2STR(&ipconfig.ip));

        Serial1.printf("%s", device_desc);
        length = os_strlen(device_desc);
        espconn_sent(conn, device_desc, length);
        
        delete[] device_desc;
        delete[] buff_sn;
    }
    else if ((pkey = os_strstr(pusrdata, ap_config_req)) != NULL && config_flag == 2)
    {
        //ssid
        pkey += os_strlen(ap_config_req);
        uint8_t *ptr;
        
        ptr = extract_substr(pkey, EEPROM.getDataPtr() + EEP_OFFSET_SSID);
        
        if (ptr)
        {
            Serial1.printf("Recv ssid: %s \r\n", EEPROM.getDataPtr() + EEP_OFFSET_SSID);
        } else
        {
            Serial1.printf("Bad format: wifi ssid password setting.\r\n");
            return;
        }
          
        //password
        ptr = extract_substr(ptr + 1, EEPROM.getDataPtr() + EEP_OFFSET_PASSWORD);
        
        if (ptr)
        {
            Serial1.printf("Recv password: %s  \r\n", EEPROM.getDataPtr() + EEP_OFFSET_PASSWORD);
        } else
        {
            Serial1.printf("Bad format: wifi ssid password setting.\r\n");
            return;
        }
        
        //key
        ptr = extract_substr(ptr + 1, EEPROM.getDataPtr() + EEP_OFFSET_KEY);
        
        if (ptr)
        {
            Serial1.printf("Recv key: %s  \r\n", EEPROM.getDataPtr() + EEP_OFFSET_KEY);
        } else
        {
            Serial1.printf("Bad format: can not find key.\r\n");
            return;
        }
        
        //sn
        ptr = extract_substr(ptr + 1, EEPROM.getDataPtr() + EEP_OFFSET_SN);
        
        if (ptr)
        {
            Serial1.printf("Recv sn: %s\r\n", EEPROM.getDataPtr() + EEP_OFFSET_SN);
        } else
        {
            Serial1.printf("Bad format: can not find sn.\r\n");
            return;
        }
        
        
        config_flag = 3;
        memset(EEPROM.getDataPtr() + EEP_OFFSET_CFG_FLAG, config_flag, 1);  //set the config flag
        EEPROM.commit();
        
        
        espconn_sent(conn, "ok\r\n", 4);
        espconn_sent(conn, "ok\r\n", 4);
        espconn_sent(conn, "ok\r\n", 4);
        
        os_timer_disarm(&timer_main_conn);
        os_timer_setfn(&timer_main_conn, fire_reboot, NULL);
        os_timer_arm(&timer_main_conn, 1000, 0);
        
    }
#if 0
    else if ((pkey=os_strstr(pusrdata, device_keysn_write_req)) != NULL)
    {
        /* prevent bad guy from flashing your node without your permission */
        if ((pusrdata + length - pkey - os_strlen(device_keysn_write_req)) >= (KEY_LEN+SN_LEN) && config_flag == 2)
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
            espconn_sent(conn, "ok\r\n", 4);
            espconn_sent(conn, "ok\r\n", 4);
            

            //if (main_conn_status == DIED_IN_CONN || main_conn_status == DIED_IN_HELLO || main_conn_status == KEEP_ALIVE)
            {
                os_timer_disarm(&timer_main_conn);
                os_timer_setfn(&timer_main_conn, fire_reboot, NULL);
                os_timer_arm(&timer_main_conn, 1000, 0);
            }
        }
    }
#endif
}

/**
 * init UDP socket
 */
void user_devicefind_init(void)
{
    udp_conn.type = ESPCONN_UDP;
    udp_conn.proto.udp = (esp_udp *)os_zalloc(sizeof(esp_udp));
    udp_conn.proto.udp->local_port = 1025;
    espconn_regist_recvcb(&udp_conn, user_devicefind_recv);
    espconn_create(&udp_conn);
}

/**
 * The callback for data receiving of the main TCP socket
 * 
 * @param arg 
 * @param pusrdata 
 * @param length 
 */
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
        keepalive_last_recv_time = millis();

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

/**
 * The callback when data sent out via main TCP socket
 * 
 * @param arg 
 */
static void main_connection_sent_cb(void *arg)
{

}

/**
 * The callback when tx data has written into ESP8266's tx buffer
 * 
 * @param arg 
 */
static void main_connection_tx_write_finish_cb(void *arg)
{
    struct espconn *pespconn = arg;

    if (!tx_stream_buffer) return;

    size_t size = tx_stream_buffer->size();

    size_t size2 = size;
    if (size > 0)
    {
        txing = true;
        size2 = (((size % 16) == 0) ? (size) : (size + (16 - size % 16)));  //damn, the python crypto library only supports 16*n block length
        char *data = (char *)malloc(size2);
        os_memset(data, 0, size2);
        tx_stream_buffer->read(data, size);
        aes_crypt_cfb128(&aes_ctx, AES_ENCRYPT, size2, &iv_offset, iv, data, data);
        espconn_sent(pespconn, data, size2);
        free(data);
    } else
    {
        txing = false;
    }
}

/**
 * put a char into tx ring-buffer
 * 
 * @param c - char to send
 */
void network_putc(char c)
{
    network_puts(&c, 1);
}

/**
 * put a block of data into tx ring-buffer
 * 
 * @param data 
 * @param len 
 */
void network_puts(char *data, int len)
{
    if (main_conn_status != KEEP_ALIVE || !tx_stream_buffer) return;
    if (main_conn.state > ESPCONN_NONE)
    {
        noInterrupts();
        size_t room = tx_stream_buffer->capacity() - tx_stream_buffer->size();
        interrupts();
        
        while (room < len)
        {
            delay(10);
            noInterrupts();
            room = tx_stream_buffer->capacity() - tx_stream_buffer->size();
            interrupts();
        }

        noInterrupts();
        tx_stream_buffer->write(data, len);
        interrupts();

        if ((strchr(data, '\r') || strchr(data, '\n') || tx_stream_buffer->size() > 512) && !txing)
        {
            //os_timer_disarm(&timer_tx);
            //os_timer_setfn(&timer_tx, main_connection_sent_cb, &main_conn);
            //os_timer_arm(&timer_tx, 1, 0);
            main_connection_tx_write_finish_cb(&main_conn);
        }
    }
}

/**
 * the function which will be called when timer_keepalive_check fired 
 * to check if the socket to server is alive 
 */
static void keepalive_check_timer_fn(void *arg)
{
    if (millis() - keepalive_last_recv_time > 70000)
    {
        Serial1.println("No longer alive, reconnect...");
        main_connection_reconnect_callback(NULL, 0);
    } else
    {
        os_timer_arm(&timer_keepalive_check, 1000, 0);
    }
}

/**
 * Timer function for checking the hello response from server
 * 
 * @param arg 
 */
static void confirm_hello(void *arg)
{
    Serial1.printf("confirm hello: %d \n", get_hello);

    if (get_hello == 1)
    {
        Serial1.printf("handshake done, keep-alive\n");
        main_conn_status = KEEP_ALIVE;
        keepalive_last_recv_time = millis();
        os_timer_disarm(&timer_keepalive_check);
        os_timer_setfn(&timer_keepalive_check, keepalive_check_timer_fn, NULL);
        os_timer_arm(&timer_keepalive_check, 1000, 0);
        
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
            if (confirm_hello_retry_cnt % 10 == 0)
            {
                os_timer_setfn(&timer_confirm_hello, main_connection_send_hello, &main_conn);
            } else
            {
                os_timer_setfn(&timer_confirm_hello, confirm_hello, NULL);
            }

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
    os_timer_setfn(&timer_confirm_hello, confirm_hello, NULL);
    os_timer_arm(&timer_confirm_hello, 100, 0);
}

/**
 * The callback when main TCP socket has been open and connected with server
 * 
 * @param arg 
 */
void main_connection_connected_callback(void *arg)
{
    struct espconn *pespconn = (struct espconn *)arg;

    Serial1.println("main conn connected");
    main_conn_status = CONNECTED;
    main_conn_retry_cnt = 0;
    espconn_regist_recvcb(pespconn, main_connection_recv_cb);
    espconn_regist_sentcb(pespconn, main_connection_sent_cb);

    espconn_set_opt(pespconn, 0x0c);  //enable the 2920 write buffer, enable keep-alive detection

    espconn_regist_write_finish(pespconn, main_connection_tx_write_finish_cb); // register write finish callback

    /* send hello */
    confirm_hello_retry_cnt = 0;
    main_connection_send_hello(arg);
    main_conn_status = WAIT_HELLO_DONE;

}

/**
 * The callback when some error or exception happened with the main TCP socket
 * 
 * @param arg 
 * @param err 
 */
void main_connection_reconnect_callback(void *arg, int8_t err)
{
    Serial1.printf("main conn re-conn, err: %d\n", err);

    os_timer_disarm(&timer_main_conn);
    os_timer_setfn(&timer_main_conn, main_connection_init, NULL);
    os_timer_arm(&timer_main_conn, 1000, 0);
    main_conn_status = WAIT_CONN_DONE;
}

/**
 * confirm main connection is connected
 */
static void main_connection_confirm_timer_fn(void *arg)
{
    if (main_conn_status == WAIT_CONN_DONE)
    {
        espconn_disconnect(&main_conn);
        os_timer_disarm(&timer_main_conn_reconn);
        os_timer_setfn(&timer_main_conn_reconn, main_connection_init, NULL);
        os_timer_arm(&timer_main_conn_reconn, 1, 0);
    }
}

/**
 * init the main TCP socket
 */
void main_connection_init(void *arg)
{
    Serial1.printf("main_connection_init.\r\n");
    
    //cant connect to server after 1min, maybe should re-join the router to renew the IP too.
    if (++main_conn_retry_cnt >= 60)
    {
        main_conn_status = DIED_IN_CONN;
        
        os_timer_disarm(&timer_main_conn);
        os_timer_setfn(&timer_main_conn, fire_reboot, NULL);
        os_timer_arm(&timer_main_conn, 1000, 0);
        
        return;
    }

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
        
    os_timer_disarm(&timer_main_conn_reconn);
    os_timer_setfn(&timer_main_conn_reconn, main_connection_confirm_timer_fn, NULL);
    os_timer_arm(&timer_main_conn_reconn, 10000, 0);
}


/**
 * begin to establish network
 */
void network_normal_mode(int config_flag)
{
    Serial1.printf("start to establish network connection.\r\n");
    
    /* enable the station mode */
    wifi_set_opmode(0x01);

    struct station_config config;
    wifi_station_get_config(&config);
    
    if (config_flag >= 2)
    {
        if (config_flag == 3)
        {
            os_strncpy(config.ssid, EEPROM.getDataPtr() + EEP_OFFSET_SSID, 32);
            os_strncpy(config.password, EEPROM.getDataPtr() + EEP_OFFSET_PASSWORD, 64);
            wifi_station_set_config(&config);
        }
        
        memset(EEPROM.getDataPtr() + EEP_OFFSET_CFG_FLAG, 0, 1);  //clear the config flag
        EEPROM.commit();

        Serial1.printf("Config flag has been reset to 0.\r\n");
    }
    
    Serial1.printf("connect to ssid %s with passwd %s\r\n", config.ssid, config.password);
    wifi_station_disconnect();
    wifi_station_connect(); //connect with saved config in flash

    /* check IP */
    uint8_t connect_status = wifi_station_get_connect_status();
    const char *get_ip_state[6] = {
        "IDLE",
        "CONNECTING",
        "WRONG_PASSWORD",
        "NO_AP_FOUND",
        "CONNECT_FAIL",
        "GOT_IP"
    };
    int wait_sec = 0;
    while (connect_status != STATION_GOT_IP)
    {
        Serial1.printf("Wait getting ip, state: %s\n", get_ip_state[connect_status]);
        delay(1000);
        connect_status = wifi_station_get_connect_status();

        if (++wait_sec > 60)
        {
            main_conn_status = DIED_IN_GET_IP;
            return;
        }
        if (digitalRead(FUNCTION_KEY) == 0)  //user pressed the function key to enter config mode
        {
            return;
        }
    }
    
    struct ip_info ip;
    wifi_get_ip_info(STATION_IF, &ip);
    Serial1.printf("Done. IP: " IPSTR "\r\n", IP2STR(&ip.ip));

    /* register a device-find responder at UDP port 1025 */
    user_devicefind_init();

    main_conn_status = WAIT_CONN_DONE;

    /* establish the connection with server */
    main_connection_init(NULL);
}


/**
 * Enter AP mode then waiting to be configured
 */
void network_config_mode()
{
    Serial1.printf("enter AP mode ... \r\n");
    
    memset(EEPROM.getDataPtr() + EEP_OFFSET_CFG_FLAG, 2, 1);  //upgrade the config flag
    EEPROM.commit();

    wifi_set_opmode(0x02); //AP mode
    
    struct softap_config *config = (struct softap_config *)malloc(sizeof(struct softap_config));
    char hwaddr[6];
    char ssid[16];

    wifi_get_macaddr(SOFTAP_IF, hwaddr);
    wifi_softap_get_config_default(config);
    os_sprintf(ssid, "PionOne_%02X%02X%02X", hwaddr[3], hwaddr[4], hwaddr[5]);
    memcpy(config->ssid, ssid, 15);
    config->ssid_len = strlen(config->ssid);
    wifi_softap_set_config(config);
    
    Serial1.printf("SSID: %s\r\n", config->ssid);

    /* register a device-find responder at UDP port 1025 */
    user_devicefind_init();
    
}


/**
 * setup the network when system startup
 */
void network_setup()
{
#if ENABLE_DEBUG_ON_UART1
    Serial1.begin(74880);
    //Serial1.setDebugOutput(true);
    Serial1.println("\n\n"); 
#endif

    if (!rx_stream_buffer) rx_stream_buffer = new CircularBuffer(256);
    if (!tx_stream_buffer) tx_stream_buffer = new CircularBuffer(1024);

    struct rst_info *reason = system_get_rst_info();
    Serial1.printf("Boot reason: %d\n", reason->flag);
    Serial1.printf("Node name: %s\n", NODE_NAME);
    Serial1.printf("Chip id: 0x%08x\n", system_get_chip_id());
    
    Serial1.print("Free heap size: ");
    Serial1.println(system_get_free_heap_size());
    
    /* get key and sn */
    char buff[33];
        
    //os_memcpy(EEPROM.getDataPtr() + EEP_OFFSET_KEY, "9ed12049da8c1eb42a9872bb27cfb02e", 32);
    format_sn(EEPROM.getDataPtr() + EEP_OFFSET_KEY, (uint8_t *)buff);
    Serial1.printf("Private key: %s\n", buff);
    
    //os_memcpy(EEPROM.getDataPtr() + EEP_OFFSET_SN, "52f4370b14aedea33416db677b1c5726", 32);
    format_sn(EEPROM.getDataPtr() + EEP_OFFSET_SN, (uint8_t *)buff);
    Serial1.printf("Node SN: %s\n", buff);
    
    Serial1.flush();
    delay(1000);  //should delay some time to wait all settled before wifi_* API calls.
    
    //start the forever timer to drive the status leds
    os_timer_disarm(&timer_network_status_indicate);
    os_timer_setfn(&timer_network_status_indicate, network_status_indicate_timer_fn, NULL);
    os_timer_arm(&timer_network_status_indicate, 1, false);

    /* get the smart config flag */
    uint8_t config_flag = *(EEPROM.getDataPtr() + EEP_OFFSET_CFG_FLAG);

    if (config_flag == 1)
    {
        main_conn_status = WAIT_CONFIG;
        network_config_mode();
    } else
    {
        main_conn_status = WAIT_GET_IP;
        network_normal_mode(config_flag);
    }
}

/**
 * Blink the LEDs according to the status of network connection
 */
void network_status_indicate_timer_fn(void *arg)
{
    static uint8_t last_main_conn_status = 0xff;
    static uint16_t blink_pattern_cnt = 0;
    static uint8_t brightness_dir = 0;

    switch (main_conn_status)
    {
    case WAIT_CONFIG:
        //os_timer_arm(&timer_network_status_indicate, 100, false);
        //digitalWrite(STATUS_LED, ~digitalRead(STATUS_LED));
        os_timer_arm(&timer_network_status_indicate, 60, false);
        if (main_conn_status != last_main_conn_status)
        {
            blink_pattern_cnt = 0;
            brightness_dir = 0;
        }
        else if (brightness_dir == 0)
        {
            blink_pattern_cnt += 50;
            if (blink_pattern_cnt > 800)
            {
                blink_pattern_cnt = 800;
                brightness_dir = 1;
            }
        }
        else if (brightness_dir == 1)
        {
            blink_pattern_cnt -= 40;
            if (blink_pattern_cnt < 40)
            {
                blink_pattern_cnt = 0;
                brightness_dir = 0;
            }
        }
        analogWrite(STATUS_LED, blink_pattern_cnt);
        break;
    case WAIT_GET_IP:
        if (main_conn_status != last_main_conn_status)
        {
            os_timer_arm(&timer_network_status_indicate, 50, false);
            digitalWrite(STATUS_LED, 1);
            blink_pattern_cnt = 1;
        }else
        {
            int timeout = 50;
            if (++blink_pattern_cnt >= 4)
            {
                timeout = 1000;
                blink_pattern_cnt = 0;
            }
            os_timer_arm(&timer_network_status_indicate, timeout, false);
            digitalWrite(STATUS_LED, ~digitalRead(STATUS_LED));
        }
        break;
    case DIED_IN_GET_IP:
        pinMode(STATUS_LED_TXD1, OUTPUT);
        digitalWrite(STATUS_LED_TXD1, 0);
        digitalWrite(STATUS_LED, 1);
        break;
    case WAIT_CONN_DONE:
    case WAIT_HELLO_DONE:
        if (main_conn_status != last_main_conn_status)
        {
            os_timer_arm(&timer_network_status_indicate, 50, false);
            analogWrite(STATUS_LED, 0);
            digitalWrite(STATUS_LED, 1);
        }else
        {
            os_timer_arm(&timer_network_status_indicate,(digitalRead(STATUS_LED) > 0 ? 1000 : 50), false);
            digitalWrite(STATUS_LED, ~digitalRead(STATUS_LED));
        }
        break;
    case DIED_IN_CONN:
        Serial1.printf("The main connection died after 5 retry\n");
        Serial1.println("Please check server's ip and port, also check AccessToken\n");
        pinMode(STATUS_LED_TXD1, OUTPUT);
        digitalWrite(STATUS_LED_TXD1, 0);
        digitalWrite(STATUS_LED, 1);
        break;
    case DIED_IN_HELLO:
        Serial1.printf("No hello ack from server after 5 retry\n");
        Serial1.println("Please check server's ip and port, also check AccessToken\n");
        pinMode(STATUS_LED_TXD1, OUTPUT);
        digitalWrite(STATUS_LED_TXD1, 0);
        digitalWrite(STATUS_LED, 1);
        break;
    case CONNECTED:
    case KEEP_ALIVE:
        os_timer_arm(&timer_network_status_indicate, 1000, false);
        digitalWrite(STATUS_LED, ~digitalRead(STATUS_LED));
        break;
    default:
        break;
    }

    last_main_conn_status = main_conn_status;
}



