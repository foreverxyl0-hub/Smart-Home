#include "mq2_gpio.h"

#include <stdio.h>

#include "gpio.h"
#include "pinctrl.h"

#ifndef XH_MQ2_DOUT_PIN
#define XH_MQ2_DOUT_PIN 9
#endif

#ifndef XH_MQ2_DOUT_MODE
#define XH_MQ2_DOUT_MODE 0
#endif

#ifndef XH_MQ2_ALARM_ACTIVE_LEVEL
#define XH_MQ2_ALARM_ACTIVE_LEVEL 0
#endif

static bool g_mq2_gpio_inited;

void mq2_gpio_init(void)
{
    if (g_mq2_gpio_inited) {
        return;
    }

    uapi_pin_set_mode((pin_t)XH_MQ2_DOUT_PIN, (pin_mode_t)XH_MQ2_DOUT_MODE);
    uapi_gpio_set_dir((pin_t)XH_MQ2_DOUT_PIN, GPIO_DIRECTION_INPUT);
    g_mq2_gpio_inited = true;
    printf("[mq2_gpio] init dout_pin=%d mode=%d active_level=%d level=%u\r\n",
        XH_MQ2_DOUT_PIN, XH_MQ2_DOUT_MODE, XH_MQ2_ALARM_ACTIVE_LEVEL,
        (unsigned int)mq2_gpio_get_level());
}

uint8_t mq2_gpio_get_level(void)
{
    if (!g_mq2_gpio_inited) {
        mq2_gpio_init();
    }
    return (uapi_gpio_get_val((pin_t)XH_MQ2_DOUT_PIN) == GPIO_LEVEL_HIGH) ? 1U : 0U;
}

bool mq2_gpio_is_alarm(void)
{
    return mq2_gpio_get_level() == (uint8_t)XH_MQ2_ALARM_ACTIVE_LEVEL;
}
