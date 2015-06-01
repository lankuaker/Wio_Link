/******************************************************************************
 * Copyright 2013-2014 Espressif Systems (Wuxi)
 *
 * FileName: user_main.c
 *
 * Description: entry file of user application
 *
 * Modification history:
 *     2015/1/23, v1.0 create this file.
*******************************************************************************/
extern "C"
{
#include "esp8266.h"

void system_phy_set_rfoption(uint8 option);
bool system_os_post(uint8 prio, os_signal_t sig, os_param_t par);
void esp_schedule();
}

#include "Arduino.h"
#include "network.h"
#include "rpc_server.h"

extern void arduino_init(void);
extern void do_global_ctors(void);

/**
 * SDK routine - callback when user_init done.
 *
 * @param
 */
void init_done()
{
    do_global_ctors();

    /* let arduino loop loop*/
    esp_schedule();
}

/**
 * This function is needed by new SDK 1.1.0 when linking stage
 *
 * @param
 */
extern "C"
void user_rf_pre_init()
{
    system_phy_set_rfoption(1);  //recalibrate the rf when power up
}


/**
 * Global function needed by libmain.a when linking
 *
 * @author Jack (5/23/2015)
 * @param
 */
extern "C"
void user_init(void)
{
    uart_div_modify(0, UART_CLK_FREQ / (115200));

    arduino_init();

    system_init_done_cb(&init_done);
}


/**
 * this function will be linked by core_esp8266_main.cpp -
 * loop_wrapper
 *
 * @param
 */
void pre_user_setup()
{
    establish_network();
    rpc_server_init();
}

/**
 * this function will be linked by core_esp8266_main.cpp - loop_wrapper
 *
 * @author Jack (5/23/2015)
 * @param
 */
void pre_user_loop()
{
    if(main_conn_status == KEEP_ALIVE)
    {
        rpc_server_loop();
    }
}


