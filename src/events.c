#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include "events.h"

/* Capacidad máxima de la cola compartida por los productores y la FSM. */
#define EVENT_QUEUE_SIZE 8

/*
 * Cola Zephyr que almacena copias de event_t. El alineamiento de 4 bytes
 * satisface los requisitos de acceso de los elementos de la cola.
 */
K_MSGQ_DEFINE(event_queue, sizeof(event_t), EVENT_QUEUE_SIZE, 4);

void events_init(void) {
    /* Descarta datos antiguos antes del arranque de los servicios. */
    k_msgq_purge(&event_queue);
}

int events_send(const event_t *event) {
    /*
     * Los productores no esperan espacio disponible: esto permite llamar a
     * esta función desde contextos que no deben bloquear, como una ISR.
     */
    if (k_msgq_put(&event_queue, event, K_NO_WAIT) != 0) {
        printk("events: queue full, event %d dropped\n", event->type);
        return -ENOMSG;
    }
    return 0;
}

int events_receive(event_t *event, k_timeout_t timeout) {
    /* La FSM suele esperar indefinidamente hasta que llegue el próximo evento. */
    return k_msgq_get(&event_queue, event, timeout);
}