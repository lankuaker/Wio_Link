
/* For 4MByte Flash */
/* spiffs 2028KB */
/* eeprom 4K + system para 16K = 20KB */
PROVIDE ( _SPIFFS_start = 0x40400000 );
PROVIDE ( _SPIFFS_end = 0x405FB000 );
