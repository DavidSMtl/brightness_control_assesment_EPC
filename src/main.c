#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include "events.h"
#include "button_service.h"


int main(void) {
  events_init();
  printk("Brightness control: events initialized\n");

  if (button_service_init() != 0) {
    printk("Button init failed\n");
  }

  printk("Brightness control started\n");
  return 0;
}
