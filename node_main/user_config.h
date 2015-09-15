#ifndef __USER_CONFIG_H__
#define __USER_CONFIG_H__

#define FUNCTION_KEY                0
#define STATUS_LED                  15
#define STATUS_LED_TXD1             2

#ifndef NODE_NAME
#define NODE_NAME                   "Pion One 001"
#endif

#ifndef SERVER_IP
#define SERVER_IP                   {45,79,111,86}
#endif

#define SERVER_PORT                 8000
#define OTA_SERVER_PORT             80
#ifndef OTA_SERVER_URL_PREFIX
#define OTA_SERVER_URL_PREFIX       "/v1"
#endif

#define ENABLE_DEBUG_ON_UART1       1

/* eeprom slots */
#define EEP_OFFSET_KEY              0
#define EEP_OFFSET_SN               100
#define EEP_OFFSET_CFG_FLAG         200
#define EEP_OFFSET_SSID             204
#define EEP_OFFSET_PASSWORD         300


#endif
