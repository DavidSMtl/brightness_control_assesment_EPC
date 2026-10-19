#ifndef LED_H
#define LED_H

#include <zephyr/kernel.h>

/* Colors that the RGB controller can represent. */
typedef enum {
    LED_COLOR_OFF = 0,
    LED_COLOR_RED,
    LED_COLOR_GREEN,
    LED_COLOR_BLUE,
    LED_COLOR_YELLOW,
} led_color_t;

/* Checks the RGB GPIOs and initially configures them as off outputs. */
int led_init(void);

/* Applies the specified color; returns -EINVAL for an unrecognized value. */
int led_set_color(led_color_t color);

#endif // LED_H