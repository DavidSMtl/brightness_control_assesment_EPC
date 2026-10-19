#ifndef BUTTON_SERVICE_H
#define BUTTON_SERVICE_H

#include <zephyr/kernel.h>

/*
 * Configures B2 as an input, registers its interrupts, and enables detection
 * of short and long presses. Returns 0 if the device is available.
 */
int button_service_init(void);

#endif