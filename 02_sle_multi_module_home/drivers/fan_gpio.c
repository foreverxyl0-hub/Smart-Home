#include "fan_gpio.h"

#include <stdio.h>

#include "common_def.h"
#include "gpio.h"
#include "pinctrl.h"

#ifndef XH_FAN_GPIO_PIN
#define XH_FAN_GPIO_PIN 9
#endif

#ifndef XH_FAN_GPIO_MODE
#define XH_FAN_GPIO_MODE 0
#endif

static bool g_fan_gpio_inited;
static bool g_fan_gpio_on;

void fan_gpio_init(void)
{
    if (g_fan_gpio_inited) {
        return;
    }

    uapi_pin_set_mode((pin_t)XH_FAN_GPIO_PIN, (pin_mode_t)XH_FAN_GPIO_MODE);
    uapi_pin_set_ds((pin_t)XH_FAN_GPIO_PIN, PIN_DS_7);
    uapi_gpio_set_dir((pin_t)XH_FAN_GPIO_PIN, GPIO_DIRECTION_OUTPUT);
    uapi_gpio_set_val((pin_t)XH_FAN_GPIO_PIN, GPIO_LEVEL_LOW);
    g_fan_gpio_on = false;
    g_fan_gpio_inited = true;
    printf("[fan_gpio] init pin=%d mode=%d default=off\r\n", XH_FAN_GPIO_PIN, XH_FAN_GPIO_MODE);
}

void fan_gpio_set_on(bool on)
{
    if (!g_fan_gpio_inited) {
        fan_gpio_init();
    }

    errcode_t ret = uapi_gpio_set_val((pin_t)XH_FAN_GPIO_PIN, on ? GPIO_LEVEL_HIGH : GPIO_LEVEL_LOW);
    g_fan_gpio_on = on;
    printf("[fan_gpio] set %s ret=0x%x out=%u\r\n", on ? "on" : "off",
        (unsigned int)ret, (unsigned int)uapi_gpio_get_output_val((pin_t)XH_FAN_GPIO_PIN));
}

bool fan_gpio_get_on(void)
{
    return g_fan_gpio_on;
}

void fan_gpio_sync_from_output(void)
{
    if (!g_fan_gpio_inited) {
        fan_gpio_init();
    }

    g_fan_gpio_on = (uapi_gpio_get_output_val((pin_t)XH_FAN_GPIO_PIN) == GPIO_LEVEL_HIGH);
}

void fan_gpio_toggle_silent(void)
{
    if (!g_fan_gpio_inited) {
        fan_gpio_init();
    }

    (void)uapi_gpio_toggle((pin_t)XH_FAN_GPIO_PIN);
    fan_gpio_sync_from_output();
}

void fan_gpio_toggle(void)
{
    if (!g_fan_gpio_inited) {
        fan_gpio_init();
    }

    errcode_t ret = uapi_gpio_toggle((pin_t)XH_FAN_GPIO_PIN);
    fan_gpio_sync_from_output();
    printf("[fan_gpio] toggle %s ret=0x%x out=%u\r\n", g_fan_gpio_on ? "on" : "off",
        (unsigned int)ret, (unsigned int)uapi_gpio_get_output_val((pin_t)XH_FAN_GPIO_PIN));
}
