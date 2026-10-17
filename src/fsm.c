#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include "events.h"
#include "ldr_service.h"
#include "led.h"
#include "fsm.h"

#define FSM_STACK_SIZE 1024
#define FSM_PRIORITY 5

static fsm_mode_t mode = FSM_MODE_NORMAL;
static uint16_t last_adc_raw = 0;

fsm_mode_t fsm_get_mode(void) {
    return mode;
}

static led_color_t color_from_raw(uint16_t raw) {
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
    mode = new_mode;

    switch (mode) {
        case FSM_MODE_NORMAL:
            printk("Mode: NORMAL\n");
            ldr_service_resume();
            led_set_color(color_from_raw(last_adc_raw));
            break;

        case FSM_MODE_BLUE:
            printk("Mode: BLUE\n");
            ldr_service_suspend();
            led_set_color(LED_COLOR_BLUE);
            break;

        case FSM_MODE_OFF:
            printk("System OFF\n");
            ldr_service_suspend();
            led_set_color(LED_COLOR_OFF);
            break;
    }
}

static void handle_event (const event_t *event) {
    switch (event->type) {
        case EVENT_BUTTON_SHORT_PRESS:
            if (mode == FSM_MODE_NORMAL) {
                enter_mode(FSM_MODE_BLUE);
            } else {
                enter_mode(FSM_MODE_NORMAL);
            }
            break;

        case EVENT_BUTTON_LONG_PRESS:
            if (mode != FSM_MODE_OFF) {
                enter_mode(FSM_MODE_OFF);
            }
            break;

        case EVENT_ADC_READING:
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

    if (led_init() != 0) {
        printk("Error: LED init failed\n");
        return;
    }
    enter_mode(FSM_MODE_NORMAL);

    while (1) {
        if (events_receive(&event, K_FOREVER) == 0) {
            handle_event(&event);
        }
    }
}

K_THREAD_DEFINE(fsm_tid, FSM_STACK_SIZE, fsm_thread, NULL, NULL, NULL, FSM_PRIORITY, 0, 0);