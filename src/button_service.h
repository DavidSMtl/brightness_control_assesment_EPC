#ifndef BUTTON_SERVICE_H
#define BUTTON_SERVICE_H

#include <zephyr/kernel.h>

int button_service_init(void); // Initialize GPIO B2 and configure interrupts.

#endif 