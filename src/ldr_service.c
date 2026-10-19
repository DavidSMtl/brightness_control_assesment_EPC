#include "ldr_service.h"
#include "events.h"
#include <zephyr/drivers/adc.h>
#include <zephyr/sys/printk.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/atomic.h>

#ifdef CONFIG_PM
#include <zephyr/pm/policy.h>
/*
 * Suspend states are locked during conversion because the ADC does not
 * complete the reading if the system enters STOP. Without CONFIG_PM, these
 * operations have no effect.
 */
#define LDR_PM_LOCK() pm_policy_state_lock_get(PM_STATE_SUSPEND_TO_IDLE, PM_ALL_SUBSTATES)
#define LDR_PM_UNLOCK() pm_policy_state_lock_put(PM_STATE_SUSPEND_TO_IDLE, PM_ALL_SUBSTATES)
#else
#define LDR_PM_LOCK() do { } while (0)
#define LDR_PM_UNLOCK() do { } while (0)
#endif

/* ADC channel described by the project's analog-in-pin devicetree alias. */
#define ADC_NODE DT_ALIAS(analog_in_pin)
static const struct adc_dt_spec ldr_adc = ADC_DT_SPEC_GET(ADC_NODE);

/* GPIO that powers the LDR divider only while each sample is being taken. */
#define LDR_PWR_NODE DT_ALIAS(ldr_pwr)
static const struct gpio_dt_spec ldr_pwr = GPIO_DT_SPEC_GET(LDR_PWR_NODE, gpios);

/* The ADC writes the 16-bit conversion value here. */
static int16_t sample;

/* Resources and timing settings for the periodic sensor thread. */
#define LDR_THREAD_STACK_SIZE 1024
#define LDR_THREAD_PRIORITY 7
#define LDR_PERIOD_MS 2000
#define LDR_SETTLE_MS 1

/*
 * The flag indicates whether new acquisitions are allowed. The semaphore
 * wakes the thread when it is blocked by a suspension requested by the FSM.
 */
static atomic_t acquisition_enabled = ATOMIC_INIT(1);
static K_SEM_DEFINE(resume_sem, 0, 1);

/* The sampling thread starts automatically as part of Zephyr's startup. */
K_THREAD_DEFINE(ldr_thread_id, LDR_THREAD_STACK_SIZE,
                ldr_thread_entry, NULL, NULL, NULL, 
                LDR_THREAD_PRIORITY, 0, 0);

void ldr_service_suspend(void) {
    /* The thread will check the flag at the next safe point in its loop. */
    atomic_set(&acquisition_enabled, 0);
    printk("LDR acquisition suspended\n");
}

void ldr_service_resume(void) {
    /* Re-enables readings and wakes the thread if it was waiting on the semaphore. */
    atomic_set(&acquisition_enabled, 1);
    k_sem_give(&resume_sem);
    printk("LDR acquisition resumed\n");
}

static int ldr_acquire(event_t *event, struct adc_sequence *seq) {
    /* Prevents suspension while the peripheral performs the conversion. */
    LDR_PM_LOCK();

    /* Powers the divider and waits briefly for it to settle before measuring. */
    gpio_pin_set_dt(&ldr_pwr, 1);
    k_msleep(LDR_SETTLE_MS);

    /* The reading is placed in sample through the adc_sequence buffer. */
    int ret = adc_read_dt(&ldr_adc, seq);

    /* Requests power-off even if adc_read_dt returned an error. */
    gpio_pin_set_dt(&ldr_pwr, 0);           /* Also runs if the conversion fails. */
    LDR_PM_UNLOCK();

    if (ret != 0) {
        return ret;
    }

    /*
     * Prevents a negative buffer value from propagating as an unsigned reading
     * and calculates the integer percentage over the ADC's full range (0..4095).
     */
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

    /*
     * The sequence specifies the destination buffer; adc_sequence_init_dt
     * fills in the remaining parameters from the devicetree.
     */
    struct adc_sequence sequence = {
        .buffer = &sample,
        .buffer_size = sizeof(sample),
    };

    /* Initial checks: if any fail, the thread reports the error and does not measure. */
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

    /* The divider power supply starts off to save energy. */
    if (gpio_pin_configure_dt(&ldr_pwr, GPIO_OUTPUT_INACTIVE) != 0) {
        printk("Error: Configuration of the GPIO Supply pin failed\n");
        return;
    }

    /*
     * An absolute target time is used to prevent the time spent on each reading
     * from accumulating in the period and causing the 2-second interval to drift.
     */
    int64_t next_wakeup = k_uptime_get();

    while (1) {
        if (!atomic_get(&acquisition_enabled)) {
            /* Waits without consuming CPU until the FSM reactivates the service. */
            while (!atomic_get(&acquisition_enabled)) {
                k_sem_take(&resume_sem, K_FOREVER);
            }
            /* On resume, the next period wait starts from the current time. */
            next_wakeup = k_uptime_get();
        }

        event_t event;
        /* Publishes the sample; reading errors are reported and not queued. */
        if (ldr_acquire(&event, &sequence) == 0) {
            events_send(&event);
        } else {
            printk("Error:ADC read failed\n");
        }

        next_wakeup += LDR_PERIOD_MS;
        int64_t remaining = next_wakeup - k_uptime_get();
        if (remaining > 0) {
            /* Sleeps only for the time remaining until the next scheduled instant. */
            k_msleep(remaining);
        } else {
            /* If the operation exceeded the period, starts a new interval from now. */
            next_wakeup = k_uptime_get();
        }
    }
}
