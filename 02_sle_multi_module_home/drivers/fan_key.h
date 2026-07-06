#ifndef FAN_KEY_H
#define FAN_KEY_H

#include <stdbool.h>

void fan_key_init(void);
bool fan_key_consume_toggled(bool *fan_on, const char **source);

#endif
