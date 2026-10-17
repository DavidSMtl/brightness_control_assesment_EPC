#ifndef LDR_SERVICE_H
#define LDR_SERVICE_H

#include <zephyr/kernel.h>

// Thread declaration for LDR service
void ldr_thread_entry(void *p1, void *p2, void *p3);
void ldr_service_suspend(void);
void ldr_service_resume(void);

#endif