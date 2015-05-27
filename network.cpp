
#include "esp8266.h"
#include "Arduino.h"
#include "cbuf.h"
#include "network.h"

enum
{
    WAIT_SMARTCONFIG_DONE, SMARTCONFIG_DONE
};


static uint8_t smartconfig_status = WAIT_SMARTCONFIG_DONE;
uint8_t main_conn_status = WAIT_CONN_DONE;
static uint8_t main_conn_retry_cnt = 0;
static bool get_hello = false;
static uint8_t confirm_hello_retry_cnt = 0;
static os_event_t update_network_task_q;

const char *device_find_request = "Node112233445566?";
static struct espconn udp_conn;
static struct espconn main_conn;
static struct _esp_tcp user_tcp;
static os_timer_t timer_main_conn;
static os_timer_t timer_network_check;
static os_timer_t timer_confirm_hello;

CircularBuffer *rx_stream_buffer = NULL;
CircularBuffer *tx_stream_buffer = NULL;

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
    char Device_mac_buffer[60] = {0};
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

    if (length == os_strlen(device_find_request) &&
            os_strncmp(pusrdata, device_find_request, os_strlen(device_find_request)) == 0)
    {
        os_sprintf(Device_mac_buffer, "Node: %s " MACSTR " " IPSTR, NODE_NAME, MAC2STR(hwaddr), IP2STR(&ipconfig.ip));

        Serial1.printf("%s\n", Device_mac_buffer);
        length = os_strlen(Device_mac_buffer);
        espconn_sent(conn, Device_mac_buffer, length);
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

    switch (main_conn_status)
    {
    case WAIT_HELLO_DONE:
        if ((pstr = strstr(pusrdata, "hello")) != NULL)
        {
            get_hello = true;
        }
        break;
    case KEEP_ALIVE:
        if (!rx_stream_buffer) return;
        //Serial1.printf("recv %d data\n", length);
        if (rx_stream_buffer->capacity() >= length)
        {
            rx_stream_buffer->write(pusrdata, length);
        }
        break;
    default:
        break;
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
    if (main_conn_status != KEEP_ALIVE) return;
    if (main_conn.state > ESPCONN_NONE)
    {
        if (main_conn.state != ESPCONN_WRITE)
        {
            if (len > 10)
            {
                espconn_sent(&main_conn, data, 10);
                noInterrupts();
                tx_stream_buffer->write(data + 10, len - 10);
                interrupts();
            } else
            {
                espconn_sent(&main_conn, data, len);
            }

        } else
        {
            noInterrupts();
            tx_stream_buffer->write(data, len);
            interrupts();
        }
    }
}

static void main_connection_sent_cb(void *arg)
{
    struct espconn *pespconn = arg;
    //Serial1.println("main_connection_sent_cb");
    if (!tx_stream_buffer) return;

    size_t size = tx_stream_buffer->size();
    if (size > 0)
    {
        char *data = (char *)malloc(size);
        tx_stream_buffer->read(data, size);
        espconn_sent(pespconn, data, size);
        free(data);
    }
}

static void confirm_hello(void *arg)
{
    Serial1.printf("confirm hello: %d \n", get_hello);

    if (get_hello || 1)
    {
        struct espconn *pespconn = (struct espconn *)arg;
        espconn_sent(pespconn, ACCESS_TOKEN, strlen(ACCESS_TOKEN));


        main_conn_status = KEEP_ALIVE;

        Serial1.printf("handshake done, keep-alive\n");
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
    espconn_sent(pespconn, "hello\r\n", 7);
    get_hello = false;
    os_timer_disarm(&timer_confirm_hello);
    os_timer_setfn(&timer_confirm_hello, confirm_hello, arg);
    os_timer_arm(&timer_confirm_hello, 1000, 0);
    confirm_hello_retry_cnt = 0;
    main_conn_status = WAIT_HELLO_DONE;
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
        while (1) ESP.wdtFeed();
        break;
    case DIED_IN_CONN:
        Serial1.printf("The main connection died after 5 retry\n");
        Serial1.println("Please check server's ip and port, also check AccessToken\n");
        digitalWrite(STATUS_LED, 0);
        while (1) ESP.wdtFeed();
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
    Serial1.begin(74880);
    Serial1.printf("start to establish network connection.\r\n");

    if (!rx_stream_buffer) rx_stream_buffer = new CircularBuffer(256);
    if (!tx_stream_buffer) tx_stream_buffer = new CircularBuffer(128);


    pinMode(SMARTCONFIG_KEY, INPUT);
    pinMode(STATUS_LED, OUTPUT);

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

    while (main_conn_status < CONNECTED )
    {
        Serial1.printf("Wait connecting to server.\n");
        delay(2000);
    }

    Serial1.printf("Network established.\n\n\n");
}


