#ifndef EVENTS_H
#define EVENTS_H

#include <stdint.h>
#include <zephyr/kernel.h>

/* Resolución del ADC configurado: 12 bits, con valores de 0 a 4095. */
#define ADC_FULL_SCALE 4095U

/*
 * Tipos de mensajes que pueden publicar los servicios. EVENT_NONE sirve como
 * valor inicial; los tres eventos restantes representan entradas del botón o
 * una medida ya obtenida por el servicio LDR.
 */
typedef enum {
    EVENT_NONE = 0,
    EVENT_BUTTON_SHORT_PRESS,
    EVENT_BUTTON_LONG_PRESS,
    EVENT_ADC_READING,
} event_type_t;

/*
 * Formato común que viaja por la cola. Los campos de medida solo tienen
 * significado cuando type es EVENT_ADC_READING; los eventos del botón los
 * inicializan a cero para que la estructura siempre esté completamente definida.
 */
typedef struct {
    event_type_t type;
    uint16_t adc_raw;        /* Solo válido para EVENT_ADC_READING. */
    uint16_t brightness_pct; /* Porcentaje de 0 a 100; solo válido para EVENT_ADC_READING. */
} event_t;

/* Inicializa la cola compartida; se llama una vez antes de arrancar servicios. */
void events_init(void);

/*
 * Copia un evento en la cola sin bloquear. Devuelve 0 si se encoló y -ENOMSG
 * si no había espacio; en ese caso events.c también registra el descarte.
 */
int events_send(const event_t *event);

/*
 * Extrae el siguiente evento respetando timeout. El resultado de Zephyr se
 * devuelve directamente: 0 indica éxito y un código negativo indica fallo o
 * expiración del tiempo de espera.
 */
int events_receive(event_t *event, k_timeout_t timeout);

#endif // EVENTS_H