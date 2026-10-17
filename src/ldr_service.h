#ifndef LDR_SERVICE_H
#define LDR_SERVICE_H

#include <zephyr/kernel.h>

/* Punto de entrada usado por la definición del hilo de adquisición del LDR. */
void ldr_thread_entry(void *p1, void *p2, void *p3);

/* Solicita detener las medidas periódicas en un punto seguro del bucle. */
void ldr_service_suspend(void);

/* Habilita las medidas y despierta al hilo si estaba esperando suspensión. */
void ldr_service_resume(void);

#endif