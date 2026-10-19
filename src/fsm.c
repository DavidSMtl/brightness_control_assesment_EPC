#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include "events.h"
#include "ldr_service.h"
#include "led.h"
#include "fsm.h"

#define FSM_STACK_SIZE 1024
#define FSM_PRIORITY 5

/* State read and modified exclusively by the FSM thread. */
static fsm_mode_t mode = FSM_MODE_NORMAL;
/* Last valid sample; used to restore the color when returning to NORMAL. */
static uint16_t last_adc_raw = 0;

fsm_mode_t fsm_get_mode(void) {
    return mode;
}

static led_color_t color_from_raw(uint16_t raw) {
    /*
     * Compare the reading without first converting it to an integer percentage,
     * so the 33% and 66% thresholds do not depend on division truncation.
     */
    uint32_t scaled = (uint32_t)raw * 100U;

    if (scaled < 33U * ADC_FULL_SCALE) {
        return LED_COLOR_RED;
    }
    if (scaled <= 66U * ADC_FULL_SCALE) {
        return LED_COLOR_YELLOW;
    }
    return LED_COLOR_GREEN;
}

static void enter_mode (fsm_mode_t new_mode) {
    /* Update the state before applying the actions associated with the mode. */
    mode = new_mode;

    switch (mode) {
        case FSM_MODE_NORMAL:
            printk("Mode: NORMAL\n");
            /* In NORMAL, resume measurements and display the last color. */
            ldr_service_resume();
            led_set_color(color_from_raw(last_adc_raw));
            break;

        case FSM_MODE_BLUE:
            printk("Mode: BLUE\n");
            /* The fixed blue color distinguishes this mode; measurements are unnecessary. */
            ldr_service_suspend();
            led_set_color(LED_COLOR_BLUE);
            break;

        case FSM_MODE_OFF:
            printk("System OFF\n");
            /* OFF turns off the output and stops periodic LDR sampling. */
            ldr_service_suspend();
            led_set_color(LED_COLOR_OFF);
            break;
    }
}

static void handle_event (const event_t *event) {
    switch (event->type) {
        case EVENT_BUTTON_SHORT_PRESS:
            /*
             * A short press toggles between NORMAL and BLUE. From OFF, it
             * explicitly returns to NORMAL, as defined by the behavior.
             */
            if (mode == FSM_MODE_NORMAL) {
                enter_mode(FSM_MODE_BLUE);
            } else {
                enter_mode(FSM_MODE_NORMAL);
            }
            break;

        case EVENT_BUTTON_LONG_PRESS:
            /* A long press enters OFF; in OFF, it causes no further transition. */
            if (mode != FSM_MODE_OFF) {
                enter_mode(FSM_MODE_OFF);
            }
            break;

        case EVENT_ADC_READING:
            /*
             * Samples are accepted for the interface only in NORMAL.
             * In other modes, the FSM ignores them and leaves the stored color unchanged.
             */
            if (mode == FSM_MODE_NORMAL) {
                last_adc_raw = event->adc_raw;
                printk("ADC raw: %u, brightness: %u%%\n",
                       event->adc_raw, event->brightness_pct);
                led_set_color(color_from_raw(event->adc_raw));
            }
            break;

        default:
            break;
    }
}

static void fsm_thread(void *p1, void *p2, void *p3) {
    event_t event;

    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    /* The FSM initializes the LED hardware before announcing its first state. */
    if (led_init() != 0) {
        printk("Error: LED init failed\n");
        return;
    }
    enter_mode(FSM_MODE_NORMAL);

    /*
     * The queue decouples this logic from the producer services. K_FOREVER
     * avoids polling: the thread blocks until it receives a message.
     */
    while (1) {
        if (events_receive(&event, K_FOREVER) == 0) {
            handle_event(&event);
        }
    }
}

/* State machine thread: 1024-byte stack and priority 5 in Zephyr. */
K_THREAD_DEFINE(fsm_tid, FSM_STACK_SIZE, fsm_thread, NULL, NULL, NULL, FSM_PRIORITY, 0, 0);