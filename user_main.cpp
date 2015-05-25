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
}

extern void (*__init_array_start)(void);
extern void (*__init_array_end)(void);
extern "C" void system_phy_set_rfoption(uint8 option);
extern "C" bool system_os_post(uint8 prio, os_signal_t sig, os_param_t par);

extern void arduino_init(void);
extern "C" void esp_schedule();

void do_global_ctors(void)
{
    void(**p)(void);
    for(p = &__init_array_start; p != &__init_array_end; ++p) (*p)();
}

void init_done()
{
    do_global_ctors();
    esp_schedule();

}

extern "C"
void user_rf_pre_init()
{
    system_phy_set_rfoption(1);
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
 * this function will be linked by core_esp8266_main.cpp - loop_wrapper
 *
 * @author Jack (5/23/2015)
 * @param
 */
void pre_user_loop()
{

}


