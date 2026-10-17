#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include "events.h"
#include "button_service.h"

#ifdef CONFIG_PM
#include <zephyr/pm/policy.h>
#endif

/* substate-id of "stop2" in stm32wl.dtsi (stop0=1, stop1=2, stop2=3) */
#define PM_SUBSTATE_STOP2 3

int main(void) {
  events_init();
  printk("Brightness control: events initialized\n");

  #ifdef CONFIG_PM
    /* STOP2 resets non-retained pripheral registers; the ADC stops converting */
    pm_policy_state_lock_get(PM_STATE_SUSPEND_TO_IDLE, PM_SUBSTATE_STOP2);
  #endif

  if (button_service_init() != 0) {
    printk("Button init failed\n");
  }

  printk("Brightness control started\n");
  return 0;
}
