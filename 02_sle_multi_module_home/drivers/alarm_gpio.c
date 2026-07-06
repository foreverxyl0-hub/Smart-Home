#include "alarm_gpio.h"

#include <stdio.h>

#include "cmsis_os2.h"
#include "common_def.h"
#include "errcode.h"
#include "gpio.h"
#include "pinctrl.h"
#include "soc_osal.h"
#include "tcxo.h"

#ifndef XH_ALARM_LED_PIN
#define XH_ALARM_LED_PIN 9
#endif

#ifndef XH_ALARM_LED_MODE
#define XH_ALARM_LED_MODE 0
#endif

#ifndef XH_ALARM_BUZZER_PIN
#define XH_ALARM_BUZZER_PIN 7
#endif

#ifndef XH_ALARM_BUZZER_MODE
#define XH_ALARM_BUZZER_MODE 0
#endif

#ifndef XH_ALARM_LED_ACTIVE_LEVEL
#define XH_ALARM_LED_ACTIVE_LEVEL 1
#endif

#ifndef XH_ALARM_BUZZER_ACTIVE_LEVEL
#define XH_ALARM_BUZZER_ACTIVE_LEVEL 1
#endif

#ifndef XH_ALARM_BOOT_BEEP_TEST
#define XH_ALARM_BOOT_BEEP_TEST 0
#endif

#ifndef XH_ALARM_BUZZER_FREQ_HZ
#define XH_ALARM_BUZZER_FREQ_HZ 2731
#endif

#ifndef XH_ALARM_BUZZER_DUTY_PERCENT
#define XH_ALARM_BUZZER_DUTY_PERCENT 50
#endif

#ifndef XH_ALARM_BUZZER_WAVE_ENABLE
#define XH_ALARM_BUZZER_WAVE_ENABLE 1
#endif

#ifndef XH_ALARM_BUZZER_STACK_SIZE
#define XH_ALARM_BUZZER_STACK_SIZE 0x1000
#endif

#ifndef XH_ALARM_BUZZER_PRIO
#define XH_ALARM_BUZZER_PRIO 26
#endif

#ifndef XH_ALARM_BUZZER_PULSE_MS
#define XH_ALARM_BUZZER_PULSE_MS 0
#endif

#if XH_ALARM_BUZZER_FREQ_HZ <= 0
#undef XH_ALARM_BUZZER_FREQ_HZ
#define XH_ALARM_BUZZER_FREQ_HZ 2731
#endif

static bool g_alarm_gpio_inited;
static bool g_alarm_buzzer_task_started;
static bool g_alarm_led_on;
static bool g_alarm_buzzer_on;
static bool g_alarm_buzzer_wave_mode;
static volatile bool g_alarm_buzzer_wave_on;
static volatile bool g_alarm_buzzer_wave_level;
static volatile uint32_t g_alarm_buzzer_wave_cycles_left;

static errcode_t alarm_gpio_write_level(pin_t pin, gpio_level_t level)
{
    return uapi_gpio_set_val(pin, level);
}

static void alarm_gpio_buzzer_configure_pin(void)
{
    uapi_pin_set_mode((pin_t)XH_ALARM_BUZZER_PIN, (pin_mode_t)XH_ALARM_BUZZER_MODE);
    uapi_pin_set_ds((pin_t)XH_ALARM_BUZZER_PIN, PIN_DS_7);
    uapi_gpio_set_dir((pin_t)XH_ALARM_BUZZER_PIN, GPIO_DIRECTION_OUTPUT);
}

static gpio_level_t alarm_gpio_level_from_state(bool on, uint8_t active_level)
{
    if (on) {
        return (active_level != 0) ? GPIO_LEVEL_HIGH : GPIO_LEVEL_LOW;
    }
    return (active_level != 0) ? GPIO_LEVEL_LOW : GPIO_LEVEL_HIGH;
}

static gpio_level_t alarm_gpio_buzzer_active_level(void)
{
    return (XH_ALARM_BUZZER_ACTIVE_LEVEL != 0) ? GPIO_LEVEL_HIGH : GPIO_LEVEL_LOW;
}

static gpio_level_t alarm_gpio_buzzer_inactive_level(void)
{
    return (XH_ALARM_BUZZER_ACTIVE_LEVEL != 0) ? GPIO_LEVEL_LOW : GPIO_LEVEL_HIGH;
}

static uint32_t alarm_gpio_buzzer_high_us(void)
{
    uint32_t period_us = 1000000U / (uint32_t)XH_ALARM_BUZZER_FREQ_HZ;
    uint32_t high_us = (period_us * (uint32_t)XH_ALARM_BUZZER_DUTY_PERCENT) / 100U;

    if (high_us == 0U) {
        high_us = 1U;
    }
    if (high_us >= period_us) {
        high_us = period_us - 1U;
    }
    return high_us;
}

static uint32_t alarm_gpio_buzzer_low_us(void)
{
    uint32_t period_us = 1000000U / (uint32_t)XH_ALARM_BUZZER_FREQ_HZ;
    uint32_t high_us = alarm_gpio_buzzer_high_us();

    if (period_us <= high_us) {
        return 1U;
    }
    return period_us - high_us;
}

static void alarm_gpio_buzzer_force_inactive(void)
{
    g_alarm_buzzer_wave_level = false;
    g_alarm_buzzer_wave_cycles_left = 0;
    (void)alarm_gpio_write_level((pin_t)XH_ALARM_BUZZER_PIN,
        alarm_gpio_buzzer_inactive_level());
}

static void alarm_gpio_buzzer_task(void *arg)
{
    (void)arg;
    const uint32_t high_us = alarm_gpio_buzzer_high_us();
    const uint32_t low_us = alarm_gpio_buzzer_low_us();
    const uint32_t period_us = high_us + low_us;
    bool was_on = false;

    printf("[WS63-ALARM] buzzer wave task start pin=%d freq=%d duty=%d high_us=%u low_us=%u\r\n",
        XH_ALARM_BUZZER_PIN, XH_ALARM_BUZZER_FREQ_HZ,
        XH_ALARM_BUZZER_DUTY_PERCENT, (unsigned int)high_us,
        (unsigned int)low_us);

    while (1) {
        if (!g_alarm_buzzer_wave_on) {
            if (was_on) {
                printf("[WS63-ALARM] buzzer wave stop pin=%d\r\n",
                    XH_ALARM_BUZZER_PIN);
                was_on = false;
            }
            if (!g_alarm_buzzer_on || g_alarm_buzzer_wave_mode) {
                alarm_gpio_buzzer_force_inactive();
            }
            osal_msleep(20);
            continue;
        }

        if (!was_on) {
            printf("[WS63-ALARM] buzzer wave start pin=%d freq=%d duty=%d\r\n",
                XH_ALARM_BUZZER_PIN, XH_ALARM_BUZZER_FREQ_HZ,
                XH_ALARM_BUZZER_DUTY_PERCENT);
            was_on = true;
        }
        g_alarm_buzzer_wave_level = true;
        (void)alarm_gpio_write_level((pin_t)XH_ALARM_BUZZER_PIN,
            alarm_gpio_buzzer_active_level());
        (void)uapi_tcxo_delay_us(high_us);
        g_alarm_buzzer_wave_level = false;
        (void)alarm_gpio_write_level((pin_t)XH_ALARM_BUZZER_PIN,
            alarm_gpio_buzzer_inactive_level());
        (void)uapi_tcxo_delay_us(low_us);
        if (g_alarm_buzzer_wave_cycles_left > 0) {
            g_alarm_buzzer_wave_cycles_left--;
            if (g_alarm_buzzer_wave_cycles_left == 0) {
                g_alarm_buzzer_wave_on = false;
                alarm_gpio_buzzer_force_inactive();
                printf("[WS63-ALARM] buzzer wave pulse done pin=%d period_us=%u\r\n",
                    XH_ALARM_BUZZER_PIN, (unsigned int)period_us);
            }
        }
    }
}

static void alarm_gpio_buzzer_task_start(void)
{
#if XH_ALARM_BUZZER_WAVE_ENABLE
    if (g_alarm_buzzer_task_started) {
        return;
    }
    osThreadAttr_t attr = {0};
    attr.name = "xh_alarm_bz";
    attr.stack_size = XH_ALARM_BUZZER_STACK_SIZE;
    attr.priority = XH_ALARM_BUZZER_PRIO;
    if (osThreadNew((osThreadFunc_t)alarm_gpio_buzzer_task, NULL, &attr) == NULL) {
        printf("[WS63-ALARM] buzzer wave task create fail pin=%d\r\n",
            XH_ALARM_BUZZER_PIN);
        return;
    }
    g_alarm_buzzer_task_started = true;
#endif
}

void alarm_gpio_init(void)
{
    if (g_alarm_gpio_inited) {
        return;
    }

    uapi_pin_set_mode((pin_t)XH_ALARM_LED_PIN, (pin_mode_t)XH_ALARM_LED_MODE);
    uapi_pin_set_ds((pin_t)XH_ALARM_LED_PIN, PIN_DS_7);
    uapi_gpio_set_dir((pin_t)XH_ALARM_LED_PIN, GPIO_DIRECTION_OUTPUT);

    alarm_gpio_buzzer_configure_pin();

    g_alarm_gpio_inited = true;
    alarm_gpio_set(false, false);
    alarm_gpio_buzzer_task_start();
#if XH_ALARM_BOOT_BEEP_TEST
    g_alarm_buzzer_wave_on = true;
    osal_mdelay(150);
    g_alarm_buzzer_wave_on = false;
    alarm_gpio_buzzer_force_inactive();
    osal_mdelay(150);
    printf("[WS63-ALARM] boot beep test done buzzer_pin=%d mode=%d freq=%d wave=%d\r\n",
        XH_ALARM_BUZZER_PIN, XH_ALARM_BUZZER_MODE,
        XH_ALARM_BUZZER_FREQ_HZ, XH_ALARM_BUZZER_WAVE_ENABLE);
#else
    g_alarm_buzzer_wave_on = false;
    alarm_gpio_buzzer_force_inactive();
    printf("[WS63-ALARM] boot beep test disabled buzzer_pin=%d default=off\r\n",
        XH_ALARM_BUZZER_PIN);
#endif
    printf("[alarm_gpio] init led_pin=%d led_mode=%d led_active=%d buzzer_pin=%d buzzer_mode=%d buzzer_active=%d freq=%d duty=%d wave=%d default=off\r\n",
        XH_ALARM_LED_PIN, XH_ALARM_LED_MODE, XH_ALARM_LED_ACTIVE_LEVEL,
        XH_ALARM_BUZZER_PIN, XH_ALARM_BUZZER_MODE, XH_ALARM_BUZZER_ACTIVE_LEVEL,
        XH_ALARM_BUZZER_FREQ_HZ, XH_ALARM_BUZZER_DUTY_PERCENT,
        XH_ALARM_BUZZER_WAVE_ENABLE);
}

void alarm_gpio_set(bool led_on, bool buzzer_on)
{
    alarm_gpio_set_ex(led_on, buzzer_on, false);
}

void alarm_gpio_set_ex(bool led_on, bool buzzer_on, bool buzzer_wave)
{
    if (!g_alarm_gpio_inited) {
        alarm_gpio_init();
    }

    gpio_level_t led_level = alarm_gpio_level_from_state(led_on, XH_ALARM_LED_ACTIVE_LEVEL);

    errcode_t led_ret = alarm_gpio_write_level((pin_t)XH_ALARM_LED_PIN, led_level);
#if XH_ALARM_BUZZER_WAVE_ENABLE
    if (!g_alarm_buzzer_task_started) {
        alarm_gpio_buzzer_task_start();
    }
    if (buzzer_wave) {
        alarm_gpio_buzzer_configure_pin();
        if (g_alarm_buzzer_on != buzzer_on || !g_alarm_buzzer_wave_mode) {
            printf("[WS63-ALARM] buzzer wave request pin=%d enable=%u freq=%d duty=%d\r\n",
                XH_ALARM_BUZZER_PIN, buzzer_on ? 1 : 0,
                XH_ALARM_BUZZER_FREQ_HZ, XH_ALARM_BUZZER_DUTY_PERCENT);
        }
        uint32_t period_us = alarm_gpio_buzzer_high_us() + alarm_gpio_buzzer_low_us();
        uint32_t cycles = 0U;
        if (XH_ALARM_BUZZER_PULSE_MS > 0) {
            cycles = ((uint32_t)XH_ALARM_BUZZER_PULSE_MS * 1000U) / period_us;
            if (cycles == 0U) {
                cycles = 1U;
            }
        }
        g_alarm_buzzer_wave_cycles_left = buzzer_on ? cycles : 0U;
        g_alarm_buzzer_wave_on = buzzer_on;
        if (!buzzer_on) {
            alarm_gpio_buzzer_force_inactive();
        }
    } else {
        if (g_alarm_buzzer_wave_on) {
            printf("[WS63-ALARM] buzzer wave cancel for static pin=%d\r\n",
                XH_ALARM_BUZZER_PIN);
        }
        g_alarm_buzzer_wave_on = false;
        gpio_level_t static_level = alarm_gpio_level_from_state(buzzer_on,
            XH_ALARM_BUZZER_ACTIVE_LEVEL);
        (void)alarm_gpio_write_level((pin_t)XH_ALARM_BUZZER_PIN, static_level);
        g_alarm_buzzer_wave_level = buzzer_on;
    }
    errcode_t buzzer_ret = ERRCODE_SUCC;
    gpio_level_t buzzer_level = buzzer_wave ?
        (g_alarm_buzzer_wave_level ? alarm_gpio_buzzer_active_level() : alarm_gpio_buzzer_inactive_level()) :
        alarm_gpio_level_from_state(buzzer_on, XH_ALARM_BUZZER_ACTIVE_LEVEL);
#else
    (void)buzzer_wave;
    gpio_level_t buzzer_level = alarm_gpio_level_from_state(buzzer_on, XH_ALARM_BUZZER_ACTIVE_LEVEL);
    errcode_t buzzer_ret = alarm_gpio_write_level((pin_t)XH_ALARM_BUZZER_PIN, buzzer_level);
#endif
    g_alarm_led_on = led_on;
    g_alarm_buzzer_on = buzzer_on;
    g_alarm_buzzer_wave_mode = buzzer_wave;
    printf("[WS63-ALARM] gpio mode=%s led_pin=%u led_ret=0x%X led_out=%u buzzer_pin=%u buzzer_ret=0x%X buzzer_enable=%u buzzer_read=%u pulse_ms=%u cycles=%u mask=0x%02X\r\n",
        buzzer_wave ? "wave" : "static",
        XH_ALARM_LED_PIN, (unsigned int)led_ret, led_level ? 1 : 0,
        XH_ALARM_BUZZER_PIN, (unsigned int)buzzer_ret, buzzer_on ? 1 : 0,
        buzzer_level ? 1 : 0, (unsigned int)XH_ALARM_BUZZER_PULSE_MS,
        (unsigned int)g_alarm_buzzer_wave_cycles_left,
        alarm_gpio_get_output_mask());
    printf("[alarm_gpio] led=%u buzzer=%u drive=%s out_mask=0x%02X\r\n",
        led_on ? 1 : 0, buzzer_on ? 1 : 0,
        buzzer_wave ? "wave" : "static", alarm_gpio_get_output_mask());
}

void alarm_gpio_get(bool *led_on, bool *buzzer_on)
{
    if (led_on != NULL) {
        *led_on = g_alarm_led_on;
    }
    if (buzzer_on != NULL) {
        *buzzer_on = g_alarm_buzzer_on;
    }
}

bool alarm_gpio_buzzer_enabled(void)
{
    return g_alarm_buzzer_on;
}

uint8_t alarm_gpio_get_output_mask(void)
{
    uint8_t mask = 0;
    if (g_alarm_led_on) {
        mask |= 0x01;
    }
    if (g_alarm_buzzer_on) {
        mask |= 0x02;
    }
    return mask;
}
