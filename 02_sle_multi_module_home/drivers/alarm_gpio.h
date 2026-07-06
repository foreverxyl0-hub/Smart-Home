#ifndef ALARM_GPIO_H
#define ALARM_GPIO_H

#include <stdbool.h>
#include <stdint.h>

void alarm_gpio_init(void);
void alarm_gpio_set(bool led_on, bool buzzer_on);
void alarm_gpio_set_ex(bool led_on, bool buzzer_on, bool buzzer_wave);
void alarm_gpio_get(bool *led_on, bool *buzzer_on);
bool alarm_gpio_buzzer_enabled(void);
uint8_t alarm_gpio_get_output_mask(void);

#endif
