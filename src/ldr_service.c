#include "ldr_service.h"
#include "events.h"
#include <zephyr/drivers/adc.h>
#include <zephyr/sys/printk.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/atomic.h>

#ifdef CONFIG_PM
#include <zephyr/pm/policy.h>
/*
 * Durante la conversión se bloquean los estados de suspensión porque el ADC
 * no completa la lectura si el sistema entra en STOP. Sin CONFIG_PM, estas
 * operaciones no tienen efecto.
 */
#define LDR_PM_LOCK() pm_policy_state_lock_get(PM_STATE_SUSPEND_TO_IDLE, PM_ALL_SUBSTATES)
#define LDR_PM_UNLOCK() pm_policy_state_lock_put(PM_STATE_SUSPEND_TO_IDLE, PM_ALL_SUBSTATES)
#else
#define LDR_PM_LOCK() do { } while (0)
#define LDR_PM_UNLOCK() do { } while (0)
#endif

/* Canal ADC descrito por el alias analog-in-pin del devicetree del proyecto. */
#define ADC_NODE DT_ALIAS(analog_in_pin)
static const struct adc_dt_spec ldr_adc = ADC_DT_SPEC_GET(ADC_NODE);

/* GPIO que alimenta el divisor del LDR solo durante la toma de cada muestra. */
#define LDR_PWR_NODE DT_ALIAS(ldr_pwr)
static const struct gpio_dt_spec ldr_pwr = GPIO_DT_SPEC_GET(LDR_PWR_NODE, gpios);

/* El ADC escribe aquí el valor de conversión de 16 bits. */
static int16_t sample;

/* Recursos y temporización propios del hilo periódico del sensor. */
#define LDR_THREAD_STACK_SIZE 1024
#define LDR_THREAD_PRIORITY 7
#define LDR_PERIOD_MS 2000
#define LDR_SETTLE_MS 1

/*
 * La bandera expresa si se permiten nuevas adquisiciones. El semáforo despierta
 * al hilo cuando queda bloqueado por una suspensión solicitada por la FSM.
 */
static atomic_t acquisition_enabled = ATOMIC_INIT(1);
static K_SEM_DEFINE(resume_sem, 0, 1);

/* El hilo de muestreo se inicia automáticamente como parte del arranque Zephyr. */
K_THREAD_DEFINE(ldr_thread_id, LDR_THREAD_STACK_SIZE,
                ldr_thread_entry, NULL, NULL, NULL, 
                LDR_THREAD_PRIORITY, 0, 0);

void ldr_service_suspend(void) {
    /* El hilo comprobará la bandera en el siguiente punto seguro de su bucle. */
    atomic_set(&acquisition_enabled, 0);
    printk("LDR acquisition suspended\n");
}

void ldr_service_resume(void) {
    /* Habilita de nuevo las lecturas y despierta al hilo si esperaba en semáforo. */
    atomic_set(&acquisition_enabled, 1);
    k_sem_give(&resume_sem);
    printk("LDR acquisition resumed\n");
}

static int ldr_acquire(event_t *event, struct adc_sequence *seq) {
    /* Evita entrar en suspensión mientras el periférico realiza la conversión. */
    LDR_PM_LOCK();

    /* Alimenta el divisor y espera su breve estabilización antes de medir. */
    gpio_pin_set_dt(&ldr_pwr, 1);
    k_msleep(LDR_SETTLE_MS);

    /* La lectura se deposita en sample mediante el buffer de adc_sequence. */
    int ret = adc_read_dt(&ldr_adc, seq);

    /* Solicita apagar la alimentación también si adc_read_dt devolvió error. */
    gpio_pin_set_dt(&ldr_pwr, 0);           /* Se ejecuta también si la conversión falla. */
    LDR_PM_UNLOCK();

    if (ret != 0) {
        return ret;
    }

    /*
     * Evita propagar un valor negativo del buffer como lectura sin signo y
     * calcula el porcentaje entero sobre el rango completo del ADC (0..4095).
     */
    int32_t value = sample < 0 ? 0 : sample;

    event->type = EVENT_ADC_READING;
    event->adc_raw = (uint16_t)value;
    event->brightness_pct = (uint8_t)MIN((value * 100) / ADC_FULL_SCALE, 100);
    return 0;
}

void ldr_thread_entry(void *p1, void *p2, void *p3) {
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    /*
     * La secuencia especifica el buffer de destino; adc_sequence_init_dt
     * completará el resto de parámetros a partir del devicetree.
     */
    struct adc_sequence sequence = {
        .buffer = &sample,
        .buffer_size = sizeof(sample),
    };

    /* Validaciones iniciales: si falla una, el hilo informa y no mide. */
    if (!adc_is_ready_dt(&ldr_adc)) {
        printk("Error: LDR devices is not ready\n");
        return;
    }

    if (adc_channel_setup_dt(&ldr_adc) != 0 ||
        adc_sequence_init_dt(&ldr_adc, &sequence) != 0) {
        printk("Error: LDR setup failed\n");
        return;
    }

    if (!gpio_is_ready_dt(&ldr_pwr)) {
        printk("Error: GPIO pin is not ready\n");
        return;
    }

    /* La alimentación del divisor comienza apagada para ahorrar energía. */
    if (gpio_pin_configure_dt(&ldr_pwr, GPIO_OUTPUT_INACTIVE) != 0) {
        printk("Error: Configuration of the GPIO Supply pin failed\n");
        return;
    }

    /*
     * Se usa una fecha objetivo absoluta para evitar que el tiempo empleado en
     * cada lectura se sume al período y haga derivar el intervalo de 2 segundos.
     */
    int64_t next_wakeup = k_uptime_get();

    while (1) {
        if (!atomic_get(&acquisition_enabled)) {
            /* Espera sin consumir CPU hasta que FSM reactive el servicio. */
            while (!atomic_get(&acquisition_enabled)) {
                k_sem_take(&resume_sem, K_FOREVER);
            }
            /* Al reanudar, la siguiente espera de período parte del instante actual. */
            next_wakeup = k_uptime_get();
        }

        event_t event;
        /* Publica la muestra; los errores de lectura se informan y no se encolan. */
        if (ldr_acquire(&event, &sequence) == 0) {
            events_send(&event);
        } else {
            printk("Error:ADC read failed\n");
        }

        next_wakeup += LDR_PERIOD_MS;
        int64_t remaining = next_wakeup - k_uptime_get();
        if (remaining > 0) {
            /* Duerme solo el tiempo que falta hasta el siguiente instante previsto. */
            k_msleep(remaining);
        } else {
            /* Si la operación excedió el período, inicia un nuevo intervalo desde ahora. */
            next_wakeup = k_uptime_get();
        }
    }
}
