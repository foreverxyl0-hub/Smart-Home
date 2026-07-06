#include "ld2401_gpio.h"

#include <stdio.h>

#include "common_def.h"
#include "gpio.h"
#include "pinctrl.h"

#ifndef XH_LD2401_OUT_PIN
#define XH_LD2401_OUT_PIN 9
#endif

#ifndef XH_LD2401_OUT_MODE
#define XH_LD2401_OUT_MODE 0
#endif

static bool g_ld2401_gpio_inited;

void ld2401_gpio_init(void)
{
    if (g_ld2401_gpio_inited) {
        return;
    }

    uapi_pin_set_mode((pin_t)XH_LD2401_OUT_PIN, (pin_mode_t)XH_LD2401_OUT_MODE);
    uapi_gpio_set_dir((pin_t)XH_LD2401_OUT_PIN, GPIO_DIRECTION_INPUT);
    g_ld2401_gpio_inited = true;
    printf("[ld2401_gpio] init out_pin=%d mode=%d present=%u\r\n",
        XH_LD2401_OUT_PIN, XH_LD2401_OUT_MODE, ld2401_gpio_is_present() ? 1 : 0);
}

bool ld2401_gpio_is_present(void)
{
    if (!g_ld2401_gpio_inited) {
        ld2401_gpio_init();
    }
    return uapi_gpio_get_val((pin_t)XH_LD2401_OUT_PIN) == GPIO_LEVEL_HIGH;
}
