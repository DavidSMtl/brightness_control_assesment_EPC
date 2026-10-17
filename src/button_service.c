#include "button_service.h"
#include "events.h"
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>

#define BUTTON_NODE DT_ALIAS(sw0)

/* Especificación de B2 tomada del alias sw0 definido en el devicetree. */
static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET(BUTTON_NODE, gpios);
static struct gpio_callback button_cb_data;

/*
 * El temporizador marca una pulsación larga al cumplirse un segundo, sin
 * esperar a que se suelte el botón. La bandera impide publicar además una
 * pulsación corta cuando posteriormente se detecta la liberación.
 */
static bool is_long_press_handled = false;

static void long_press_handler(struct k_timer *timer) {
    ARG_UNUSED(timer);
    is_long_press_handled = true;

    /* El temporizador publica el mismo formato de evento usado por la FSM. */
    event_t event = {
        .type = EVENT_BUTTON_LONG_PRESS,
        .adc_raw = 0,
        .brightness_pct = 0
    };

    events_send(&event);
}

K_TIMER_DEFINE(long_press_timer, long_press_handler, NULL);

static void button_callback(const struct device *dev, struct gpio_callback *cb, uint32_t pins) {
    /*
     * gpio_pin_get_dt devuelve el nivel lógico del pin, respetando su
     * configuración de activo bajo en devicetree: positivo equivale a pulsado.
     */
    int val = gpio_pin_get_dt(&button);

    if ( val > 0) {
        /* Cada nuevo flanco de pulsación inicia desde cero el tiempo de 1 s. */
        is_long_press_handled = false;
        k_timer_start(&long_press_timer, K_SECONDS(1), K_NO_WAIT);
    }
    else {
       /* Soltar antes del vencimiento cancela el temporizador. */
       k_timer_stop(&long_press_timer);
       
       if (!is_long_press_handled) {
            /* Solo una liberación anterior al umbral genera pulsación corta. */
            event_t event = {
                .type = EVENT_BUTTON_SHORT_PRESS,
                .adc_raw = 0,
                .brightness_pct = 0
            };
            events_send(&event);
       }
    }
}

int button_service_init(void) {
    /* Comprueba el dispositivo antes de configurar el GPIO del botón. */
    if (!gpio_is_ready_dt(&button)) {
        printk("Error: button device is not ready\n");
        return -ENODEV;
    }

    /* Las interrupciones en ambos flancos permiten detectar pulsación y suelta. */
    gpio_pin_configure_dt(&button, GPIO_INPUT);
    gpio_pin_interrupt_configure_dt(&button, GPIO_INT_EDGE_BOTH);

    /* Registra el callback para el pin asociado al botón. */
    gpio_init_callback(&button_cb_data, button_callback, BIT(button.pin));
    gpio_add_callback(button.port, &button_cb_data);

    return 0;
}
