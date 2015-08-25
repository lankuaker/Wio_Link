/*
 * grove_led_ws2812.h
 *
 * Copyright (c) 2015 seeed technology inc.
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
 *
 *
 */

/*
 * +++++++++++++++++++++++++++++++++
 * NOTE:
 * This library is only for esp8266
 * +++++++++++++++++++++++++++++++++
 */


#ifndef __GROVE_LED_WS2812_H__
#define __GROVE_LED_WS2812_H__

#include "suli2.h"

//GROVE_NAME        "Grove-WS2812 LED Strip 60"
//IF_TYPE           GPIO
//IMAGE_URL         http://www.seeedstudio.com/depot/bmz_cache/3/3780d3a3cc3e57600702c7999a1550d3.image.530x397.jpg

#define MAX_LED_CNT             60

struct rgb_s
{
    uint8_t g,r,b;
}__attribute__((__packed__));

union rgb_buffer_u
{
    uint8_t buff[MAX_LED_CNT*3];
    struct rgb_s pixels[MAX_LED_CNT];
};

class GroveLedWs2812
{
public:
    GroveLedWs2812(int pin);
    char *get_last_error() { return error_desc; };
    bool write_clear(uint8_t total_led_cnt, char *rgb_hex_string);
    bool write_segment(uint8_t start, char *rgb_hex_string);

private:
    IO_T *io;
    union rgb_buffer_u rgb_buffer;
    bool _extract_rgb_from_string(int start, char *str);
    char *error_desc;
    uint8_t led_cnt;
};

#endif

