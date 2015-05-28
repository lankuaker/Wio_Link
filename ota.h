


#ifndef __OTA_H__
#define __OTA_H__

#include "esp8266.h"

extern bool ota_fini;
extern bool ota_succ;
extern os_timer_t timer_reboot;

void ota_start();

#endif
