#ifndef BUTTON_SERVICE_H
#define BUTTON_SERVICE_H

#include <zephyr/kernel.h>

/*
 * Prepara B2 como entrada, registra sus interrupciones y habilita la detección
 * de pulsaciones cortas y largas. Devuelve 0 si el dispositivo está disponible.
 */
int button_service_init(void);

#endif