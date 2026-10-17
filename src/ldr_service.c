#include "ldr_service.h"
#include "events.h"
#include <zephyr/drivers/adc.h>
#include <zephyr/sys/printk.h>
#include <zephyr/drivers/gpio.h>

#define ADC_NODE DT_ALIAS(analog_in_pin)
static const struct adc_dt_spec ldr_adc = ADC_DT_SPEC_GET(ADC_NODE);

#define LDR_PWR_NODE DT_ALIAS(ldr_pwr)
static const struct gpio_dt_spec ldr_pwr = GPIO_DT_SPEC_GET(LDR_PWR_NODE, gpios);

static int16_t sample;

#define LDR_THREAD_STACK_SIZE 1024
#define LDR_THREAD_PRIORITY 7

K_THREAD_DEFINE(ldr_thread_id, LDR_THREAD_STACK_SIZE,
                ldr_thread_entry, NULL, NULL, NULL, 
                LDR_THREAD_PRIORITY, 0, 0);

void ldr_service_suspend(void) {
    
    if (gpio_is_ready_dt(&ldr_pwr)) {
        gpio_pin_set_dt(&ldr_pwr, 0);
    } 

    k_thread_suspend(ldr_thread_id);
    printk("LDR Thread suspended\n");
}

void ldr_service_resume(void) {
    k_thread_resume(ldr_thread_id);
    printk("LDR Thread resumed\n");
}

void ldr_thread_entry(void *p1, void *p2, void *p3) {
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    if (!adc_is_ready_dt(&ldr_adc)) {
        printk("Error: ADC device is not ready\n");
        return;
    }

    if (adc_channel_setup_dt(&ldr_adc) != 0) {
        printk("Error: ADC channel setup failed\n");
        return;
    }

    if (!gpio_is_ready_dt(&ldr_pwr)) {
        printk("Error: GPIO pin is not ready\n");
        return;
    }

    if (gpio_pin_configure_dt(&ldr_pwr, GPIO_OUTPUT_INACTIVE) != 0) {
        printk("Error: Configuration of the GPIO Supply pin failed\n");
        return;
    }

    while (1) {

        gpio_pin_set_dt(&ldr_pwr, 1);

        k_busy_wait(1000);

        struct adc_sequence sequence = {
            .buffer = &sample,
            .buffer_size = sizeof(sample),
        };
        
        if (adc_sequence_init_dt(&ldr_adc, &sequence) != 0) {
            k_msleep(2000);
            continue;
        }

        if (adc_read_dt(&ldr_adc, &sequence) != 0) {
            k_msleep(2000);
            continue;
        }
        event_t event = {
            .type = EVENT_ADC_READING,
            .adc_raw = sample,
            .brightness_pct = (sample * 100) / 4095,
        };

        events_send(&event);
        gpio_pin_set_dt(&ldr_pwr, 0);

        k_msleep(2000);
    }
}

