#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include "events.h"
#include "ldr_service.h"
#include "led.h"
#include "fsm.h"

#define FSM_STACK_SIZE 1024
#define FSM_PRIORITY 5

/* Estado que consulta y modifica exclusivamente el hilo de la FSM. */
static fsm_mode_t mode = FSM_MODE_NORMAL;
/* Última muestra válida; permite recuperar el color al volver a NORMAL. */
static uint16_t last_adc_raw = 0;

fsm_mode_t fsm_get_mode(void) {
    return mode;
}

static led_color_t color_from_raw(uint16_t raw) {
    /*
     * Compara la lectura sin convertirla primero a porcentaje entero: así los
     * límites 33 % y 66 % no dependen del truncamiento de la división.
     */
    uint32_t scaled = (uint32_t)raw * 100U;

    if (scaled < 33U * ADC_FULL_SCALE) {
        return LED_COLOR_RED;
    }
    if (scaled <= 66U * ADC_FULL_SCALE) {
        return LED_COLOR_YELLOW;
    }
    return LED_COLOR_GREEN;
}

static void enter_mode (fsm_mode_t new_mode) {
    /* Se actualiza el estado antes de aplicar las acciones propias del modo. */
    mode = new_mode;

    switch (mode) {
        case FSM_MODE_NORMAL:
            printk("Mode: NORMAL\n");
            /* En NORMAL se retoman las medidas y se presenta el último color. */
            ldr_service_resume();
            led_set_color(color_from_raw(last_adc_raw));
            break;

        case FSM_MODE_BLUE:
            printk("Mode: BLUE\n");
            /* El color azul fijo distingue este modo y no se necesitan medidas. */
            ldr_service_suspend();
            led_set_color(LED_COLOR_BLUE);
            break;

        case FSM_MODE_OFF:
            printk("System OFF\n");
            /* OFF apaga la salida y detiene la adquisición periódica del LDR. */
            ldr_service_suspend();
            led_set_color(LED_COLOR_OFF);
            break;
    }
}

static void handle_event (const event_t *event) {
    switch (event->type) {
        case EVENT_BUTTON_SHORT_PRESS:
            /*
             * Una pulsación corta alterna NORMAL y BLUE. Desde OFF se vuelve
             * explícitamente a NORMAL, tal como define el comportamiento.
             */
            if (mode == FSM_MODE_NORMAL) {
                enter_mode(FSM_MODE_BLUE);
            } else {
                enter_mode(FSM_MODE_NORMAL);
            }
            break;

        case EVENT_BUTTON_LONG_PRESS:
            /* La pulsación larga lleva a OFF; en OFF no produce otra transición. */
            if (mode != FSM_MODE_OFF) {
                enter_mode(FSM_MODE_OFF);
            }
            break;

        case EVENT_ADC_READING:
            /*
             * Solo se aceptan muestras para la interfaz cuando está en NORMAL.
             * En los demás modos la FSM las ignora y no cambia el color guardado.
             */
            if (mode == FSM_MODE_NORMAL) {
                last_adc_raw = event->adc_raw;
                printk("ADC raw: %u, brightness: %u%%\n",
                       event->adc_raw, event->brightness_pct);
                led_set_color(color_from_raw(event->adc_raw));
            }
            break;

        default:
            break;
    }
}

static void fsm_thread(void *p1, void *p2, void *p3) {
    event_t event;

    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    /* La FSM inicializa el hardware LED antes de emitir su primer estado. */
    if (led_init() != 0) {
        printk("Error: LED init failed\n");
        return;
    }
    enter_mode(FSM_MODE_NORMAL);

    /*
     * La cola desacopla esta lógica de los servicios productores. K_FOREVER
     * evita sondeo: el hilo queda bloqueado hasta recibir un mensaje.
     */
    while (1) {
        if (events_receive(&event, K_FOREVER) == 0) {
            handle_event(&event);
        }
    }
}

/* Hilo de la máquina de estados: pila de 1024 bytes y prioridad 5 en Zephyr. */
K_THREAD_DEFINE(fsm_tid, FSM_STACK_SIZE, fsm_thread, NULL, NULL, NULL, FSM_PRIORITY, 0, 0);