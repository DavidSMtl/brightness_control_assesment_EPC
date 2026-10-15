#ifndef EVENTS_H
#define EVENTS_H

#include <stdint.h>
#include <zephyr/kernel.h>

typedef enum { // Event types
    EVENT_NONE = 0,
    EVENT_BUTTON_SHORT_PRESS,
    EVENT_BUTTON_LONG_PRESS,
    EVENT_ADC_READING,
} event_type_t;

typedef struct {
    event_type_t type;
    uint16_t adc_raw;        /* Only valid for EVENT_ADC_READING */
    uint16_t brightness_pct; /* Percentage 0-100, only valid for EVENT_ADC_READING */
} event_t;

/* Initialise the shared event queue. Call once before starting threads. */
void events_init(void);

/* Send an event without blocking. Returns 0 on success, -ENOMSG if the queue is full. */
int events_send(const event_t *event);

/* Wait for an event. Returns 0 on success, -EAGAIN on timeout. */
int events_receive(event_t *event, k_timeout_t timeout);

#endif // EVENTS_H