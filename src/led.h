#ifndef LED_H
#define LED_H

#include <zephyr/kernel.h>

typedef enum {
    LED_COLOR_OFF = 0,
    LED_COLOR_RED,
    LED_COLOR_GREEN,
    LED_COLOR_BLUE,
    LED_COLOR_YELLOW,
} led_color_t;

int led_init(void);
int led_set_color(led_color_t color);

#endif // LED_H