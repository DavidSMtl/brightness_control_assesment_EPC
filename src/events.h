#ifndef EVENTS_H
#define EVENTS_H

#include <stdint.h>
#include <zephyr/kernel.h>

/* Configured ADC resolution: 12 bits, with values from 0 to 4095. */
#define ADC_FULL_SCALE 4095U

/*
 * Types of messages that services can publish. EVENT_NONE serves as the
 * initial value; the remaining three events represent button input or a
 * measurement already obtained by the LDR service.
 */
typedef enum {
    EVENT_NONE = 0,
    EVENT_BUTTON_SHORT_PRESS,
    EVENT_BUTTON_LONG_PRESS,
    EVENT_ADC_READING,
} event_type_t;

/*
 * Common format carried through the queue. Measurement fields are meaningful
 * only when type is EVENT_ADC_READING; button events initialize them to zero
 * so the structure is always fully defined.
 */
typedef struct {
    event_type_t type;
    uint16_t adc_raw;        /* Valid only for EVENT_ADC_READING. */
    uint16_t brightness_pct; /* Percentage from 0 to 100; valid only for EVENT_ADC_READING. */
} event_t;

/* Initializes the shared queue; called once before starting the services. */
void events_init(void);

/*
 * Copies an event to the queue without blocking. Returns 0 if it was queued
 * and -ENOMSG if there was no space; in that case events.c also logs the drop.
 */
int events_send(const event_t *event);

/*
 * Retrieves the next event subject to timeout. The Zephyr result is returned
 * directly: 0 indicates success, and a negative code indicates failure or
 * timeout expiration.
 */
int events_receive(event_t *event, k_timeout_t timeout);

#endif // EVENTS_H