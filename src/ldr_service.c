#include "ldr_service.h"
#include "events.h"
#include <zephyr/drivers/adc.h>
#include <zephyr/sys/printk.h>

#define ADC_NODE DT_ALIAS(analog_in_pin)

static const struct adc_dt_spec ldr_adc = ADC_DT_SPEC_GET(ADC_NODE);
static int16_t sample;

#define LDR_THREAD_STACK_SIZE 1024
#define LDR_THREAD_PRIORITY 7

K_THREAD_DEFINE(ldr_thread_id, LDR_THREAD_STACK_SIZE,
                ldr_thread_entry, NULL, NULL, NULL, 
                LDR_THREAD_PRIORITY, 0, 0);

void ldr_service_suspend(void) {
    
    #if DT_NODE_EXISTS(DT_ALIAS(ldr_pwr)) 
    gpio_pin_set_dt(&ldr_pwr, 0);
    #endif

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

    while (1) {
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
        k_msleep(2000);
    }
}

