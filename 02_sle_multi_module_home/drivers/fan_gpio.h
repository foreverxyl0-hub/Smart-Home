#ifndef FAN_GPIO_H
#define FAN_GPIO_H

#include <stdbool.h>

void fan_gpio_init(void);
void fan_gpio_set_on(bool on);
bool fan_gpio_get_on(void);
void fan_gpio_toggle(void);
void fan_gpio_toggle_silent(void);
void fan_gpio_sync_from_output(void);

#endif
