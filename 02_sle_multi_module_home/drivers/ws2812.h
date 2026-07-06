#ifndef WS2812_H
#define WS2812_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    bool on;
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t brightness;
} ws2812_color_t;

int ws2812_init(void);
int ws2812_set_color(uint8_t r, uint8_t g, uint8_t b, uint8_t brightness);
int ws2812_set_color_control(uint8_t r, uint8_t g, uint8_t b, uint8_t brightness);
int ws2812_set_color_profile(uint8_t r, uint8_t g, uint8_t b, uint8_t brightness,
    uint8_t profile);
int ws2812_off(void);
int ws2812_off_control(void);
int ws2812_off_profile(uint8_t profile);
ws2812_color_t ws2812_get_color(void);

#endif
