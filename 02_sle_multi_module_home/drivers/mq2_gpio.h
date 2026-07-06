#ifndef MQ2_GPIO_H
#define MQ2_GPIO_H

#include <stdbool.h>
#include <stdint.h>

void mq2_gpio_init(void);
bool mq2_gpio_is_alarm(void);
uint8_t mq2_gpio_get_level(void);

#endif
