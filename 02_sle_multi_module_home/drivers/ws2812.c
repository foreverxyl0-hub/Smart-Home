#include "ws2812.h"

#include <stdio.h>

#include "common_def.h"
#include "gpio.h"
#include "hal_gpio_v150_comm.h"
#include "hal_gpio_v150_regs_op.h"
#include "pinctrl.h"
#include "soc_osal.h"
#include "tcxo.h"

#ifndef XH_LIGHT_DIN_PIN
#define XH_LIGHT_DIN_PIN 9
#endif

#ifndef XH_LIGHT_DIN_MODE
#define XH_LIGHT_DIN_MODE 0
#endif

#ifndef XH_LIGHT_LED_COUNT
#define XH_LIGHT_LED_COUNT 6
#endif

#ifndef XH_WS2812_T0H_LOOPS
#define XH_WS2812_T0H_LOOPS 3
#endif

#ifndef XH_WS2812_T0L_LOOPS
#define XH_WS2812_T0L_LOOPS 8
#endif

#ifndef XH_WS2812_T1H_LOOPS
#define XH_WS2812_T1H_LOOPS 8
#endif

#ifndef XH_WS2812_T1L_LOOPS
#define XH_WS2812_T1L_LOOPS 3
#endif

#ifndef XH_WS2812_DIAG_ENABLE
#define XH_WS2812_DIAG_ENABLE 1
#endif

#ifndef XH_WS2812_CONTROL_PROFILE
#define XH_WS2812_CONTROL_PROFILE 0
#endif

typedef struct {
    uint8_t t0h;
    uint8_t t0l;
    uint8_t t1h;
    uint8_t t1l;
} ws2812_profile_t;

static const ws2812_profile_t g_ws2812_profiles[] = {
    { XH_WS2812_T0H_LOOPS, XH_WS2812_T0L_LOOPS, XH_WS2812_T1H_LOOPS, XH_WS2812_T1L_LOOPS },
    { 2, 12, 9, 5 },
    { 4, 16, 13, 7 },
    { 6, 24, 18, 10 },
    { 8, 32, 24, 14 },
};

static bool g_ws2812_inited;
static ws2812_color_t g_ws2812_color;
static uint32_t g_ws2812_channel;
static uint32_t g_ws2812_group;
static uint32_t g_ws2812_group_pin;

static inline void ws2812_delay_loops(uint32_t loops)
{
    while (loops-- > 0) {
        __asm__ volatile ("nop");
    }
}

static inline void ws2812_high(void)
{
    hal_gpio_gpio_data_set_set_bit(g_ws2812_channel, g_ws2812_group,
        g_ws2812_group_pin, 1U);
}

static inline void ws2812_low(void)
{
    hal_gpio_gpio_data_clr_set_bit(g_ws2812_channel, g_ws2812_group,
        g_ws2812_group_pin, 1U);
}

static uint8_t ws2812_profile_count(void)
{
    return (uint8_t)(sizeof(g_ws2812_profiles) / sizeof(g_ws2812_profiles[0]));
}

static const ws2812_profile_t *ws2812_get_profile(uint8_t profile)
{
    if (profile >= ws2812_profile_count()) {
        profile = 0;
    }
    return &g_ws2812_profiles[profile];
}

static void ws2812_send_bit(bool one, const ws2812_profile_t *profile)
{
    ws2812_high();
    ws2812_delay_loops(one ? profile->t1h : profile->t0h);
    ws2812_low();
    ws2812_delay_loops(one ? profile->t1l : profile->t0l);
}

static void ws2812_send_byte(uint8_t value, const ws2812_profile_t *profile)
{
    for (uint8_t mask = 0x80; mask != 0; mask >>= 1) {
        ws2812_send_bit((value & mask) != 0, profile);
    }
}

static uint8_t ws2812_scale(uint8_t value, uint8_t brightness)
{
    return (uint8_t)(((uint16_t)value * (uint16_t)brightness) / 255U);
}

static void ws2812_write_scaled_profile(uint8_t r, uint8_t g, uint8_t b,
    uint8_t brightness, uint8_t profile_id)
{
    uint8_t sr = ws2812_scale(r, brightness);
    uint8_t sg = ws2812_scale(g, brightness);
    uint8_t sb = ws2812_scale(b, brightness);
    const ws2812_profile_t *profile = ws2812_get_profile(profile_id);

    uint32_t irq = osal_irq_lock();
    for (uint16_t i = 0; i < XH_LIGHT_LED_COUNT; i++) {
        ws2812_send_byte(sg, profile);
        ws2812_send_byte(sr, profile);
        ws2812_send_byte(sb, profile);
    }
    osal_irq_restore(irq);

    ws2812_low();
    (void)uapi_tcxo_delay_us(100);
}

int ws2812_init(void)
{
    if (g_ws2812_inited) {
        return 0;
    }

    uapi_pin_set_mode((pin_t)XH_LIGHT_DIN_PIN, (pin_mode_t)XH_LIGHT_DIN_MODE);
    uapi_pin_set_ds((pin_t)XH_LIGHT_DIN_PIN, PIN_DS_7);
    uapi_gpio_set_dir((pin_t)XH_LIGHT_DIN_PIN, GPIO_DIRECTION_OUTPUT);
    if (hal_gpio_v150_pin_info_get((pin_t)XH_LIGHT_DIN_PIN, &g_ws2812_channel,
        &g_ws2812_group, &g_ws2812_group_pin) != ERRCODE_SUCC) {
        printf("[ws2812] pin info fail pin=%d\r\n", XH_LIGHT_DIN_PIN);
        return -1;
    }
    g_ws2812_inited = true;
    (void)ws2812_off();
    printf("[ws2812] init din_pin=%d mode=%d count=%d profiles=%u default_loops=%d/%d/%d/%d diag=%d\r\n",
        XH_LIGHT_DIN_PIN, XH_LIGHT_DIN_MODE, XH_LIGHT_LED_COUNT,
        ws2812_profile_count(), XH_WS2812_T0H_LOOPS, XH_WS2812_T0L_LOOPS,
        XH_WS2812_T1H_LOOPS, XH_WS2812_T1L_LOOPS, XH_WS2812_DIAG_ENABLE);
    printf("[WS63-LIGHT] ws2812 init ret=0 din_pin=%d mode=%d group=%u group_pin=%u count=%d\r\n",
        XH_LIGHT_DIN_PIN, XH_LIGHT_DIN_MODE, (unsigned int)g_ws2812_group,
        (unsigned int)g_ws2812_group_pin, XH_LIGHT_LED_COUNT);
    return 0;
}

int ws2812_set_color_profile(uint8_t r, uint8_t g, uint8_t b, uint8_t brightness,
    uint8_t profile)
{
    if (!g_ws2812_inited) {
        int init_ret = ws2812_init();
        if (init_ret != 0) {
            return init_ret;
        }
    }
    if (!g_ws2812_inited) {
        return -4;
    }
    g_ws2812_color.on = (brightness != 0 && (r != 0 || g != 0 || b != 0));
    g_ws2812_color.r = r;
    g_ws2812_color.g = g;
    g_ws2812_color.b = b;
    g_ws2812_color.brightness = brightness;

    if (!g_ws2812_color.on) {
        ws2812_write_scaled_profile(0, 0, 0, 0, profile);
    } else {
        ws2812_write_scaled_profile(r, g, b, brightness, profile);
    }
    const ws2812_profile_t *p = ws2812_get_profile(profile);
    printf("[ws2812] set profile=%u loops=%u/%u/%u/%u on=%u rgb=%u,%u,%u brightness=%u\r\n",
        (unsigned int)profile, (unsigned int)p->t0h, (unsigned int)p->t0l,
        (unsigned int)p->t1h, (unsigned int)p->t1l,
        g_ws2812_color.on ? 1 : 0, (unsigned int)r, (unsigned int)g,
        (unsigned int)b, (unsigned int)brightness);
    return 0;
}

int ws2812_set_color(uint8_t r, uint8_t g, uint8_t b, uint8_t brightness)
{
    return ws2812_set_color_profile(r, g, b, brightness, 0);
}

int ws2812_set_color_control(uint8_t r, uint8_t g, uint8_t b, uint8_t brightness)
{
    int ret = ws2812_set_color_profile(r, g, b, brightness,
        (uint8_t)XH_WS2812_CONTROL_PROFILE);
    printf("[WS63-LIGHT] control fixed profile=%u applied rgb=%u,%u,%u bright=%u ret=%d\r\n",
        (unsigned int)XH_WS2812_CONTROL_PROFILE,
        (unsigned int)r, (unsigned int)g, (unsigned int)b,
        (unsigned int)brightness, ret);
    return ret;
}

int ws2812_off_profile(uint8_t profile)
{
    if (!g_ws2812_inited) {
        int init_ret = ws2812_init();
        if (init_ret != 0) {
            return init_ret;
        }
    }
    g_ws2812_color.on = false;
    g_ws2812_color.r = 0;
    g_ws2812_color.g = 0;
    g_ws2812_color.b = 0;
    g_ws2812_color.brightness = 0;
    ws2812_write_scaled_profile(0, 0, 0, 0, profile);
    const ws2812_profile_t *p = ws2812_get_profile(profile);
    printf("[ws2812] off profile=%u loops=%u/%u/%u/%u\r\n",
        (unsigned int)profile, (unsigned int)p->t0h, (unsigned int)p->t0l,
        (unsigned int)p->t1h, (unsigned int)p->t1l);
    return 0;
}

int ws2812_off(void)
{
    return ws2812_off_profile(0);
}

int ws2812_off_control(void)
{
    int ret = ws2812_off_profile((uint8_t)XH_WS2812_CONTROL_PROFILE);
    printf("[WS63-LIGHT] control fixed profile=%u off applied ret=%d\r\n",
        (unsigned int)XH_WS2812_CONTROL_PROFILE, ret);
    return ret;
}

ws2812_color_t ws2812_get_color(void)
{
    return g_ws2812_color;
}
