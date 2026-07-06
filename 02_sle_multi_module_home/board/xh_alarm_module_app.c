#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "cmsis_os2.h"
#include "soc_osal.h"

#include "alarm_gpio.h"
#include "xh_module_ids.h"
#include "xh_sle_module_server.h"
#include "xh_sle_proto.h"

#define XH_ALARM_STACK_SIZE 0x2800
#define XH_ALARM_PRIO 25

#ifndef XH_ALARM_TICK_MS
#define XH_ALARM_TICK_MS 100
#endif

#ifndef XH_ALARM_MAX_ON_MS
#define XH_ALARM_MAX_ON_MS 60000
#endif

#ifndef XH_ALARM_SHORT_BEEP_MS
#define XH_ALARM_SHORT_BEEP_MS 500
#endif

typedef enum {
    XH_ALARM_MODE_OFF = 0,
    XH_ALARM_MODE_LED_ON = 1,
    XH_ALARM_MODE_LED_BLINK = 2,
    XH_ALARM_MODE_BUZZER = 3,
    XH_ALARM_MODE_LED_BLINK_BUZZER = 4,
    XH_ALARM_MODE_SHORT_BEEP = 5,
} xh_alarm_mode_t;

static uint8_t g_alarm_mode;
static uint8_t g_alarm_commanded_mode;
static bool g_alarm_timeout_protected;
static uint32_t g_alarm_on_ms;
static uint32_t g_alarm_blink_ms;
static bool g_alarm_blink_on;
static bool g_alarm_report_pending;

static bool xh_alarm_mode_valid(uint8_t mode)
{
    return mode <= XH_ALARM_MODE_SHORT_BEEP;
}

static void xh_alarm_report(void)
{
    uint8_t frame[XH_PROTO_MAX_LEN] = {0};
    uint16_t len = XH_PROTO_HDR_LEN;
    uint8_t seq = xh_proto_next_seq();
    uint8_t output_mask = alarm_gpio_get_output_mask();
    uint8_t sw = (g_alarm_mode == XH_ALARM_MODE_OFF) ? 0 : 1;

    if (xh_proto_begin(frame, sizeof(frame), seq, XH_MODULE_ID_ALARM, XH_PROTO_MSG_REPORT) &&
        xh_proto_put_u8(frame, sizeof(frame), &len, XH_TLV_ONLINE, 1) &&
        xh_proto_put_u8(frame, sizeof(frame), &len, XH_TLV_SWITCH, sw) &&
        xh_proto_put_u8(frame, sizeof(frame), &len, XH_TLV_ALARM_MODE, g_alarm_mode) &&
        xh_proto_put_u8(frame, sizeof(frame), &len, XH_TLV_GPIO_OUTPUT, output_mask) &&
        xh_proto_put_u8(frame, sizeof(frame), &len, XH_TLV_TIMEOUT_PROTECTED,
            g_alarm_timeout_protected ? 1 : 0) &&
        xh_proto_finish(frame, sizeof(frame), &len)) {
        printf("[WS63-ALARM] report sw=%u mode=%u gpio=0x%02X timeout=%u seq=%u\r\n",
            (unsigned int)sw, (unsigned int)g_alarm_mode,
            (unsigned int)output_mask, g_alarm_timeout_protected ? 1 : 0,
            (unsigned int)seq);
        (void)xh_sle_module_server_report(frame, len);
    }
}

static void xh_alarm_apply_outputs(void)
{
    bool led = false;
    bool buzzer = false;
    bool buzzer_wave = false;

    switch (g_alarm_mode) {
    case XH_ALARM_MODE_LED_ON:
        led = true;
        break;
    case XH_ALARM_MODE_LED_BLINK:
        led = g_alarm_blink_on;
        break;
    case XH_ALARM_MODE_BUZZER:
        buzzer = true;
        buzzer_wave = true;
        break;
    case XH_ALARM_MODE_LED_BLINK_BUZZER:
        led = g_alarm_blink_on;
        buzzer = true;
        buzzer_wave = true;
        break;
    case XH_ALARM_MODE_SHORT_BEEP:
        buzzer = true;
        buzzer_wave = true;
        break;
    case XH_ALARM_MODE_OFF:
    default:
        break;
    }

    printf("[WS63-ALARM] apply mode=%u led=%u buzzer=%u drive=%s\r\n",
        (unsigned int)g_alarm_mode, led ? 1 : 0, buzzer ? 1 : 0,
        buzzer_wave ? "wave" : "static");
    alarm_gpio_set_ex(led, buzzer, buzzer_wave);
}

static void xh_alarm_set_mode(uint8_t mode, bool timeout_protected)
{
    g_alarm_commanded_mode = mode;
    g_alarm_mode = mode;
    g_alarm_timeout_protected = timeout_protected;
    g_alarm_on_ms = 0;
    g_alarm_blink_ms = 0;
    g_alarm_blink_on = true;
    xh_alarm_apply_outputs();
    g_alarm_report_pending = true;
    printf("[xh_alarm_module] mode=%u timeout=%u\r\n",
        (unsigned int)g_alarm_mode, g_alarm_timeout_protected ? 1 : 0);
}

static void xh_alarm_control_cb(uint16_t conn_id, const uint8_t *data, uint16_t len)
{
    (void)conn_id;
    xh_proto_msg_t msg = {0};
    if (!xh_proto_decode(data, len, &msg) || msg.msg != XH_PROTO_MSG_CONTROL ||
        msg.module_id != XH_MODULE_ID_ALARM) {
        (void)xh_sle_module_server_ack(0, XH_MODULE_ID_ALARM, XH_ACK_INVALID);
        return;
    }

    uint8_t mode = 0;
    uint8_t sw = 1;
    bool have_mode = xh_proto_get_u8(&msg, XH_TLV_ALARM_MODE, &mode);
    bool have_switch = xh_proto_get_u8(&msg, XH_TLV_SWITCH, &sw);
    printf("[WS63-ALARM] rx ctrl seq=%u module=6 sw=%u sw_present=%u mode=%u mode_present=%u\r\n",
        (unsigned int)msg.seq, (unsigned int)sw, have_switch ? 1 : 0,
        (unsigned int)mode, have_mode ? 1 : 0);

    if (have_switch && sw == 0) {
        mode = XH_ALARM_MODE_OFF;
    } else if (!have_mode) {
        mode = XH_ALARM_MODE_LED_BLINK_BUZZER;
    }

    if (!xh_alarm_mode_valid(mode)) {
        printf("[WS63-ALARM] invalid mode=%u seq=%u\r\n",
            (unsigned int)mode, (unsigned int)msg.seq);
        (void)xh_sle_module_server_ack(msg.seq, XH_MODULE_ID_ALARM, XH_ACK_INVALID);
        return;
    }

    xh_alarm_set_mode(mode, false);
    (void)xh_sle_module_server_ack(msg.seq, XH_MODULE_ID_ALARM, XH_ACK_OK);
    xh_alarm_report();
    g_alarm_report_pending = false;
}

static void xh_alarm_task(void)
{
    printf("[xh_alarm_module] start build=%s %s tick=%u max_on=%u short=%u\r\n",
        __DATE__, __TIME__,
        (unsigned int)XH_ALARM_TICK_MS,
        (unsigned int)XH_ALARM_MAX_ON_MS,
        (unsigned int)XH_ALARM_SHORT_BEEP_MS);

    alarm_gpio_init();
    alarm_gpio_set(false, false);
    (void)xh_sle_module_server_init(XH_MODULE_ID_ALARM, xh_alarm_control_cb);
    xh_alarm_set_mode(XH_ALARM_MODE_OFF, false);
    xh_alarm_report();
    g_alarm_report_pending = false;

    while (1) {
        bool output_changed = false;
        if (g_alarm_mode != XH_ALARM_MODE_OFF) {
            g_alarm_on_ms += XH_ALARM_TICK_MS;
        }

        if (g_alarm_mode == XH_ALARM_MODE_LED_BLINK ||
            g_alarm_mode == XH_ALARM_MODE_LED_BLINK_BUZZER) {
            g_alarm_blink_ms += XH_ALARM_TICK_MS;
            if (g_alarm_blink_ms >= 500U) {
                g_alarm_blink_ms = 0;
                g_alarm_blink_on = !g_alarm_blink_on;
                output_changed = true;
            }
        }

        if (g_alarm_mode == XH_ALARM_MODE_SHORT_BEEP &&
            g_alarm_on_ms >= XH_ALARM_SHORT_BEEP_MS) {
            xh_alarm_set_mode(XH_ALARM_MODE_OFF, false);
        } else if (g_alarm_mode != XH_ALARM_MODE_OFF &&
            g_alarm_on_ms >= XH_ALARM_MAX_ON_MS) {
            printf("[xh_alarm_module] timeout auto off commanded=%u\r\n",
                (unsigned int)g_alarm_commanded_mode);
            xh_alarm_set_mode(XH_ALARM_MODE_OFF, true);
        } else if (output_changed) {
            xh_alarm_apply_outputs();
        }

        if (g_alarm_report_pending) {
            xh_alarm_report();
            g_alarm_report_pending = false;
        }

        osal_msleep(XH_ALARM_TICK_MS);
    }
}

void xh_alarm_module_app_start(void)
{
    osThreadAttr_t attr = {0};
    attr.name = "xh_alarm";
    attr.stack_size = XH_ALARM_STACK_SIZE;
    attr.priority = XH_ALARM_PRIO;
    if (osThreadNew((osThreadFunc_t)xh_alarm_task, NULL, &attr) == NULL) {
        printf("[xh_alarm_module] create task fail\r\n");
    }
}
