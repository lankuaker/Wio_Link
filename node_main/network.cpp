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

#define KEY_LEN             32
#define SN_LEN              32

uint8_t conn_status[2] = { WAIT_GET_IP, WAIT_GET_IP };
static int conn_retry_cnt[2] = {0};
static uint8_t get_hello[2] = {0};
static uint8_t confirm_hello_retry_cnt[2] = {0};
uint32_t keepalive_last_recv_time[2] = {0};

static struct espconn udp_conn;
struct espconn tcp_conn[2];
static struct _esp_tcp tcp_conn_tcp_s[2];
static os_timer_t timer_conn[2];
static os_timer_t timer_network_status_indicate[2];
static os_timer_t timer_confirm_hello[2];
static os_timer_t timer_tx[2];
static os_timer_t timer_keepalive_check[2];
const char *conn_name[2] = {"data", "ota" };

const char *device_find_request = "Node?";
const char *blank_device_find_request = "Blank?";
const char *device_keysn_write_req = "KeySN: ";
const char *ap_config_req = "APCFG: ";
const char *reboot_req = "REBOOT";

CircularBuffer *data_stream_rx_buffer = NULL;
CircularBuffer *data_stream_tx_buffer = NULL;
CircularBuffer *ota_stream_rx_buffer = NULL;
CircularBuffer *ota_stream_tx_buffer = NULL;

static aes_context aes_ctx[2];
static int iv_offset[2] = {0};
static unsigned char iv[2][16];
static bool txing[2] = {false};

extern "C" struct rst_info* system_get_rst_info(void);
void connection_init(void *arg);
void connection_send_hello(void *arg);
void connection_reconnect_callback(void *arg, int8_t err);
void network_status_indicate_timer_fn(void *arg);
void ota_conn_status_indicate_timer_fn(void *arg);

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
    uint8_t *ptr = os_strchr(input, '\t');
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
 * extract parts of ip v4 from a string
 * 
 * @param input 
 * @param output 
 * 
 * @return bool 
 */
bool extract_ip(uint8_t *input, uint8_t *output)
{
    uint8_t *ptr;
    char num[4];
    int i = 0;
    
    while ((ptr = os_strchr(input, '.')) != NULL)
    {
        i++;
        os_memset(num, 0, 4);
        os_memcpy(num, input, (ptr - input));
        *output++ = atoi((const char *)num);
        input = ptr + 1;
    }
    
    if (i<3)
    {
        return false;
    } else
    {
        *output = atoi((const char *)input);
        return true;
    }
}

/**
 * print the data socket online status to ota socket stream
 */
void print_online_status()
{
    network_puts(ota_stream_tx_buffer, "{\"msg_type\":\"online_status\",\"msg\":", 34);
    if (conn_status[0] == KEEP_ALIVE)
    {
        network_puts(ota_stream_tx_buffer, "1", 1);
    } else
    {
        network_puts(ota_stream_tx_buffer, "0", 1);
    }
    network_puts(ota_stream_tx_buffer, "}\r\n", 3);
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
    
    remot_info *premot = NULL;

    espconn_get_connection_info(conn, &premot, 0);  // get sender data (source IP)
    os_memcpy(udp_conn.proto.udp->remote_ip, premot->remote_ip, 4);
    udp_conn.proto.udp->remote_port = premot->remote_port;
    
    // shows correct source IP
    Serial1.printf("UDP remote: " IPSTR ":%d\n",  IP2STR(udp_conn.proto.udp->remote_ip), udp_conn.proto.udp->remote_port);
    
    // shows always the current device IP; never a broadcast address
    Serial1.printf("UDP local:  " IPSTR ":%d\n",  IP2STR(conn->proto.udp->local_ip), conn->proto.udp->local_port);

#if 0
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
#endif
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
                   buff_sn, MAC2STR(hwaddr), IP2STR(conn->proto.udp->local_ip));

        Serial1.printf("%s", device_desc);
        length = os_strlen(device_desc);
        espconn_sendto(&udp_conn, device_desc, length);
        
        delete[] device_desc;
        delete[] buff_sn;
    }
    else if ((pkey = os_strstr(pusrdata, reboot_req)) != NULL && config_flag == 2)
    {
        os_timer_disarm(&timer_conn[0]);
        os_timer_setfn(&timer_conn[0], fire_reboot, NULL);
        os_timer_arm(&timer_conn[0], 1, 0);
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
        
        //ip data
        ptr = extract_substr(ptr + 1, EEPROM.getDataPtr() + EEP_DATA_SERVER_IP+4);
        
        if (ptr)
        {
            Serial1.printf("Recv data server ip: %s\r\n", EEPROM.getDataPtr() + EEP_DATA_SERVER_IP+4);
            if (!extract_ip(EEPROM.getDataPtr() + EEP_DATA_SERVER_IP + 4, EEPROM.getDataPtr() + EEP_DATA_SERVER_IP))
            {
                Serial1.printf("Fail to extract data server ip.\r\n");
            }
        } else
        {
            Serial1.printf("Bad format: can not find data server ip.\r\n");
            return;
        }

        //ip ota
        ptr = extract_substr(ptr + 1, EEPROM.getDataPtr() + EEP_OTA_SERVER_IP+4);
        
        if (ptr)
        {
            Serial1.printf("Recv ota server ip: %s\r\n", EEPROM.getDataPtr() + EEP_OTA_SERVER_IP+4);
            if (!extract_ip(EEPROM.getDataPtr() + EEP_OTA_SERVER_IP + 4, EEPROM.getDataPtr() + EEP_OTA_SERVER_IP))
            {
                Serial1.printf("Fail to extract data server ip.\r\n");
            }
        } else
        {
            Serial1.printf("Bad format: can not find ota server ip.\r\n");
            return;
        }
        
        
        
        config_flag = 3;
        memset(EEPROM.getDataPtr() + EEP_OFFSET_CFG_FLAG, config_flag, 1);  //set the config flag
        EEPROM.commit();
        
        
        espconn_sendto(&udp_conn, "ok\r\n", 4);
        espconn_sendto(&udp_conn, "ok\r\n", 4);
        espconn_sendto(&udp_conn, "ok\r\n", 4);
        
        os_timer_disarm(&timer_conn[0]);
        os_timer_setfn(&timer_conn[0], fire_reboot, NULL);
        os_timer_arm(&timer_conn[0], 1000, 0);
        
    }
}

/**
 * init UDP socket
 */
void local_udp_config_port_init(void)
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
static void connection_recv_cb(void *arg, char *pusrdata, unsigned short length)
{
    struct espconn *pespconn = (struct espconn *)arg;
    int index = (pespconn == (&tcp_conn[0])) ? 0 : 1;
    CircularBuffer *rx_buffer = (index == 0) ? data_stream_rx_buffer : ota_stream_rx_buffer;
    CircularBuffer *tx_buffer = (index == 0) ? data_stream_tx_buffer : ota_stream_tx_buffer;
    
    char *pstr = NULL;
    size_t room;

    switch (conn_status[index])
    {
    case WAIT_HELLO_DONE:
        if ((pstr = strstr(pusrdata, "sorry")) != NULL)
        {
            get_hello[index] = 2;
        } else
        {
            aes_init(&aes_ctx[index]);
            aes_setkey_enc(&aes_ctx[index], EEPROM.getDataPtr() + EEP_OFFSET_KEY, KEY_LEN*8);
            memcpy(iv[index], pusrdata, 16);
            aes_crypt_cfb128(&aes_ctx[index], AES_DECRYPT, length - 16, &iv_offset[index], iv[index], pusrdata + 16, pusrdata);
            if (os_strncmp(pusrdata, "hello", 5) == 0)
            {
                get_hello[index] = 1;
            }
        }

        break;
    case KEEP_ALIVE:
        aes_crypt_cfb128(&aes_ctx[index], AES_DECRYPT, length, &iv_offset[index], iv[index], pusrdata, pusrdata);

        Serial1.printf("%s conn: ", conn_name[index]);
        Serial1.println(pusrdata);
        keepalive_last_recv_time[index] = millis();

        if (os_strncmp(pusrdata, "##PING##", 8) == 0 && rx_buffer)
        {
            if (index == 0)
            {
                network_puts(tx_buffer, "##ALIVE##\r\n", 11);
            } else
            {
                print_online_status();
            }

            return;
        }

        if (!rx_buffer) return;

        //Serial1.printf("recv %d data\n", length);
        room = rx_buffer->capacity()-rx_buffer->size();
        length = os_strlen(pusrdata);  //filter out the padding 0s
        if ( room > 0 )
        {
            rx_buffer->write(pusrdata, (room>length?length:room));
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
static void connection_sent_cb(void *arg)
{

}

/**
 * The callback when tx data has written into ESP8266's tx buffer
 * 
 * @param arg 
 */
static void connection_tx_write_finish_cb(void *arg)
{
    struct espconn *p_conn = (struct espconn *)arg;
    int index = (p_conn == (&tcp_conn[0])) ? 0 : 1;
    
    CircularBuffer *tx_buffer = (index == 0) ? data_stream_tx_buffer : ota_stream_tx_buffer;

    if (!tx_buffer) return;

    size_t size = tx_buffer->size();

    size_t size2 = size;
    if (size > 0)
    {
        txing[index] = true;
        size2 = (((size % 16) == 0) ? (size) : (size + (16 - size % 16)));  //damn, the python crypto library only supports 16*n block length
        char *data = (char *)malloc(size2);
        os_memset(data, 0, size2);
        tx_buffer->read(data, size);
        aes_crypt_cfb128(&aes_ctx[index], AES_ENCRYPT, size2, &iv_offset[index], iv[index], data, data);
        espconn_send(p_conn, data, size2);
        free(data);
    } else
    {
        txing[index] = false;
    }
}

/**
 * put a char into tx ring-buffer
 * 
 * @param c - char to send
 */
void network_putc(CircularBuffer *tx_buffer, char c)
{
    network_puts(tx_buffer, &c, 1);
}

/**
 * put a block of data into tx ring-buffer
 * 
 * @param data 
 * @param len 
 */
void network_puts(CircularBuffer *tx_buffer, char *data, int len)
{
    int index = (tx_buffer == data_stream_tx_buffer) ? 0 : 1;
    int tx_threshold = (index == 0) ? 512 : 32;
    
    if (conn_status[index] != KEEP_ALIVE || !tx_buffer) return;
    if (tcp_conn[index].state > ESPCONN_NONE)
    {
        noInterrupts();
        size_t room = tx_buffer->capacity() - tx_buffer->size();
        interrupts();
        
        while (room < len)
        {
            delay(10);
            noInterrupts();
            room = tx_buffer->capacity() - tx_buffer->size();
            interrupts();
        }

        noInterrupts();
        tx_buffer->write(data, len);
        interrupts();

        if ((strchr(data, '\r') || strchr(data, '\n') || tx_buffer->size() > tx_threshold) && !txing[index])
        {
            //os_timer_disarm(&timer_tx);
            //os_timer_setfn(&timer_tx, main_connection_sent_cb, &tcp_conn[0]);
            //os_timer_arm(&timer_tx, 1, 0);
            connection_tx_write_finish_cb(&tcp_conn[index]);
        }
    }
}

/**
 * the function which will be called when timer_keepalive_check fired 
 * to check if the socket to server is alive 
 */
static void keepalive_check_timer_fn(void *arg)
{
    struct espconn *p_conn = (struct espconn *)arg;
    int index = (p_conn == (&tcp_conn[0])) ? 0 : 1;
    
    if (millis() - keepalive_last_recv_time[index] > 70000)
    {
        Serial1.printf("%s conn no longer alive, reconnect...\r\n", conn_name[index]);
        connection_reconnect_callback(p_conn, 0);
    } else
    {
        os_timer_arm(&timer_keepalive_check[index], 1000, 0);
    }
}

/**
 * Timer function for checking the hello response from server
 * 
 * @param arg 
 */
static void connection_confirm_hello(void *arg)
{
    struct espconn *p_conn = (struct espconn *)arg;
    int index = (p_conn == (&tcp_conn[0])) ? 0 : 1;
    
    Serial1.printf("%s conn confirm hello: %d \n", conn_name[index], get_hello[index]);

    if (get_hello[index] == 1)
    {
        Serial1.printf("%s conn handshake done, keep-alive\n", conn_name[index]);
        conn_status[index] = KEEP_ALIVE;
        keepalive_last_recv_time[index] = millis();
        os_timer_disarm(&timer_keepalive_check[index]);
        os_timer_setfn(&timer_keepalive_check[index], keepalive_check_timer_fn, p_conn);
        os_timer_arm(&timer_keepalive_check[index], 1000, 0);
        
        uint8_t ota_result_flag = *(EEPROM.getDataPtr() + EEP_OTA_RESULT_FLAG);
        
        if (ota_result_flag == 1 && index == 1)
        {
            memset(EEPROM.getDataPtr() + EEP_OTA_RESULT_FLAG, 0, 1);
            EEPROM.commit();
            for (int i = 0; i < 2;i++)
            {
                network_puts(ota_stream_tx_buffer, "{\"msg_type\":\"ota_result\",\"msg\":\"success\"}\r\n", 43);
            }
        } else if(ota_result_flag == 2 && index == 1)
        {
            memset(EEPROM.getDataPtr() + EEP_OTA_RESULT_FLAG, 0, 1);
            EEPROM.commit();
            network_puts(ota_stream_tx_buffer, "{\"msg_type\":\"ota_result\",\"msg\":\"fail\"}\r\n", 40);
        }

    } else if (get_hello[index] == 2)
    {
        Serial1.printf("%s conn handshake: sorry from server\n", conn_name[index]);
        conn_status[index] = DIED_IN_HELLO;
    } else
    {
        if (++confirm_hello_retry_cnt[index] > 60)
        {
            if (index == 0)
            {
                conn_status[0] = DIED_IN_HELLO;
                os_timer_disarm(&timer_conn[0]);
                os_timer_setfn(&timer_conn[0], fire_reboot, &tcp_conn[0]);
                os_timer_arm(&timer_conn[0], 1000, 0);
                return;
            }
            else
            {
                conn_status[1] = WAIT_CONN_DONE;
                os_timer_disarm(&timer_conn[1]);
                os_timer_setfn(&timer_conn[1], connection_init, &tcp_conn[1]);
                os_timer_arm(&timer_conn[1], 1000, 0);
                return;
            }
        } 
        
        if (confirm_hello_retry_cnt[index] % 10 == 0)
        {
            os_timer_setfn(&timer_confirm_hello[index], connection_send_hello, p_conn);
        } else
        {
            os_timer_setfn(&timer_confirm_hello[index], connection_confirm_hello, p_conn);
        }

        os_timer_arm(&timer_confirm_hello[index], 1000, 0);
    }
}

/**
 * start the handshake with server
 * node will send itself's sn number and a signature signed with
 * its private key to server
 * @param
 */
void connection_send_hello(void *arg)
{
    struct espconn *pespconn = (struct espconn *)arg;
    int index = (pespconn == (&tcp_conn[0])) ? 0 : 1;
    
    uint8_t hmac_hash[32];

    uint8_t *buff = os_malloc(SN_LEN + 32);
    if (!buff)
    {
        conn_status[index] = DIED_IN_HELLO;
        return;
    }
    //EEPROM.getDataPtr() + EEP_OFFSET_SN
    os_memcpy(buff, EEPROM.getDataPtr() + EEP_OFFSET_SN, SN_LEN);

    sha256_hmac(EEPROM.getDataPtr() + EEP_OFFSET_KEY, KEY_LEN, buff, SN_LEN, hmac_hash, 0);

    os_memcpy(buff + SN_LEN, hmac_hash, 32);

    espconn_send(pespconn, buff, SN_LEN+32);

    os_free(buff);

    get_hello[index] = 0;
    os_timer_disarm(&timer_confirm_hello[index]);
    os_timer_setfn(&timer_confirm_hello[index], connection_confirm_hello, pespconn);
    os_timer_arm(&timer_confirm_hello[index], 100, 0);
}


/**
 * The callback when the TCP socket has been open and connected with server
 * 
 * @param arg 
 */
void connection_connected_callback(void *arg)
{
    struct espconn *p_conn = (struct espconn *)arg;
    int index = (p_conn == (&tcp_conn[0])) ? 0 : 1;

    Serial1.printf("%s conn connected\r\n", conn_name[index]);
    conn_retry_cnt[index] = 0;
    espconn_regist_recvcb(p_conn, connection_recv_cb);
    espconn_regist_sentcb(p_conn, connection_sent_cb);

    espconn_set_opt(p_conn, 0x0c);  //enable the 2920 write buffer, enable keep-alive detection

    espconn_regist_write_finish(p_conn, connection_tx_write_finish_cb); // register write finish callback

    /* send hello */
    confirm_hello_retry_cnt[index] = 0;
    connection_send_hello(p_conn);
    conn_status[index] = WAIT_HELLO_DONE;

}


/**
 * The callback when some error or exception happened with the TCP socket
 * 
 * @param arg 
 * @param err 
 */
void connection_reconnect_callback(void *arg, int8_t err)
{
    struct espconn *p_conn = (struct espconn *)arg;
    int index = (p_conn == (&tcp_conn[0])) ? 0 : 1;
    
    Serial1.printf("%s conn re-conn, err: %d\n", conn_name[index], err);

    os_timer_disarm(&timer_conn[index]);
    os_timer_setfn(&timer_conn[index], connection_init, &tcp_conn[index]);
    os_timer_arm(&timer_conn[index], 1000, 0);
    conn_status[index] = WAIT_CONN_DONE;
}


/**
 * confirm the connection is connected
 */
static void connection_confirm_timer_fn(void *arg)
{
    struct espconn *p_conn = (struct espconn *)arg;
    int index = (p_conn == (&tcp_conn[0])) ? 0 : 1;
    
    if (conn_status[index] == WAIT_CONN_DONE)
    {
        espconn_disconnect(&tcp_conn[index]);
        os_timer_disarm(&timer_conn[index]);
        os_timer_setfn(&timer_conn[index], connection_init, &tcp_conn[index]);
        os_timer_arm(&timer_conn[index], 1, 0);
    }
}

/**
 * init the TCP socket
 */
void connection_init(void *arg)
{
    struct espconn *p_conn = (struct espconn *)arg;
    int index = (p_conn == (&tcp_conn[0])) ? 0 : 1;
    char *server_ip = (index == 0) ? (EEPROM.getDataPtr() + EEP_DATA_SERVER_IP) : (EEPROM.getDataPtr() + EEP_OTA_SERVER_IP);
    
    Serial1.printf("%s connection init.\r\n", conn_name[index]);
    
    //cant connect to server after 1min, maybe should re-join the router to renew the IP too.
    if (++conn_retry_cnt[index] >= 60)
    {
        conn_status[index] = DIED_IN_CONN;
        
        if (index == 0)
        {
            os_timer_disarm(&timer_conn[0]);
            os_timer_setfn(&timer_conn[0], fire_reboot, &tcp_conn[index]);
            os_timer_arm(&timer_conn[0], 1000, 0);
            return;
        }
    }

    tcp_conn[index].type = ESPCONN_TCP;
    tcp_conn[index].state = ESPCONN_NONE;
    tcp_conn[index].proto.tcp = &tcp_conn_tcp_s[index];
    //const char server_ip[4] = SERVER_IP;
    os_memcpy(tcp_conn[index].proto.tcp->remote_ip, server_ip, 4);
    tcp_conn[index].proto.tcp->remote_port = (index == 0) ? DATA_SERVER_PORT : OTA_SERVER_PORT;
    tcp_conn[index].proto.tcp->local_port = espconn_port();
    espconn_regist_connectcb(&tcp_conn[index], connection_connected_callback);
    espconn_regist_reconcb(&tcp_conn[index], connection_reconnect_callback);
    espconn_connect(&tcp_conn[index]);
        
    os_timer_disarm(&timer_conn[index]);
    os_timer_setfn(&timer_conn[index], connection_confirm_timer_fn, &tcp_conn[index]);
    os_timer_arm(&timer_conn[index], 10000, 0);
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
            conn_status[0] = DIED_IN_GET_IP;
            os_timer_disarm(&timer_conn[0]);
            os_timer_setfn(&timer_conn[0], fire_reboot, &tcp_conn[0]);
            os_timer_arm(&timer_conn[0], 1, 0);
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

    /* open the config interface at UDP port 1025 */
    local_udp_config_port_init();

    conn_status[0] = WAIT_CONN_DONE;
    conn_status[1] = WAIT_CONN_DONE;

    /* establish the connection with server */
    connection_init(&tcp_conn[0]);
    connection_init(&tcp_conn[1]);
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
    os_sprintf(ssid, "WioLink_%02X%02X%02X", hwaddr[3], hwaddr[4], hwaddr[5]);
    memcpy(config->ssid, ssid, 15);
    config->ssid_len = strlen(config->ssid);
    wifi_softap_set_config(config);
    
    Serial1.printf("SSID: %s\r\n", config->ssid);

    /* open the config interface at UDP port 1025 */
    local_udp_config_port_init();
    
    /* list connected devices */
    struct station_info * station;
    while (1)
    {
        station = wifi_softap_get_station_info();
        while(station)
        {
            Serial1.printf("bssid : " MACSTR ", ip : " IPSTR "\r\n",   MAC2STR(station->bssid), IP2STR(&station->ip)); 
            station = STAILQ_NEXT(station, next);  
        }
        wifi_softap_free_station_info();    // Free it by calling functions
        delay(2000);
    }
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
#else
    pinMode(STATUS_LED_TXD1, OUTPUT);
    digitalWrite(STATUS_LED_TXD1, 1);
#endif

    if (!data_stream_rx_buffer) data_stream_rx_buffer = new CircularBuffer(512);
    if (!data_stream_tx_buffer) data_stream_tx_buffer = new CircularBuffer(1024);

    if (!ota_stream_rx_buffer) ota_stream_rx_buffer = new CircularBuffer(8);
    if (!ota_stream_tx_buffer) ota_stream_tx_buffer = new CircularBuffer(64);

    struct rst_info *reason = system_get_rst_info();
    const char *boot_reason_desc[7] = {
        "DEFAULT",
        "WDT",
        "EXCEPTION",
        "SOFT_WDT",
        "SOFT_RESTART",
        "DEEP_SLEEP_AWAKE",
        "EXT_SYS"
    }; 
    Serial1.printf("Boot reason: %s\n", boot_reason_desc[reason->reason]);
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
    
    //
    Serial1.printf("Data server: " IPSTR "\r\n", IP2STR((uint32 *)(EEPROM.getDataPtr() + EEP_DATA_SERVER_IP)));

    Serial1.printf("OTA server: " IPSTR "\r\n", IP2STR((uint32 *)(EEPROM.getDataPtr() + EEP_OTA_SERVER_IP)));
    
    Serial1.printf("OTA result flag: %d\r\n", *(EEPROM.getDataPtr() + EEP_OTA_RESULT_FLAG));
    
    Serial1.flush();
    delay(1000);  //should delay some time to wait all settled before wifi_* API calls.
    
    //start the forever timer to drive the status leds
    os_timer_disarm(&timer_network_status_indicate[0]);
    os_timer_setfn(&timer_network_status_indicate[0], network_status_indicate_timer_fn, NULL);
    os_timer_arm(&timer_network_status_indicate[0], 1, false);
    
#if !ENABLE_DEBUG_ON_UART1
    os_timer_disarm(&timer_network_status_indicate[1]);
    os_timer_setfn(&timer_network_status_indicate[1], ota_conn_status_indicate_timer_fn, NULL);
    os_timer_arm(&timer_network_status_indicate[1], 1, false);
#endif    

    /* get the smart config flag */
    uint8_t config_flag = *(EEPROM.getDataPtr() + EEP_OFFSET_CFG_FLAG);

    if (config_flag == 1)
    {
        conn_status[0] = WAIT_CONFIG;
        network_config_mode();
    } else
    {
        conn_status[0] = WAIT_GET_IP;
        conn_status[1] = WAIT_GET_IP;
        network_normal_mode(config_flag);
    }
}

/**
 * Blink the LEDs according to the status of network connection
 */
void network_status_indicate_timer_fn(void *arg)
{
    static uint8_t last_main_status = 0xff;
    static uint16_t blink_pattern_cnt = 0;
    static uint8_t brightness_dir = 0;

    switch (conn_status[0])
    {
    case WAIT_CONFIG:
        //os_timer_arm(&timer_network_status_indicate[0], 100, false);
        //digitalWrite(STATUS_LED, ~digitalRead(STATUS_LED));
        os_timer_arm(&timer_network_status_indicate[0], 60, false);
        if (conn_status[0] != last_main_status)
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
        if (conn_status[0] != last_main_status)
        {
            os_timer_arm(&timer_network_status_indicate[0], 50, false);
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
            os_timer_arm(&timer_network_status_indicate[0], timeout, false);
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
        if (conn_status[0] != last_main_status)
        {
            os_timer_arm(&timer_network_status_indicate[0], 50, false);
            analogWrite(STATUS_LED, 0);
            digitalWrite(STATUS_LED, 1);
        }else
        {
            os_timer_arm(&timer_network_status_indicate[0],(digitalRead(STATUS_LED) > 0 ? 1000 : 50), false);
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
        os_timer_arm(&timer_network_status_indicate[0], 1000, false);
        digitalWrite(STATUS_LED, ~digitalRead(STATUS_LED));
        break;
    default:
        break;
    }

    last_main_status = conn_status[0];
}

void ota_conn_status_indicate_timer_fn(void *arg)
{
#if !ENABLE_DEBUG_ON_UART1
    static uint8_t last_ota_conn_status = 0xff;

    switch (conn_status[1] )
    {
    case WAIT_CONN_DONE:
    case WAIT_HELLO_DONE:
        if (conn_status[1] != last_ota_conn_status)
        {
            os_timer_arm(&timer_network_status_indicate[1], 50, false);
            digitalWrite(STATUS_LED_TXD1, 0);
        }else
        {
            os_timer_arm(&timer_network_status_indicate[1],(digitalRead(STATUS_LED_TXD1) > 0 ? 50 : 1000), false);
            digitalWrite(STATUS_LED_TXD1, ~digitalRead(STATUS_LED_TXD1));
        }
        break;
    case DIED_IN_CONN:
    case DIED_IN_HELLO:
        os_timer_arm(&timer_network_status_indicate[1], 50, false);
        digitalWrite(STATUS_LED_TXD1, 0);
        break;
    case CONNECTED:
    case KEEP_ALIVE:
        os_timer_arm(&timer_network_status_indicate[1], 1000, false);
        digitalWrite(STATUS_LED_TXD1, ~digitalRead(STATUS_LED_TXD1));
        break;
    default:
        os_timer_arm(&timer_network_status_indicate[1], 50, false);
        break;
    }

    last_ota_conn_status = conn_status[1] ;
#endif
}





