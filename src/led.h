#ifndef LED_H
#define LED_H

#include <zephyr/kernel.h>

/* Colores que el controlador RGB puede representar. */
typedef enum {
    LED_COLOR_OFF = 0,
    LED_COLOR_RED,
    LED_COLOR_GREEN,
    LED_COLOR_BLUE,
    LED_COLOR_YELLOW,
} led_color_t;

/* Comprueba los GPIO RGB y los configura inicialmente como salidas apagadas. */
int led_init(void);

/* Aplica el color indicado; devuelve -EINVAL para un valor no reconocido. */
int led_set_color(led_color_t color);

#endif // LED_H