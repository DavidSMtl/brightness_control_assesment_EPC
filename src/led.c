#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include "led.h"

#define RED_LED_NODE DT_ALIAS(led_red)
#define GREEN_LED_NODE DT_ALIAS(led_green)
#define BLUE_LED_NODE DT_ALIAS(led_blue)

/* RGB GPIOs resolved from the board overlay aliases. */
static const struct gpio_dt_spec red_led = GPIO_DT_SPEC_GET(RED_LED_NODE, gpios);
static const struct gpio_dt_spec green_led = GPIO_DT_SPEC_GET(GREEN_LED_NODE, gpios);
static const struct gpio_dt_spec blue_led = GPIO_DT_SPEC_GET(BLUE_LED_NODE, gpios);

int led_init(void) {

    /* All three channels must be available to use the RGB LED. */
    if (!gpio_is_ready_dt(&red_led) ||
        !gpio_is_ready_dt(&green_led) ||
        !gpio_is_ready_dt(&blue_led)) {
            
        printk("LED GPIO device not ready\n");
        return -ENODEV;

    }

    /*
     * Configure each channel as an inactive output. With GPIO_ACTIVE_LOW for
     * the common-anode LED, inactive corresponds to turning the channel off.
     */
    if (gpio_pin_configure_dt(&red_led, GPIO_OUTPUT_INACTIVE) != 0 ||
        gpio_pin_configure_dt(&green_led, GPIO_OUTPUT_INACTIVE) != 0 ||
        gpio_pin_configure_dt(&blue_led, GPIO_OUTPUT_INACTIVE) != 0) {
            
        printk("Failed to configure LED GPIO pins\n");
        return -EIO;

    }

    return 0;
}

int led_set_color(led_color_t color) {
    /*
     * Logical value per channel (1 = on). The devicetree GPIO mapping applies
     * the physical polarity of the common-anode LED.
     */
    int red = 0;
    int green = 0;
    int blue = 0;

    /* Yellow is produced by turning on red and green simultaneously. */
    switch (color) {
        case LED_COLOR_OFF:
            red = 0;
            green = 0;
            blue = 0;
            break;
        case LED_COLOR_RED:
            red = 1;
            green = 0;
            blue = 0;
            break;
        case LED_COLOR_GREEN:
            red = 0;
            green = 1;
            blue = 0;
            break;
        case LED_COLOR_BLUE:
            red = 0;
            green = 0;
            blue = 1;
            break;
        case LED_COLOR_YELLOW:
            red = 1;
            green = 1;
            blue = 0;
            break;
        default:
            printk("Invalid LED color\n");
            return -EINVAL;
    }

    /* Update all three channels to set a complete, well-defined color. */
    if (gpio_pin_set_dt(&red_led, red) != 0 ||
        gpio_pin_set_dt(&green_led, green) != 0 ||
        gpio_pin_set_dt(&blue_led, blue) != 0) {
            
        printk("Failed to set LED color\n");
        return -EIO;

    }

    return 0;
}