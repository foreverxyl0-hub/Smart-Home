#include "xh_sle_hub.h"

#include <stdio.h>

#include "errcode.h"
#include "soc_osal.h"
#include "uart_tx_data_down.h"
#include "xh_module_ids.h"
#include "xh_scene_rule.h"
#include "xh_sensor_state.h"
#include "xh_sle_client.h"
#include "xh_sle_proto.h"

#ifndef XH_FAN_TRIGGER_TEMP100
#define XH_FAN_TRIGGER_TEMP100 3300
#endif

typedef struct {
    bool active;
    uint8_t seq;
    uint8_t module_id;
    uint8_t age_s;
    bool fallback_tried;
    uint8_t on;
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t brightness;
} xh_light_pending_t;

static xh_light_pending_t g_light_pending;
static uint8_t g_sht30_poll_age_s;

static void xh_hub_log_module_diag(uint8_t module_id, const char *tag)
{
    sle_client_module_diag_t diag;
    const char *t = (tag != NULL) ? tag : "diag";

    if (!sle_client_get_module_diag(module_id, &diag) || diag.found == 0) {
        printf("[WS63-BIND] %s module=%u offline\r\n", t, module_id);
        return;
    }

    printf("[WS63-BIND] %s module=%u slot=%u conn_id=0x%02X value=0x%04X ccc=0x%04X ready=%u ntf=%u addr=%02X:%02X:%02X:%02X:%02X:%02X\r\n",
        t, module_id, diag.slot, diag.conn_id, diag.property_handle, diag.ccc_handle,
        diag.property_ready, diag.ntf_subscribed,
        diag.addr[0], diag.addr[1], diag.addr[2],
        diag.addr[3], diag.addr[4], diag.addr[5]);
}

static bool xh_sle_hub_send_control(uint8_t module_id, const uint8_t *payload, uint16_t len)
{
    xh_hub_log_module_diag(module_id, "send-target");
    errcode_t ret = sle_client_send_msg_to_module(module_id, payload, len);
    printf("[WS63-HUB] send control module=%u len=%u ret=0x%X\r\n",
        module_id, (unsigned int)len, (unsigned int)ret);
    return (ret == ERRCODE_SUCC || ret == (errcode_t)ERRCODE_SLE_SUCCESS);
}

static void xh_hub_enqueue_p4_frame(uint16_t cmd, const uint8_t *payload, uint16_t len, bool urgent)
{
    (void)set_pending_sensor_frame(cmd, payload, len, urgent);
}

static void xh_hub_enqueue_p4_ack_cmd(uint16_t cmd, uint8_t seq, uint8_t module_id, uint16_t err)
{
    uint8_t payload[XH_PROTO_MAX_LEN] = {0};
    uint16_t len = 0;
    if (xh_proto_pack_ack(payload, sizeof(payload), &len, seq, module_id, err)) {
        xh_hub_enqueue_p4_frame(cmd, payload, len, true);
    }
}

static void xh_hub_enqueue_p4_ack(uint8_t seq, uint8_t module_id, uint16_t err)
{
    xh_hub_enqueue_p4_ack_cmd(0x0F12, seq, module_id, err);
}

static void xh_hub_enqueue_module_report(const uint8_t *payload, uint16_t len)
{
    xh_hub_enqueue_p4_frame(0x0F10, payload, len, false);
}

static void xh_hub_enqueue_module_report_urgent(const uint8_t *payload, uint16_t len, bool urgent)
{
    xh_hub_enqueue_p4_frame(0x0F10, payload, len, urgent);
}

static void xh_hub_light_pending_set(uint8_t seq, uint8_t on, const uint8_t *rgb)
{
    g_light_pending.active = true;
    g_light_pending.seq = seq;
    g_light_pending.module_id = XH_MODULE_ID_LIGHT;
    g_light_pending.age_s = 0;
    g_light_pending.fallback_tried = false;
    g_light_pending.on = on;
    g_light_pending.r = rgb[0];
    g_light_pending.g = rgb[1];
    g_light_pending.b = rgb[2];
    g_light_pending.brightness = rgb[3];
}

static void xh_hub_light_pending_clear(const char *reason)
{
    if (!g_light_pending.active) {
        return;
    }
    printf("[WS63-LIGHT-HUB] pending clear seq=%u reason=%s target on=%u rgb=%u,%u,%u bright=%u\r\n",
        (unsigned int)g_light_pending.seq, (reason != NULL) ? reason : "unknown",
        g_light_pending.on, g_light_pending.r, g_light_pending.g,
        g_light_pending.b, g_light_pending.brightness);
    g_light_pending.active = false;
}

static bool xh_hub_light_report_matches_pending(uint8_t sw, const uint8_t *rgb)
{
    if (!g_light_pending.active || rgb == NULL) {
        return false;
    }
    if ((sw != 0) != (g_light_pending.on != 0)) {
        return false;
    }
    if (g_light_pending.on == 0) {
        return true;
    }
    return rgb[0] == g_light_pending.r &&
           rgb[1] == g_light_pending.g &&
           rgb[2] == g_light_pending.b &&
           rgb[3] == g_light_pending.brightness;
}

bool xh_sle_hub_send_fan_control(bool on)
{
    uint8_t payload[XH_PROTO_MAX_LEN] = {0};
    uint16_t len = XH_PROTO_HDR_LEN;
    uint8_t seq = xh_proto_next_seq();
    if (!xh_proto_begin(payload, sizeof(payload), seq, XH_MODULE_ID_FAN, XH_PROTO_MSG_CONTROL) ||
        !xh_proto_put_u8(payload, sizeof(payload), &len, XH_TLV_SWITCH, on ? 1 : 0) ||
        !xh_proto_put_u8(payload, sizeof(payload), &len, XH_TLV_FAN_LEVEL, on ? 1 : 0) ||
        !xh_proto_finish(payload, sizeof(payload), &len)) {
        return false;
    }

    xh_sensor_state_set_fan_commanded(on);
    return xh_sle_hub_send_control(XH_MODULE_ID_FAN, payload, len);
}

bool xh_sle_hub_send_light_control(bool on, uint8_t r, uint8_t g,
                                   uint8_t b, uint8_t brightness)
{
    uint8_t payload[XH_PROTO_MAX_LEN] = {0};
    uint8_t rgb[4] = {r, g, b, brightness};
    uint16_t len = XH_PROTO_HDR_LEN;
    uint8_t seq = xh_proto_next_seq();

    if (!xh_proto_begin(payload, sizeof(payload), seq, XH_MODULE_ID_LIGHT,
            XH_PROTO_MSG_CONTROL) ||
        !xh_proto_put_u8(payload, sizeof(payload), &len, XH_TLV_SWITCH, on ? 1 : 0) ||
        !xh_proto_put_bytes(payload, sizeof(payload), &len, XH_TLV_RGB, rgb, sizeof(rgb)) ||
        !xh_proto_finish(payload, sizeof(payload), &len)) {
        return false;
    }

    printf("[WS63-LIGHT-HUB] scene ctrl seq=%u on=%u rgb=%u,%u,%u bright=%u\r\n",
        (unsigned int)seq, on ? 1 : 0, r, g, b, brightness);
    xh_sensor_state_set_light_commanded(on, r, g, b, brightness);
    xh_hub_light_pending_set(seq, on ? 1 : 0, rgb);
    bool ok = xh_sle_hub_send_control(XH_MODULE_ID_LIGHT, payload, len);
    if (!ok) {
        xh_hub_light_pending_clear("scene-send-fail");
    }
    return ok;
}

bool xh_sle_hub_send_alarm_control(uint8_t mode)
{
    uint8_t payload[XH_PROTO_MAX_LEN] = {0};
    uint16_t len = XH_PROTO_HDR_LEN;
    uint8_t seq = xh_proto_next_seq();
    uint8_t sw = (mode == 0) ? 0 : 1;

    if (!xh_proto_begin(payload, sizeof(payload), seq, XH_MODULE_ID_ALARM,
            XH_PROTO_MSG_CONTROL) ||
        !xh_proto_put_u8(payload, sizeof(payload), &len, XH_TLV_SWITCH, sw) ||
        !xh_proto_put_u8(payload, sizeof(payload), &len, XH_TLV_ALARM_MODE, mode) ||
        !xh_proto_finish(payload, sizeof(payload), &len)) {
        return false;
    }

    printf("[WS63-ALARM-HUB] scene ctrl seq=%u switch=%u mode=%u\r\n",
        (unsigned int)seq, sw, (unsigned int)mode);
    return xh_sle_hub_send_control(XH_MODULE_ID_ALARM, payload, len);
}

void xh_sle_hub_send_scene_report(bool urgent)
{
    uint8_t out[XH_PROTO_MAX_LEN] = {0};
    uint16_t len = xh_scene_rule_pack_report(out, sizeof(out));
    if (len > 0) {
        xh_hub_enqueue_module_report_urgent(out, len, urgent);
    }
}

static void xh_hub_handle_legacy_th(const uint8_t *data, uint16_t len)
{
    if (data == NULL || len < 8) {
        return;
    }
    int32_t temp100 = (int32_t)((uint32_t)data[0] |
                                ((uint32_t)data[1] << 8) |
                                ((uint32_t)data[2] << 16) |
                                ((uint32_t)data[3] << 24));
    int32_t humi100 = (int32_t)((uint32_t)data[4] |
                                ((uint32_t)data[5] << 8) |
                                ((uint32_t)data[6] << 16) |
                                ((uint32_t)data[7] << 24));
    xh_sensor_state_update_sht30(temp100, humi100);
    set_pending_sensor_th_data(temp100, humi100);
    xh_scene_rule_on_sensor_update(XH_MODULE_ID_SHT30);
}

static void xh_hub_handle_report(const xh_proto_msg_t *msg, const uint8_t *raw, uint16_t raw_len)
{
    int32_t temp100 = 0;
    int32_t humi100 = 0;
    uint8_t valid = 1;
    uint8_t warmup = 0;
    uint8_t smoke_alarm = 0;
    uint32_t raw_value = 0;
    uint8_t adc_valid = 0;
    uint32_t adc_raw = 0;
    uint32_t adc_avg = 0;
    uint16_t adc_error = 1;
    uint8_t sw = 0;
    uint8_t level = 0;
    uint8_t present = 0;
    int32_t age_ms = 0;
    uint32_t lux100 = 0;
    uint16_t error_code = 0;
    uint8_t mode = 0;
    uint8_t gpio_output = 0;
    uint8_t timeout_protected = 0;
    const uint8_t *rgb = NULL;
    uint8_t rgb_len = 0;

    if (msg->module_id == XH_MODULE_ID_SHT30 &&
        xh_proto_get_i32(msg, XH_TLV_TEMP100, &temp100) &&
        xh_proto_get_i32(msg, XH_TLV_HUMI100, &humi100)) {
        xh_sensor_state_update_sht30_seq(temp100, humi100, msg->seq);
        set_pending_sensor_th_data(temp100, humi100);
        xh_hub_enqueue_module_report(raw, raw_len);
        xh_scene_rule_on_sensor_update(XH_MODULE_ID_SHT30);
        return;
    }

    if (msg->module_id == XH_MODULE_ID_MQ2 &&
        xh_proto_get_u8(msg, XH_TLV_SMOKE_ALARM, &smoke_alarm)) {
        if (!xh_proto_get_u8(msg, XH_TLV_VALID, &valid)) {
            valid = 1;
        }
        if (!xh_proto_get_u8(msg, XH_TLV_WARMUP, &warmup)) {
            warmup = 0;
        }
        if (!xh_proto_get_u32(msg, XH_TLV_RAW_VALUE, &raw_value)) {
            raw_value = smoke_alarm;
        }
        if (!xh_proto_get_u8(msg, XH_TLV_ADC_VALID, &adc_valid)) {
            adc_valid = 0;
        }
        if (!xh_proto_get_u32(msg, XH_TLV_ADC_RAW, &adc_raw)) {
            adc_raw = 0;
        }
        if (!xh_proto_get_u32(msg, XH_TLV_ADC_AVG, &adc_avg)) {
            adc_avg = adc_raw;
        }
        if (!xh_proto_get_u16(msg, XH_TLV_ADC_ERROR, &adc_error)) {
            adc_error = adc_valid ? 0 : 1;
        }
        xh_sensor_state_update_mq2_adc(adc_valid != 0, adc_raw, adc_avg, adc_error);
        xh_sensor_state_update_mq2_seq(smoke_alarm != 0, valid != 0,
            warmup != 0, raw_value, msg->seq);
        xh_hub_enqueue_module_report_urgent(raw, raw_len, (valid != 0 && smoke_alarm != 0));
        xh_scene_rule_on_sensor_update(XH_MODULE_ID_MQ2);
        return;
    }

    if (msg->module_id == XH_MODULE_ID_FAN &&
        xh_proto_get_u8(msg, XH_TLV_SWITCH, &sw)) {
        if (!xh_proto_get_u8(msg, XH_TLV_FAN_LEVEL, &level)) {
            level = sw ? 1 : 0;
        }
        xh_fan_state_t old_fan;
        bool old_valid = xh_sensor_state_get_fan(&old_fan);
        xh_sensor_state_update_fan_seq(sw != 0, level, msg->seq);
        if (old_valid) {
            xh_scene_rule_on_fan_report_transition(old_fan.on, sw != 0, "fan_report");
        }
        xh_hub_enqueue_module_report(raw, raw_len);
        return;
    }

    if (msg->module_id == XH_MODULE_ID_LD2401 &&
        xh_proto_get_u8(msg, XH_TLV_PRESENCE, &present)) {
        if (!xh_proto_get_i32(msg, XH_TLV_AGE_MS, &age_ms)) {
            age_ms = 0;
        }
        xh_sensor_state_update_presence_seq(present != 0, (uint32_t)age_ms, msg->seq);
        xh_hub_enqueue_module_report(raw, raw_len);
        xh_scene_rule_on_sensor_update(XH_MODULE_ID_LD2401);
        return;
    }

    if (msg->module_id == XH_MODULE_ID_BH1750 &&
        xh_proto_get_u32(msg, XH_TLV_LUX100, &lux100)) {
        if (!xh_proto_get_u16(msg, XH_TLV_ERROR_CODE, &error_code)) {
            error_code = 0;
        }
        xh_sensor_state_update_bh1750_seq(lux100, error_code, msg->seq);
        xh_hub_enqueue_module_report(raw, raw_len);
        return;
    }

    if (msg->module_id == XH_MODULE_ID_ALARM &&
        xh_proto_get_u8(msg, XH_TLV_ALARM_MODE, &mode)) {
        if (!xh_proto_get_u8(msg, XH_TLV_SWITCH, &sw)) {
            sw = (mode == 0) ? 0 : 1;
        }
        if (!xh_proto_get_u8(msg, XH_TLV_GPIO_OUTPUT, &gpio_output)) {
            gpio_output = 0;
        }
        if (!xh_proto_get_u8(msg, XH_TLV_TIMEOUT_PROTECTED, &timeout_protected)) {
            timeout_protected = 0;
        }
        xh_sensor_state_update_alarm_seq(sw != 0, mode, gpio_output,
            timeout_protected != 0, msg->seq);
        xh_hub_enqueue_module_report(raw, raw_len);
        return;
    }

    if (msg->module_id == XH_MODULE_ID_LIGHT &&
        xh_proto_get_u8(msg, XH_TLV_SWITCH, &sw) &&
        xh_proto_get_bytes(msg, XH_TLV_RGB, &rgb, &rgb_len) &&
        rgb_len >= 4) {
        if (!xh_proto_get_u8(msg, XH_TLV_GPIO_OUTPUT, &gpio_output)) {
            gpio_output = sw;
        }
        xh_sensor_state_update_light_seq(sw != 0, rgb[0], rgb[1], rgb[2], rgb[3],
            gpio_output, msg->seq);
        printf("[WS63-LIGHT-HUB] module report on=%u rgb=%u,%u,%u bright=%u gpio=%u seq=%u\r\n",
            sw != 0, rgb[0], rgb[1], rgb[2], rgb[3], gpio_output, msg->seq);
        if (xh_hub_light_report_matches_pending(sw, rgb)) {
            xh_hub_light_pending_clear("report-match");
        } else if (g_light_pending.active) {
            printf("[WS63-LIGHT-HUB] report mismatch keep pending target on=%u rgb=%u,%u,%u bright=%u\r\n",
                g_light_pending.on, g_light_pending.r, g_light_pending.g,
                g_light_pending.b, g_light_pending.brightness);
        }
        xh_hub_enqueue_module_report(raw, raw_len);
        return;
    }
}

int sle_client_recv_data(uint16_t conn_id, uint8_t *data, uint16_t len)
{
    (void)conn_id;
    if (data == NULL || len == 0) {
        return 0;
    }

    xh_proto_msg_t msg = {0};
    if (!xh_proto_decode(data, len, &msg)) {
        xh_hub_handle_legacy_th(data, len);
        return len;
    }

    printf("[xh_sle_hub] recv module=0x%02X msg=0x%02X seq=%u len=%u\r\n",
        msg.module_id, msg.msg, (unsigned int)msg.seq, (unsigned int)len);
    if (msg.msg == XH_PROTO_MSG_REPORT || msg.msg == XH_PROTO_MSG_HELLO ||
        msg.msg == XH_PROTO_MSG_HEARTBEAT) {
        xh_hub_handle_report(&msg, data, len);
    } else if (msg.msg == XH_PROTO_MSG_ACK) {
        if (msg.module_id == XH_MODULE_ID_LIGHT) {
            uint16_t err = 0xFFFF;
            (void)xh_proto_get_u16(&msg, XH_TLV_ERROR_CODE, &err);
            printf("[WS63-LIGHT-HUB] module ack seq=%u err=%u\r\n",
                (unsigned int)msg.seq, (unsigned int)err);
            xh_hub_light_pending_clear("ack");
        }
        xh_hub_enqueue_p4_frame(0x0F12, data, len, true);
    }
    return len;
}

void xh_sensor_uart_handle_query(const uint8_t *payload, uint32_t payload_len)
{
    (void)payload;
    (void)payload_len;
    uint8_t out[XH_PROTO_MAX_LEN] = {0};
    uint16_t len = xh_sensor_state_pack_sht30_snapshot(out, sizeof(out));
    if (len > 0) {
        xh_hub_enqueue_p4_frame(0x0F10, out, len, true);
    }
    len = xh_sensor_state_pack_fan_snapshot(out, sizeof(out));
    if (len > 0) {
        xh_hub_enqueue_p4_frame(0x0F10, out, len, true);
    }
    len = xh_sensor_state_pack_mq2_snapshot(out, sizeof(out));
    if (len > 0) {
        xh_hub_enqueue_p4_frame(0x0F10, out, len, true);
    }
    len = xh_sensor_state_pack_presence_snapshot(out, sizeof(out));
    if (len > 0) {
        xh_hub_enqueue_p4_frame(0x0F10, out, len, true);
    }
    len = xh_sensor_state_pack_bh1750_snapshot(out, sizeof(out));
    if (len > 0) {
        xh_hub_enqueue_p4_frame(0x0F10, out, len, true);
    }
    len = xh_sensor_state_pack_alarm_snapshot(out, sizeof(out));
    if (len > 0) {
        xh_hub_enqueue_p4_frame(0x0F10, out, len, true);
    }
    len = xh_sensor_state_pack_light_snapshot(out, sizeof(out));
    if (len > 0) {
        xh_hub_enqueue_p4_frame(0x0F10, out, len, true);
    }
    len = xh_scene_rule_pack_report(out, sizeof(out));
    if (len > 0) {
        xh_hub_enqueue_p4_frame(0x0F10, out, len, true);
    }
}

void xh_sensor_uart_handle_control(const uint8_t *payload, uint32_t payload_len)
{
    if (payload == NULL || payload_len == 0 || payload_len > 0xFFFFU) {
        xh_hub_enqueue_p4_ack(0, 0, XH_ACK_INVALID);
        return;
    }

    uint8_t forward_payload[XH_PROTO_MAX_LEN] = {0};
    uint16_t forward_len = (uint16_t)payload_len;
    const uint8_t *send_payload = payload;

    xh_proto_msg_t msg = {0};
    if (!xh_proto_decode(payload, (uint16_t)payload_len, &msg) ||
        msg.msg != XH_PROTO_MSG_CONTROL) {
        xh_hub_enqueue_p4_ack(0, 0, XH_ACK_INVALID);
        return;
    }

    if (msg.module_id != XH_MODULE_ID_FAN &&
        msg.module_id != XH_MODULE_ID_ALARM &&
        msg.module_id != XH_MODULE_ID_LIGHT) {
        xh_hub_enqueue_p4_ack(msg.seq, msg.module_id, XH_ACK_UNSUPPORTED);
        return;
    }

    if (msg.module_id == XH_MODULE_ID_FAN) {
        uint8_t sw = 0;
        if (!xh_proto_get_u8(&msg, XH_TLV_SWITCH, &sw)) {
            xh_hub_enqueue_p4_ack(msg.seq, msg.module_id, XH_ACK_INVALID);
            return;
        }

        xh_scene_rule_on_manual_fan_control(sw != 0, "p4");
        xh_sensor_state_set_fan_commanded(sw != 0);
    } else if (msg.module_id == XH_MODULE_ID_ALARM) {
        uint8_t mode = 0;
        uint8_t sw = 1;
        bool have_mode = xh_proto_get_u8(&msg, XH_TLV_ALARM_MODE, &mode);
        bool have_switch = xh_proto_get_u8(&msg, XH_TLV_SWITCH, &sw);
        if ((!have_mode && !have_switch) || mode > 5) {
            xh_hub_enqueue_p4_ack(msg.seq, msg.module_id, XH_ACK_INVALID);
            return;
        }
    } else if (msg.module_id == XH_MODULE_ID_LIGHT) {
        uint8_t sw = 0;
        const uint8_t *rgb = NULL;
        uint8_t rgb_len = 0;
        uint8_t rgb_value[4] = {0, 0, 0, 0};
        if (!xh_proto_get_u8(&msg, XH_TLV_SWITCH, &sw)) {
            xh_hub_enqueue_p4_ack(msg.seq, msg.module_id, XH_ACK_INVALID);
            return;
        }
        if (xh_proto_get_bytes(&msg, XH_TLV_RGB, &rgb, &rgb_len)) {
            if (rgb_len < 4) {
                xh_hub_enqueue_p4_ack(msg.seq, msg.module_id, XH_ACK_INVALID);
                return;
            }
            rgb_value[0] = rgb[0];
            rgb_value[1] = rgb[1];
            rgb_value[2] = rgb[2];
            rgb_value[3] = rgb[3];
        } else if (sw != 0) {
            xh_hub_enqueue_p4_ack(msg.seq, msg.module_id, XH_ACK_INVALID);
            return;
        } else {
            forward_len = XH_PROTO_HDR_LEN;
            if (!xh_proto_begin(forward_payload, sizeof(forward_payload), msg.seq,
                    XH_MODULE_ID_LIGHT, XH_PROTO_MSG_CONTROL) ||
                !xh_proto_put_u8(forward_payload, sizeof(forward_payload), &forward_len,
                    XH_TLV_SWITCH, 0) ||
                !xh_proto_put_bytes(forward_payload, sizeof(forward_payload), &forward_len,
                    XH_TLV_RGB, rgb_value, sizeof(rgb_value)) ||
                !xh_proto_finish(forward_payload, sizeof(forward_payload), &forward_len)) {
                xh_hub_enqueue_p4_ack(msg.seq, msg.module_id, XH_ACK_INVALID);
                return;
            }
            send_payload = forward_payload;
            printf("[WS63-LIGHT-HUB] normalize off ctrl seq=%u add rgb=0,0,0,0\r\n",
                (unsigned int)msg.seq);
        }
        printf("[WS63-LIGHT-HUB] rx p4 ctrl seq=%u module=7 on=%u rgb=%u,%u,%u bright=%u\r\n",
            (unsigned int)msg.seq, sw, rgb_value[0], rgb_value[1],
            rgb_value[2], rgb_value[3]);
        xh_hub_log_module_diag(XH_MODULE_ID_LIGHT, "p4-light");
        xh_sensor_state_set_light_commanded(sw != 0, rgb_value[0], rgb_value[1],
            rgb_value[2], rgb_value[3]);
        xh_hub_light_pending_set(msg.seq, sw, rgb_value);
    }

    bool ok = xh_sle_hub_send_control(msg.module_id, send_payload, forward_len);
    if (!ok && msg.module_id == XH_MODULE_ID_LIGHT) {
        xh_hub_light_pending_clear("send-fail");
    } else if (ok && msg.module_id == XH_MODULE_ID_LIGHT) {
        printf("[WS63-LIGHT-HUB] p4 ack means queued, waiting module ack/report seq=%u\r\n",
            (unsigned int)msg.seq);
    }
    xh_hub_enqueue_p4_ack(msg.seq, msg.module_id, ok ? XH_ACK_OK : XH_ACK_OFFLINE);
}

void xh_sensor_uart_handle_scene(const uint8_t *payload, uint32_t payload_len)
{
    if (payload == NULL || payload_len == 0 || payload_len > 0xFFFFU) {
        xh_hub_enqueue_p4_ack_cmd(0x0F13, 0, XH_MODULE_ID_HUB_SCENE, XH_ACK_INVALID);
        return;
    }

    xh_proto_msg_t msg = {0};
    uint8_t mode = 0;
    if (!xh_proto_decode(payload, (uint16_t)payload_len, &msg) ||
        msg.module_id != XH_MODULE_ID_HUB_SCENE ||
        msg.msg != XH_PROTO_MSG_CONTROL ||
        !xh_proto_get_u8(&msg, XH_TLV_SCENE_MODE, &mode)) {
        xh_hub_enqueue_p4_ack_cmd(0x0F13, 0, XH_MODULE_ID_HUB_SCENE, XH_ACK_INVALID);
        return;
    }

    bool ok = xh_scene_rule_set_mode(mode, "p4");
    xh_hub_enqueue_p4_ack_cmd(0x0F13, msg.seq, XH_MODULE_ID_HUB_SCENE,
        ok ? XH_ACK_OK : XH_ACK_INVALID);
    xh_sle_hub_send_scene_report(true);
}

void xh_sle_hub_tick(void)
{
    g_sht30_poll_age_s++;
    if (g_sht30_poll_age_s >= 2U) {
        uint8_t payload[XH_PROTO_MAX_LEN] = {0};
        uint16_t len = XH_PROTO_HDR_LEN;
        uint8_t seq = xh_proto_next_seq();
        g_sht30_poll_age_s = 0;
        if (xh_proto_begin(payload, sizeof(payload), seq, XH_MODULE_ID_SHT30,
                XH_PROTO_MSG_HEARTBEAT) &&
            xh_proto_finish(payload, sizeof(payload), &len)) {
            errcode_t ret = sle_client_send_msg_to_module(
                XH_MODULE_ID_SHT30, payload, len);
            printf("[WS63-SHT30-HUB] poll seq=%u ret=0x%X\r\n",
                (unsigned int)seq, (unsigned int)ret);
        }
    }

    if (!g_light_pending.active) {
        return;
    }
    g_light_pending.age_s++;
    if (g_light_pending.age_s >= 1U && !g_light_pending.fallback_tried) {
        g_light_pending.fallback_tried = true;
        if (sle_client_module_tx_pending(XH_MODULE_ID_LIGHT, g_light_pending.seq)) {
            printf("[WS63-LIGHT-HUB] no write cfm, fallback write_cmd module=7 seq=%u\r\n",
                (unsigned int)g_light_pending.seq);
            errcode_t ret = sle_client_retry_pending_write_cmd(XH_MODULE_ID_LIGHT);
            printf("[WS63-LIGHT-HUB] fallback write_cmd ret=0x%X seq=%u\r\n",
                (unsigned int)ret, (unsigned int)g_light_pending.seq);
        } else {
            printf("[WS63-LIGHT-HUB] write cfm observed, still waiting module ack/report seq=%u\r\n",
                (unsigned int)g_light_pending.seq);
        }
    }
    if (g_light_pending.age_s >= 2U) {
        bool tx_pending = sle_client_module_tx_pending(XH_MODULE_ID_LIGHT, g_light_pending.seq);
        printf("[WS63-LIGHT-HUB] pending timeout seq=%u module=7 reason=%s fallback=%u target on=%u rgb=%u,%u,%u bright=%u\r\n",
            (unsigned int)g_light_pending.seq,
            tx_pending ? "no-cfm-no-ack" : "no-ack-report",
            g_light_pending.fallback_tried ? 1 : 0,
            g_light_pending.on, g_light_pending.r, g_light_pending.g,
            g_light_pending.b, g_light_pending.brightness);
        xh_hub_log_module_diag(XH_MODULE_ID_LIGHT, "pending-timeout");
        g_light_pending.active = false;
    }
}

void xh_sle_hub_start(void)
{
    printf("[xh_sle_hub] start scan prefix XH_M_\r\n");
    sle_set_server_name("");
    sle_client_init();
#if USE_CUSTOM_MAC_ADDR
    uint8_t client_addr[SLE_ADDR_LEN] = {0x10, 0x22, 0x33, 0x44, 0x55, 0x01};
    sle_client_set_local_address(client_addr, SLE_ADDR_LEN);
#else
    sle_client_set_local_address(NULL, 0);
#endif
}
