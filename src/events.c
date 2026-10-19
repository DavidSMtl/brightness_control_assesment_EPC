#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include "events.h"

/* Maximum capacity of the queue shared by the producers and the FSM. */
#define EVENT_QUEUE_SIZE 8

/*
 * Zephyr queue that stores copies of event_t. The 4-byte alignment satisfies
 * the access requirements of the queue elements.
 */
K_MSGQ_DEFINE(event_queue, sizeof(event_t), EVENT_QUEUE_SIZE, 4);

void events_init(void) {
    /* Discards old data before the services start. */
    k_msgq_purge(&event_queue);
}

int events_send(const event_t *event) {
    /*
     * Producers do not wait for available space: this allows this function to
     * be called from contexts that must not block, such as an ISR.
     */
    if (k_msgq_put(&event_queue, event, K_NO_WAIT) != 0) {
        printk("events: queue full, event %d dropped\n", event->type);
        return -ENOMSG;
    }
    return 0;
}

int events_receive(event_t *event, k_timeout_t timeout) {
    /* The FSM usually waits indefinitely until the next event arrives. */
    return k_msgq_get(&event_queue, event, timeout);
}