#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include "events.h"
#include "button_service.h"

/* API de política de energía: solo se usa si CONFIG_PM está habilitado. */
#ifdef CONFIG_PM
#include <zephyr/pm/policy.h>
#endif

/*
 * Identificador del subestado STOP2 definido para STM32WL. Se impide ese
 * estado porque, según las pruebas descritas en el README, al reanudar desde
 * STOP2 el ADC no permite completar correctamente las conversiones.
 */
#define PM_SUBSTATE_STOP2 3

int main(void) {
  /* La cola debe estar lista antes de que los servicios publiquen eventos. */
  events_init();
  printk("Brightness control: events initialized\n");

  #ifdef CONFIG_PM
    /*
     * Bloquea la entrada en STOP2 durante el resto de la ejecución. Se siguen
     * permitiendo otros estados de bajo consumo compatibles con el ADC.
     */
    pm_policy_state_lock_get(PM_STATE_SUSPEND_TO_IDLE, PM_SUBSTATE_STOP2);
  #endif

  /* El servicio del botón configura B2 y sus interrupciones GPIO. */
  if (button_service_init() != 0) {
    printk("Button init failed\n");
  }

  /*
   * Los hilos de FSM y LDR se crean con K_THREAD_DEFINE en sus módulos.
   * Por eso main puede terminar tras la inicialización: el kernel mantiene
   * esos hilos en ejecución.
   */
  printk("Brightness control started\n");
  return 0;
}
