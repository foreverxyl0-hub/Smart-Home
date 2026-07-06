#include "xh_scene_rule.h"

#include <stdio.h>

#include "xh_fan_control.h"
#include "xh_module_ids.h"
#include "xh_sensor_state.h"
#include "xh_sle_hub.h"
#include "xh_sle_proto.h"

#ifndef XH_HOME_FAN_TRIGGER_TEMP100
#define XH_HOME_FAN_TRIGGER_TEMP100 3200
#endif

#ifndef XH_HOME_FAN_RELEASE_TEMP100
#define XH_HOME_FAN_RELEASE_TEMP100 3100
#endif

#ifndef XH_HOME_FAN_TRIGGER_HUMI100
#define XH_HOME_FAN_TRIGGER_HUMI100 5000
#endif

#ifndef XH_HOME_FAN_RELEASE_HUMI100
#define XH_HOME_FAN_RELEASE_HUMI100 4800
#endif

#ifndef XH_AWAY_ALARM_TRIGGER_TEMP100
#define XH_AWAY_ALARM_TRIGGER_TEMP100 3500
#endif

#ifndef XH_AWAY_ALARM_RELEASE_TEMP100
#define XH_AWAY_ALARM_RELEASE_TEMP100 3400
#endif

#ifndef XH_AWAY_ALARM_TRIGGER_HUMI100
#define XH_AWAY_ALARM_TRIGGER_HUMI100 6000
#endif

#ifndef XH_AWAY_ALARM_RELEASE_HUMI100
#define XH_AWAY_ALARM_RELEASE_HUMI100 5800
#endif

#define XH_SCENE_MODE_HOME 1U
#define XH_SCENE_MODE_AWAY 2U

#define XH_SCENE_CAUSE_TEMP_HIGH 0x01U
#define XH_SCENE_CAUSE_HUMI_HIGH 0x02U
#define XH_SCENE_CAUSE_ACT_FAIL  0x10U

#define XH_ALARM_MODE_OFF              0U
#define XH_ALARM_MODE_LED_BLINK_BUZZER 4U

#define XH_HOME_LIGHT_R          255U
#define XH_HOME_LIGHT_G          255U
#define XH_HOME_LIGHT_B          255U
#define XH_HOME_LIGHT_BRIGHTNESS 128U

static uint8_t g_scene_mode = XH_SCENE_MODE_HOME;
static uint8_t g_scene_cause;
static bool g_fan_auto_inhibit;
static bool g_away_alarm_on;

static void xh_scene_report(void)
{
    xh_sle_hub_send_scene_report(true);
}

static bool xh_scene_send_fan(bool on)
{
    if (xh_fan_control_set_on(on)) {
        return true;
    }

    g_scene_cause |= XH_SCENE_CAUSE_ACT_FAIL;
    printf("[xh_scene] fan %s failed\r\n", on ? "on" : "off");
    return false;
}

static bool xh_scene_send_alarm(uint8_t mode)
{
    if (xh_sle_hub_send_alarm_control(mode)) {
        return true;
    }

    g_scene_cause |= XH_SCENE_CAUSE_ACT_FAIL;
    printf("[xh_scene] alarm mode=%u failed\r\n", (unsigned int)mode);
    return false;
}

static bool xh_scene_send_light(bool on)
{
    uint8_t r = on ? XH_HOME_LIGHT_R : 0U;
    uint8_t g = on ? XH_HOME_LIGHT_G : 0U;
    uint8_t b = on ? XH_HOME_LIGHT_B : 0U;
    uint8_t brightness = on ? XH_HOME_LIGHT_BRIGHTNESS : 0U;

    if (xh_sle_hub_send_light_control(on, r, g, b, brightness)) {
        return true;
    }

    g_scene_cause |= XH_SCENE_CAUSE_ACT_FAIL;
    printf("[xh_scene] light %s failed\r\n", on ? "on" : "off");
    return false;
}

static void xh_scene_apply_home_rule(const xh_sht30_state_t *th)
{
    xh_fan_state_t fan;
    bool fan_valid = xh_sensor_state_get_fan(&fan);
    bool trigger = th->temp100 >= XH_HOME_FAN_TRIGGER_TEMP100 ||
                   th->humi100 > XH_HOME_FAN_TRIGGER_HUMI100;
    bool release = th->temp100 <= XH_HOME_FAN_RELEASE_TEMP100 &&
                   th->humi100 < XH_HOME_FAN_RELEASE_HUMI100;

    if (release) {
        if (g_fan_auto_inhibit) {
            printf("[xh_scene] home fan manual inhibit cleared temp=%ld humi=%ld\r\n",
                (long)th->temp100, (long)th->humi100);
        }
        g_fan_auto_inhibit = false;
        if (fan_valid && fan.on) {
            printf("[xh_scene] home release fan off temp=%ld humi=%ld\r\n",
                (long)th->temp100, (long)th->humi100);
            (void)xh_scene_send_fan(false);
        }
        return;
    }

    if (!trigger || g_fan_auto_inhibit) {
        return;
    }

    if (!fan_valid || !fan.on) {
        printf("[xh_scene] home trigger fan on temp=%ld humi=%ld\r\n",
            (long)th->temp100, (long)th->humi100);
        (void)xh_scene_send_fan(true);
    }
}

static void xh_scene_apply_away_rule(const xh_sht30_state_t *th)
{
    bool temp_high = th->temp100 >= XH_AWAY_ALARM_TRIGGER_TEMP100;
    bool humi_high = th->humi100 > XH_AWAY_ALARM_TRIGGER_HUMI100;
    bool release = th->temp100 <= XH_AWAY_ALARM_RELEASE_TEMP100 &&
                   th->humi100 < XH_AWAY_ALARM_RELEASE_HUMI100;
    uint8_t new_cause = g_scene_cause & XH_SCENE_CAUSE_ACT_FAIL;

    if (temp_high) {
        new_cause |= XH_SCENE_CAUSE_TEMP_HIGH;
    }
    if (humi_high) {
        new_cause |= XH_SCENE_CAUSE_HUMI_HIGH;
    }

    if ((temp_high || humi_high) && !g_away_alarm_on) {
        printf("[xh_scene] away trigger alarm temp=%ld humi=%ld\r\n",
            (long)th->temp100, (long)th->humi100);
        g_scene_cause = new_cause;
        g_away_alarm_on = xh_scene_send_alarm(XH_ALARM_MODE_LED_BLINK_BUZZER);
        xh_scene_report();
        return;
    }

    if (release && g_away_alarm_on) {
        printf("[xh_scene] away release alarm temp=%ld humi=%ld\r\n",
            (long)th->temp100, (long)th->humi100);
        if (xh_scene_send_alarm(XH_ALARM_MODE_OFF)) {
            g_away_alarm_on = false;
        }
        g_scene_cause &= XH_SCENE_CAUSE_ACT_FAIL;
        xh_scene_report();
        return;
    }

    if (g_away_alarm_on) {
        g_scene_cause = new_cause;
    }
}

void xh_scene_rule_init(void)
{
    g_scene_mode = XH_SCENE_MODE_HOME;
    g_scene_cause = 0U;
    g_fan_auto_inhibit = false;
    g_away_alarm_on = false;

    printf("[xh_scene] init HOME fan T=%d/%d H=%d/%d away T=%d/%d H=%d/%d\r\n",
        XH_HOME_FAN_TRIGGER_TEMP100, XH_HOME_FAN_RELEASE_TEMP100,
        XH_HOME_FAN_TRIGGER_HUMI100, XH_HOME_FAN_RELEASE_HUMI100,
        XH_AWAY_ALARM_TRIGGER_TEMP100, XH_AWAY_ALARM_RELEASE_TEMP100,
        XH_AWAY_ALARM_TRIGGER_HUMI100, XH_AWAY_ALARM_RELEASE_HUMI100);
}

bool xh_scene_rule_set_mode(uint8_t mode, const char *source)
{
    const char *src = (source != NULL) ? source : "unknown";

    if (mode != XH_SCENE_MODE_HOME && mode != XH_SCENE_MODE_AWAY) {
        printf("[xh_scene] invalid mode=%u source=%s\r\n",
            (unsigned int)mode, src);
        return false;
    }

    printf("[xh_scene] set mode %u->%u source=%s\r\n",
        (unsigned int)g_scene_mode, (unsigned int)mode, src);

    g_scene_mode = mode;
    g_scene_cause = 0U;
    g_fan_auto_inhibit = false;
    g_away_alarm_on = false;

    if (mode == XH_SCENE_MODE_HOME) {
        (void)xh_scene_send_light(true);
        (void)xh_scene_send_alarm(XH_ALARM_MODE_OFF);
    } else {
        (void)xh_scene_send_light(false);
        (void)xh_scene_send_fan(false);
        (void)xh_scene_send_alarm(XH_ALARM_MODE_OFF);
    }

    xh_scene_rule_on_sensor_update(XH_MODULE_ID_SHT30);
    xh_scene_report();
    return true;
}

uint8_t xh_scene_rule_get_mode(void)
{
    return g_scene_mode;
}

uint8_t xh_scene_rule_get_cause(void)
{
    return g_scene_cause;
}

uint16_t xh_scene_rule_pack_report(uint8_t *out, uint16_t cap)
{
    uint16_t len = XH_PROTO_HDR_LEN;
    uint8_t flags = g_away_alarm_on ? 1U : 0U;

    if (out == NULL || cap < XH_PROTO_HDR_LEN) {
        return 0U;
    }

    if (!xh_proto_begin(out, cap, xh_proto_next_seq(),
            XH_MODULE_ID_HUB_SCENE, XH_PROTO_MSG_REPORT) ||
        !xh_proto_put_u8(out, cap, &len, XH_TLV_ONLINE, 1U) ||
        !xh_proto_put_u8(out, cap, &len, XH_TLV_SCENE_MODE, g_scene_mode) ||
        !xh_proto_put_u8(out, cap, &len, XH_TLV_SCENE_CAUSE, g_scene_cause) ||
        !xh_proto_put_u8(out, cap, &len, XH_TLV_SCENE_FLAGS, flags) ||
        !xh_proto_finish(out, cap, &len)) {
        return 0U;
    }
    return len;
}

void xh_scene_rule_on_manual_fan_control(bool on, const char *source)
{
    xh_sht30_state_t th;
    const char *src = (source != NULL) ? source : "unknown";

    if (g_scene_mode != XH_SCENE_MODE_HOME || on) {
        if (on) {
            g_fan_auto_inhibit = false;
        }
        return;
    }

    if (!xh_sensor_state_get_sht30(&th)) {
        return;
    }

    if (th.temp100 >= XH_HOME_FAN_TRIGGER_TEMP100 ||
        th.humi100 > XH_HOME_FAN_TRIGGER_HUMI100) {
        g_fan_auto_inhibit = true;
        printf("[xh_scene] home manual fan off inhibits auto source=%s temp=%ld humi=%ld\r\n",
            src, (long)th.temp100, (long)th.humi100);
    }
}

void xh_scene_rule_on_fan_report_transition(bool was_on, bool is_on, const char *source)
{
    if (g_scene_mode == XH_SCENE_MODE_HOME && was_on && !is_on) {
        xh_scene_rule_on_manual_fan_control(false, source);
    }
}

void xh_scene_rule_on_sensor_update(uint8_t module_id)
{
    xh_sht30_state_t th;

    if (module_id != XH_MODULE_ID_SHT30 ||
        !xh_sensor_state_get_sht30(&th)) {
        return;
    }

    if (g_scene_mode == XH_SCENE_MODE_HOME) {
        xh_scene_apply_home_rule(&th);
    } else {
        xh_scene_apply_away_rule(&th);
    }
}

void xh_scene_rule_tick(void)
{
    /* Rules run when a fresh SHT30 report arrives. */
}
