#include "fan_key.h"

#include <stdio.h>

#include "common_def.h"
#include "fan_gpio.h"
#include "gpio.h"
#include "pinctrl.h"
#include "soc_osal.h"

#ifndef XH_FAN_KEY_PIN
#define XH_FAN_KEY_PIN 5
#endif

#ifndef XH_FAN_KEY_MODE
#define XH_FAN_KEY_MODE 4
#endif

#ifndef XH_FAN_KEY_DEBOUNCE_MS
#define XH_FAN_KEY_DEBOUNCE_MS 30
#endif

static bool g_fan_key_inited;
static volatile bool g_fan_key_irq_event;
static volatile bool g_fan_key_wait_release;
static volatile bool g_fan_key_enabled;

static void fan_key_isr(void)
{
    if (!g_fan_key_enabled || g_fan_key_wait_release) {
        return;
    }

    g_fan_key_irq_event = true;
}

static bool fan_key_toggle_after_debounce(bool *fan_on, const char **source, const char *event_source)
{
    g_fan_key_wait_release = true;
    g_fan_key_irq_event = false;

    osal_msleep(XH_FAN_KEY_DEBOUNCE_MS);
    gpio_level_t level = uapi_gpio_get_val((pin_t)XH_FAN_KEY_PIN);
    if (level != GPIO_LEVEL_LOW) {
        g_fan_key_wait_release = false;
        (void)uapi_gpio_clear_interrupt((pin_t)XH_FAN_KEY_PIN);
        printf("[fan_key] %s ignored level=%u\r\n", event_source, (unsigned int)level);
        return false;
    }

    fan_gpio_toggle();
    if (fan_on != NULL) {
        *fan_on = fan_gpio_get_on();
    }
    if (source != NULL) {
        *source = event_source;
    }
    return true;
}

void fan_key_init(void)
{
    if (g_fan_key_inited) {
        return;
    }

    g_fan_key_enabled = false;
    g_fan_key_irq_event = false;
    g_fan_key_wait_release = false;

    uapi_pin_set_mode((pin_t)XH_FAN_KEY_PIN, (pin_mode_t)XH_FAN_KEY_MODE);
    uapi_gpio_set_dir((pin_t)XH_FAN_KEY_PIN, GPIO_DIRECTION_INPUT);
    uapi_gpio_set_val((pin_t)XH_FAN_KEY_PIN, GPIO_LEVEL_HIGH);
    gpio_level_t level = uapi_gpio_get_val((pin_t)XH_FAN_KEY_PIN);
    errcode_t ret = uapi_gpio_register_isr_func((pin_t)XH_FAN_KEY_PIN, GPIO_INTERRUPT_FALLING_EDGE, fan_key_isr);
    (void)uapi_gpio_clear_interrupt((pin_t)XH_FAN_KEY_PIN);

    g_fan_key_wait_release = (level == GPIO_LEVEL_LOW);
    g_fan_key_enabled = true;
    g_fan_key_inited = true;
    printf("[fan_key] init pin=%d mode=%d irq=falling ret=0x%x level=%u wait_release=%u\r\n",
        XH_FAN_KEY_PIN, XH_FAN_KEY_MODE, (unsigned int)ret,
        (unsigned int)level, (unsigned int)g_fan_key_wait_release);
}

bool fan_key_consume_toggled(bool *fan_on, const char **source)
{
    if (!g_fan_key_inited) {
        return false;
    }

    gpio_level_t level = uapi_gpio_get_val((pin_t)XH_FAN_KEY_PIN);
    if (g_fan_key_wait_release) {
        if (level == GPIO_LEVEL_HIGH) {
            g_fan_key_wait_release = false;
            g_fan_key_irq_event = false;
            (void)uapi_gpio_clear_interrupt((pin_t)XH_FAN_KEY_PIN);
            printf("[fan_key] released\r\n");
        }
        return false;
    }

    if (g_fan_key_irq_event) {
        g_fan_key_irq_event = false;
        return fan_key_toggle_after_debounce(fan_on, source, "irq");
    }

    if (level != GPIO_LEVEL_LOW) {
        return false;
    }

    return fan_key_toggle_after_debounce(fan_on, source, "poll");
}
