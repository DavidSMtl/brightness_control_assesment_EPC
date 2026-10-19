#include "button_service.h"
#include "events.h"
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>

#define BUTTON_NODE DT_ALIAS(sw1)

/* B2 specification obtained from the sw1 alias defined in the devicetree. */
static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET(BUTTON_NODE, gpios);
static struct gpio_callback button_cb_data;

/*
 * The timer marks a long press after one second, without waiting for the
 * button to be released. The flag prevents also publishing a short press
 * when the release is subsequently detected.
 */
static bool is_long_press_handled = false;

static void long_press_handler(struct k_timer *timer) {
    ARG_UNUSED(timer);
    is_long_press_handled = true;

    /* The timer publishes the same event format used by the FSM. */
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
     * gpio_pin_get_dt returns the logical pin level, respecting its
     * active-low configuration in the devicetree: positive means pressed.
     */
    int val = gpio_pin_get_dt(&button);

    if ( val > 0) {
        /* Each new press edge restarts the 1-second timer. */
        is_long_press_handled = false;
        k_timer_start(&long_press_timer, K_SECONDS(1), K_NO_WAIT);
    }
    else {
       /* Releasing before the timer expires cancels it. */
       k_timer_stop(&long_press_timer);
       
       if (!is_long_press_handled) {
            /* Only a release before the threshold generates a short press. */
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
    /* Checks the device before configuring the button GPIO. */
    if (!gpio_is_ready_dt(&button)) {
        printk("Error: button device is not ready\n");
        return -ENODEV;
    }

    /* Interrupts on both edges allow detection of press and release. */
    gpio_pin_configure_dt(&button, GPIO_INPUT);
    gpio_pin_interrupt_configure_dt(&button, GPIO_INT_EDGE_BOTH);

    /* Registers the callback for the pin associated with the button. */
    gpio_init_callback(&button_cb_data, button_callback, BIT(button.pin));
    gpio_add_callback(button.port, &button_cb_data);

    return 0;
}
