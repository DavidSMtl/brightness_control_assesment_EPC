#include "ldr_service.h"
#include "events.h"
#include <zephyr/drivers/adc.h>
#include <zephyr/sys/printk.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/atomic.h>

#ifdef CONFIG_PM
#include <zephyr/pm/policy.h>
/* A conversion cannot complete while the system is in a STOP state */
#define LDR_PM_LOCK() pm_policy_state_lock_get(PM_STATE_SUSPEND_TO_IDLE, PM_ALL_SUBSTATES)
#define LDR_PM_UNLOCK() pm_policy_state_lock_put(PM_STATE_SUSPEND_TO_IDLE, PM_ALL_SUBSTATES)
#else
#define LDR_PM_LOCK() do { } while (0)
#define LDR_PM_UNLOCK() do { } while (0)
#endif

#define ADC_NODE DT_ALIAS(analog_in_pin)
static const struct adc_dt_spec ldr_adc = ADC_DT_SPEC_GET(ADC_NODE);

#define LDR_PWR_NODE DT_ALIAS(ldr_pwr)
static const struct gpio_dt_spec ldr_pwr = GPIO_DT_SPEC_GET(LDR_PWR_NODE, gpios);

static int16_t sample;

#define LDR_THREAD_STACK_SIZE 1024
#define LDR_THREAD_PRIORITY 7
#define LDR_PERIOD_MS 2000
#define LDR_SETTLE_MS 1

static atomic_t acquisition_enabled = ATOMIC_INIT(1);
static K_SEM_DEFINE(resume_sem, 0, 1);

K_THREAD_DEFINE(ldr_thread_id, LDR_THREAD_STACK_SIZE,
                ldr_thread_entry, NULL, NULL, NULL, 
                LDR_THREAD_PRIORITY, 0, 0);

void ldr_service_suspend(void) {
    
    atomic_set(&acquisition_enabled, 0);
    printk("LDR acquisition suspended\n");
}

void ldr_service_resume(void) {
    atomic_set(&acquisition_enabled, 1);
    k_sem_give(&resume_sem);
    printk("LDR acquisition resumed\n");
}

static int ldr_acquire(event_t *event, struct adc_sequence *seq) {
    LDR_PM_LOCK();

    //printk("LDR: power on\n");
    gpio_pin_set_dt(&ldr_pwr, 1);
    k_msleep(LDR_SETTLE_MS);
    //printk("LDR: reading ADC\n");
    int ret = adc_read_dt(&ldr_adc, seq);
    //printk("LDR: ADC done, ret=%d\n", ret);
    gpio_pin_set_dt(&ldr_pwr, 0);           /* Always off, even on error */

    LDR_PM_UNLOCK();

    if (ret != 0) {
        return ret;
    }

    int32_t value = sample < 0 ? 0 : sample;

    event->type = EVENT_ADC_READING;
    event->adc_raw = (uint16_t)value;
    event->brightness_pct = (uint8_t)MIN((value * 100) / ADC_FULL_SCALE, 100);
    return 0;
}

void ldr_thread_entry(void *p1, void *p2, void *p3) {
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    struct adc_sequence sequence = {
        .buffer = &sample,
        .buffer_size = sizeof(sample),
    };

    if (!adc_is_ready_dt(&ldr_adc)) {
        printk("Error: LDR devices is not ready\n");
        return;
    }

    if (adc_channel_setup_dt(&ldr_adc) != 0 ||
        adc_sequence_init_dt(&ldr_adc, &sequence) != 0) {
        printk("Error: LDR setup failed\n");
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

    int64_t next_wakeup = k_uptime_get();

    while (1) {
        if (!atomic_get(&acquisition_enabled)) {
            while (!atomic_get(&acquisition_enabled)) {
                k_sem_take(&resume_sem, K_FOREVER);
            }
            next_wakeup = k_uptime_get();
        }

        event_t event;
        if (ldr_acquire(&event, &sequence) == 0) {
            events_send(&event);
        } else {
            printk("Error:ADC read failed\n");
        }

        next_wakeup += LDR_PERIOD_MS;
        int64_t remaining = next_wakeup - k_uptime_get();
        if (remaining > 0) {
            k_msleep(remaining);
        } else {
            next_wakeup = k_uptime_get();
        }
    }
}

