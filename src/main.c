#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include "events.h"


int main(void) {
  events_init();
  printk("Brightness control: events initialized\n");

  return 0;
}
