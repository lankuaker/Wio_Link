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

//#include "grove_spdt_relay_30a.h"
//#include "grove_speaker.h"
//#include "grove_gesture_paj7620.h"
//#include "grove_airquality_tp401a.h"
//#include "grove_multichannel_gas_mics6814.h"


void setup()
{

}

void loop()
{    

}



//test PWM
/*
PWM_T pwm;
uint32_t freq = 10000;

void setup()
{
    Serial1.println("\r\nsuli_pwm_init");
    suli_pwm_init(&pwm, 5);
    Serial1.println("suli_pwm_frequency");
    suli_pwm_frequency(&pwm, freq);//unit: Hz
    Serial1.println("suli_pwm_output");
    suli_pwm_output(&pwm, 0.5);
    Serial1.println("initialized");
}

void loop()
{    
    suli_pwm_frequency(&pwm, freq);//unit: Hz
    freq += 1000;
    if(freq > 50000) freq = 10000;
    delay(250);
    //Serial1.print(".");
}
*/


//GroveMultiChannelGas
#if 0

GroveMultiChannelGas *multichannel_gas;
uint16_t res_ch1, res_ch2, res_ch3;
float nh3, co, no2;//gas concentration

void setup()
{
    Serial1.println("\r\n\r\nGroveMultiChannelGas\r\n");
    multichannel_gas = new GroveMultiChannelGas(4, 5);
    Serial1.println("\r\n\r\ninitialized");
}

void loop()
{  
    multichannel_gas->read_res(&res_ch1, &res_ch2, &res_ch3);
    Serial1.print("Res[0]: ");
    Serial1.println(res_ch1);
    Serial1.print("Res[1]: ");
    Serial1.println(res_ch2);
    Serial1.print("Res[2]: ");
    Serial1.println(res_ch3);
    
    multichannel_gas->read_concentration(&nh3, &co, &no2);
    Serial1.print("NH3: ");
    Serial1.print(nh3);
    Serial1.println("ppm");
    Serial1.print("CO: ");
    Serial1.print(co);
    Serial1.println("ppm");
    Serial1.print("NO2: ");
    Serial1.print(no2);
    Serial1.println("ppm");
    
    delay(1000);
    Serial1.println("...");
}

#endif


//GroveAirquality
#if 0

GroveAirquality *airquality;
int quality = 0;

void setup()
{
    Serial1.println("\r\n\r\nGroveAirquality\r\n");
    airquality = new GroveAirquality(17);
    Serial1.println("\r\n\r\ninitialized");
}

void loop()
{  
    if(airquality->read_quality(&quality) == true)
    {
        Serial1.print("get quality is ");
        Serial1.print(quality);
        if(quality>700)
        {
            Serial1.println("High pollution! Force signal active.");		
        }
    	else if(quality>150)
        {			
            Serial1.println("\t High pollution!");		
        }
    	else if(quality>50)
        {		
            Serial1.println("\t Low pollution!");		
        }
    	else
        {
            Serial1.println("\t Air fresh");
        }
    }
    else
    {
        Serial1.println("the air quality sensor is not ready for reading value");
    }
    
    delay(1000);
}

#endif


//GroveGesture
#if 0

GroveGesture *gesture;
uint8_t motion = 0;

void setup()
{
    Serial1.println("\r\n\r\nGroveGesture\r\n");
    gesture = new GroveGesture(4, 5);
    Serial1.println("\r\n\r\ninitialized");
}

void loop()
{
    gesture->read_motion(&motion);
    if(motion != Gesture_None)
    {
        switch(motion)
        {
            case Gesture_None:
                //nothing to do
                break;
            case Gesture_Right:
                Serial1.println("Right");
                break;
            case Gesture_Left:
                Serial1.println("Left");
                break;
            case Gesture_Up:
                Serial1.println("Up");
                break;
            case Gesture_Down:
                Serial1.println("Down");
                break;
            case Gesture_Foward:
                Serial1.println("Forward");
                break;
            case Gesture_Backward:
                Serial1.println("Backward");
                break;
            case Gesture_Clockwise:
                Serial1.println("Clockwise");
                break;
            case Gesture_CountClockwize:
                Serial1.println("Count Clockwise");
                break;
            case Gesture_Wave:
                Serial1.println("Wave");
                break;
            default:
                //nothing to do
                break;
        }
        motion = 0;//clear flag
    }
    delay(100);
}

#endif

//GroveSpeaker
#if 0

GroveSpeaker *speaker;
const int BassTab[]={1911,1702,1516,1431,1275,1136,1012};//bass 1~7

void setup() 
{
    Serial1.println("\r\n\r\nGroveSpeaker\r\n");
    speaker = new GroveSpeaker(14);
}
void loop()
{
	for(int note_index=0;note_index<7;note_index++)
  	{
    	speaker->write_sound(BassTab[note_index], 500);
		delay(500);
  	}
}

#endif


//GroveSPDTRelay30A
#if 0

GroveSPDTRelay30A *spdtRelay;
void setup()
{
    Serial1.println("\r\n\r\nGroveSPDTRelay30A\r\n");
    spdtRelay = new GroveSPDTRelay30A(14);
}

void loop()
{
    spdtRelay->write_onoff(1);
    delay(500);
    spdtRelay->write_onoff(0);
    delay(500);
}

#endif























#if 0

GroveAccMMA7660 *acc;

void setup()
{
    //Serial1.begin(9600);
    //Serial1.println("Demo\r\n");
    acc = new GroveAccMMA7660(4, 5);
}
void loop()
{
    float ax,ay,az;
    acc->read_accelerometer(&ax,&ay,&az);
    Serial1.println("accleration of X/Y/Z: ");
    Serial1.print(ax);
    Serial1.println(" g");
    Serial1.print(ay);
    Serial1.println(" g");
    Serial1.print(az);
    Serial1.println(" g");

    uint8_t sk;
    acc->read_shacked(&sk);
    Serial1.println(sk, HEX);
    Serial1.println("*************");

    delay(1000);
}


#endif

#if 0
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
        Serial1.println("device_upgrade_success\r\n");
        wifi_station_disconnect();
        system_upgrade_reboot();
    } else
    {
        Serial1.println("device_upgrade_failed\r\n");
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
        Serial1.printf("Running user1.bin \r\n\r\n");
        os_memcpy(user_bin, "user2.bin", 10);
    } else if(system_upgrade_userbin_check() == UPGRADE_FW_BIN2)
    {
        Serial1.printf("Running user2.bin \r\n\r\n");
        os_memcpy(user_bin, "user1.bin", 10);
    }

    os_sprintf(upServer->url,
               "GET /%s HTTP/1.1\r\nHost: " IPSTR ":%d\r\n" pheadbuffer "",
               user_bin, IP2STR(upServer->ip), 80);


    if(system_upgrade_start(upServer) == false)
    {
        Serial1.println("Upgrade already started.");
    } else
    {
        Serial1.println("Upgrade started");
    }
}

static int cnt = 0;

os_timer_t t1;

void print_cnt(void *arg)
{
    int *counter = (int *)arg;
    Serial1.printf("cnt: %d \r\n", *counter);
}
void task1(os_event_t *event)
{
    Serial1.println("task1");
    cnt++;
    os_timer_arm(&t1, 1, 0);
}
void task2(os_event_t *event)
{
    Serial1.print("task2");
    Serial1.println(cnt);
}

struct station_config config;
static os_event_t q1[2];
static os_event_t q2[2];

// the setup function runs once when you press reset or power the board
void setup()
{
    // initialize digital pin 13 as an output.
    pinMode(12, OUTPUT);
    Serial1.begin(115200);
    //suli_i2c_init(&i2c, 0, 0);
#if 0
    wifi_set_opmode_current(0x01);
    memcpy(config.ssid, "Xiaomi_Blindeggb", 17);
    memcpy(config.password, "~375837~",9);
    wifi_station_set_config_current(&config);
#endif
    system_os_task(task1, USER_TASK_PRIO_2, q1, 2);
    os_timer_disarm(&t1);
    os_timer_setfn(&t1, print_cnt, &cnt);

}

// the loop function runs over and over again forever
void loop()
{
    return;
    Serial1.println(analogRead(A0));
    delay(1000);
    return;
    Serial1.println(wifi_station_get_connect_status());
    Serial1.flush();
    digitalWrite(12, HIGH);   // turn the LED on (HIGH is the voltage level)
    delay(1000);              // wait for a second
    digitalWrite(12, LOW);    // turn the LED off by making the voltage LOW
    delay(200);              // wait for a second
    Serial1.println("hello2");
    //suli_i2c_write(&i2c, 0, "adfasdfasd", 5);
    if(Serial1.available()>0)
    {
        char c = Serial1.read();
        Serial1.println(c);
        if(c == 'u')
        {
            at_exeCmdCiupdate(0);
        }
    }
    system_os_post(USER_TASK_PRIO_2, 0, 0);
    uint8_t app = system_upgrade_userbin_check() + 1;
    Serial1.print("user");
    Serial1.println(app);

    os_printf("test print from loop 3333\r\n");
}


#endif
