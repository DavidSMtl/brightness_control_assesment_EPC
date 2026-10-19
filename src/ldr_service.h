#ifndef LDR_SERVICE_H
#define LDR_SERVICE_H

#include <zephyr/kernel.h>

/* Entry point used by the LDR acquisition thread definition. */
void ldr_thread_entry(void *p1, void *p2, void *p3);

/* Requests that periodic measurements stop at a safe point in the loop. */
void ldr_service_suspend(void);

/* Enables measurements and wakes the thread if it was waiting while suspended. */
void ldr_service_resume(void);

#endif