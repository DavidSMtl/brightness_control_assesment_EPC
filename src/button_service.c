#include "button_service.h"
#include "events.h"
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>

#define BUTTON_NODE DT_ALIAS(sw0)

static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET(BUTTON_NODE, gpios);
static struct gpio_callback button_cb_data;

// Timer for time measure of 1 second of continuous button press
static bool is_long_press_handled = false;

static void long_press_handler(struct k_timer *timer) {
    ARG_UNUSED(timer);
    is_long_press_handled = true;

    event_t event = {
        .type = EVENT_BUTTON_LONG_PRESS,
        .adc_raw = 0,
        .brightness_pct = 0
    };

    events_send(&event);
}

K_TIMER_DEFINE(long_press_timer, long_press_handler, NULL);

static void button_callback(const struct device *dev, struct gpio_callback *cb, uint32_t pins) {
    int val = gpio_pin_get_dt(&button);

    if ( val > 0) {
        is_long_press_handled = false;
        k_timer_start(&long_press_timer, K_SECONDS(1), K_NO_WAIT);
    }
    else {
       k_timer_stop(&long_press_timer);
       
       if (!is_long_press_handled) {
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
    if (!gpio_is_ready_dt(&button)) {
        printk("Error: button device is not ready\n");
        return -ENODEV;
    }

    gpio_pin_configure_dt(&button, GPIO_INPUT);
    gpio_pin_interrupt_configure_dt(&button, GPIO_INT_EDGE_BOTH);

    gpio_init_callback(&button_cb_data, button_callback, BIT(button.pin));
    gpio_add_callback(button.port, &button_cb_data);

    return 0;
}
