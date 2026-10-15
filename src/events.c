#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include "events.h"

#define EVENT_QUEUE_SIZE 8

K_MSGQ_DEFINE(event_queue, sizeof(event_t), EVENT_QUEUE_SIZE, 4);

void events_init(void) {
    k_msgq_purge(&event_queue);
}

int events_send(const event_t *event) {
    if (k_msgq_put(&event_queue, event, K_NO_WAIT) != 0) {
        printk("events: queue full, event %d dropped\n", event->type);
        return -ENOMSG;
    }
    return 0;
}

int events_receive(event_t *event, k_timeout_t timeout) {
    return k_msgq_get(&event_queue, event, timeout);
}