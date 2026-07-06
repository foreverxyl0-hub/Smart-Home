#include "xh_sensor_state.h"

#include <stdio.h>
#include <string.h>

#include "xh_module_ids.h"
#include "xh_sle_proto.h"

static xh_sht30_state_t g_sht30_state;
static xh_mq2_state_t g_mq2_state;
static xh_fan_state_t g_fan_state;
static xh_presence_state_t g_presence_state;
static xh_bh1750_state_t g_bh1750_state;
static xh_alarm_state_t g_alarm_state;
static xh_light_state_t g_light_state;
static uint32_t g_state_tick_ms;

static uint32_t xh_state_now_ms(void)
{
    g_state_tick_ms += 1U;
    return g_state_tick_ms;
}

void xh_sensor_state_init(void)
{
    memset(&g_sht30_state, 0, sizeof(g_sht30_state));
    memset(&g_mq2_state, 0, sizeof(g_mq2_state));
    memset(&g_fan_state, 0, sizeof(g_fan_state));
    memset(&g_presence_state, 0, sizeof(g_presence_state));
    memset(&g_bh1750_state, 0, sizeof(g_bh1750_state));
    memset(&g_alarm_state, 0, sizeof(g_alarm_state));
    memset(&g_light_state, 0, sizeof(g_light_state));
    printf("[xh_state] init\r\n");
}

void xh_sensor_state_update_sht30(int32_t temp100, int32_t humi100)
{
    xh_sensor_state_update_sht30_seq(temp100, humi100, 0);
}

void xh_sensor_state_update_sht30_seq(int32_t temp100, int32_t humi100, uint8_t seq)
{
    g_sht30_state.online = true;
    g_sht30_state.last_seen_ms = xh_state_now_ms();
    g_sht30_state.temp100 = temp100;
    g_sht30_state.humi100 = humi100;
    g_sht30_state.seq = seq;
    printf("[xh_state] sht30 temp100=%ld humi100=%ld seq=%u\r\n",
        (long)temp100, (long)humi100, (unsigned int)seq);
}

bool xh_sensor_state_get_sht30(xh_sht30_state_t *out)
{
    if (out == NULL || !g_sht30_state.online) {
        return false;
    }
    *out = g_sht30_state;
    return true;
}

void xh_sensor_state_update_mq2_seq(bool alarm, bool valid, bool warmup,
                                    uint32_t raw_value, uint8_t seq)
{
    g_mq2_state.online = true;
    g_mq2_state.last_seen_ms = xh_state_now_ms();
    g_mq2_state.alarm = alarm;
    g_mq2_state.valid = valid;
    g_mq2_state.warmup = warmup;
    g_mq2_state.raw_value = raw_value;
    g_mq2_state.seq = seq;
    printf("[xh_state] mq2 alarm=%u valid=%u warmup=%u raw=%u adc_valid=%u adc_raw=%u adc_avg=%u adc_err=0x%04X seq=%u\r\n",
        alarm ? 1 : 0, valid ? 1 : 0, warmup ? 1 : 0,
        (unsigned int)raw_value, g_mq2_state.adc_valid ? 1 : 0,
        (unsigned int)g_mq2_state.adc_raw, (unsigned int)g_mq2_state.adc_avg,
        (unsigned int)g_mq2_state.adc_error, (unsigned int)seq);
}

void xh_sensor_state_update_mq2_adc(bool adc_valid, uint32_t adc_raw,
                                    uint32_t adc_avg, uint16_t adc_error)
{
    g_mq2_state.adc_valid = adc_valid;
    g_mq2_state.adc_raw = adc_raw;
    g_mq2_state.adc_avg = adc_avg;
    g_mq2_state.adc_error = adc_error;
}

bool xh_sensor_state_get_mq2(xh_mq2_state_t *out)
{
    if (out == NULL || !g_mq2_state.online) {
        return false;
    }
    *out = g_mq2_state;
    return true;
}

void xh_sensor_state_update_fan(bool on, uint8_t level)
{
    xh_sensor_state_update_fan_seq(on, level, 0);
}

void xh_sensor_state_update_fan_seq(bool on, uint8_t level, uint8_t seq)
{
    g_fan_state.online = true;
    g_fan_state.last_seen_ms = xh_state_now_ms();
    g_fan_state.on = on;
    g_fan_state.level = level;
    g_fan_state.commanded_on = on;
    g_fan_state.gpio_on = on;
    g_fan_state.seq = seq;
    printf("[xh_state] fan on=%u level=%u seq=%u\r\n",
        on ? 1 : 0, level, (unsigned int)seq);
}

void xh_sensor_state_set_fan_commanded(bool on)
{
    g_fan_state.commanded_on = on;
    printf("[xh_state] fan commanded=%u\r\n", on ? 1 : 0);
}

bool xh_sensor_state_get_fan(xh_fan_state_t *out)
{
    if (out == NULL || !g_fan_state.online) {
        return false;
    }
    *out = g_fan_state;
    return true;
}

void xh_sensor_state_update_presence_seq(bool present, uint32_t age_ms, uint8_t seq)
{
    g_presence_state.online = true;
    g_presence_state.last_seen_ms = xh_state_now_ms();
    g_presence_state.present = present;
    g_presence_state.age_ms = age_ms;
    g_presence_state.seq = seq;
    printf("[xh_state] presence present=%u age_ms=%u seq=%u\r\n",
        present ? 1 : 0, (unsigned int)age_ms, (unsigned int)seq);
}

bool xh_sensor_state_get_presence(xh_presence_state_t *out)
{
    if (out == NULL || !g_presence_state.online) {
        return false;
    }
    *out = g_presence_state;
    return true;
}

void xh_sensor_state_update_bh1750_seq(uint32_t lux100, uint16_t error_code, uint8_t seq)
{
    g_bh1750_state.online = true;
    g_bh1750_state.last_seen_ms = xh_state_now_ms();
    g_bh1750_state.lux100 = lux100;
    g_bh1750_state.error_code = error_code;
    if (error_code != 0) {
        g_bh1750_state.err_count++;
    }
    g_bh1750_state.seq = seq;
    printf("[xh_state] bh1750 lux100=%u err=0x%04X err_count=%u seq=%u\r\n",
        (unsigned int)lux100, (unsigned int)error_code,
        (unsigned int)g_bh1750_state.err_count, (unsigned int)seq);
}

bool xh_sensor_state_get_bh1750(xh_bh1750_state_t *out)
{
    if (out == NULL || !g_bh1750_state.online) {
        return false;
    }
    *out = g_bh1750_state;
    return true;
}

void xh_sensor_state_update_alarm_seq(bool on, uint8_t mode, uint8_t gpio_output,
                                      bool timeout_protected, uint8_t seq)
{
    g_alarm_state.online = true;
    g_alarm_state.last_seen_ms = xh_state_now_ms();
    g_alarm_state.on = on;
    g_alarm_state.mode = mode;
    g_alarm_state.gpio_output = gpio_output;
    g_alarm_state.timeout_protected = timeout_protected;
    g_alarm_state.seq = seq;
    printf("[xh_state] alarm on=%u mode=%u gpio=0x%02X timeout=%u seq=%u\r\n",
        on ? 1 : 0, (unsigned int)mode, (unsigned int)gpio_output,
        timeout_protected ? 1 : 0, (unsigned int)seq);
}

bool xh_sensor_state_get_alarm(xh_alarm_state_t *out)
{
    if (out == NULL || !g_alarm_state.online) {
        return false;
    }
    *out = g_alarm_state;
    return true;
}

void xh_sensor_state_update_light_seq(bool on, uint8_t r, uint8_t g, uint8_t b,
                                      uint8_t brightness, uint8_t gpio_output,
                                      uint8_t seq)
{
    g_light_state.online = true;
    g_light_state.last_seen_ms = xh_state_now_ms();
    g_light_state.on = on;
    g_light_state.r = r;
    g_light_state.g = g;
    g_light_state.b = b;
    g_light_state.brightness = brightness;
    g_light_state.gpio_output = gpio_output;
    g_light_state.commanded_on = on;
    g_light_state.seq = seq;
    printf("[xh_state] light on=%u rgb=%u,%u,%u brightness=%u gpio=%u seq=%u\r\n",
        on ? 1 : 0, (unsigned int)r, (unsigned int)g, (unsigned int)b,
        (unsigned int)brightness, (unsigned int)gpio_output, (unsigned int)seq);
}

void xh_sensor_state_set_light_commanded(bool on, uint8_t r, uint8_t g,
                                         uint8_t b, uint8_t brightness)
{
    g_light_state.commanded_on = on;
    g_light_state.r = r;
    g_light_state.g = g;
    g_light_state.b = b;
    g_light_state.brightness = brightness;
    printf("[xh_state] light commanded=%u rgb=%u,%u,%u brightness=%u\r\n",
        on ? 1 : 0, (unsigned int)r, (unsigned int)g, (unsigned int)b,
        (unsigned int)brightness);
}

bool xh_sensor_state_get_light(xh_light_state_t *out)
{
    if (out == NULL || !g_light_state.online) {
        return false;
    }
    *out = g_light_state;
    return true;
}

uint16_t xh_sensor_state_pack_snapshot(uint8_t *out, uint16_t cap)
{
    if (out == NULL || cap < XH_PROTO_HDR_LEN) {
        return 0;
    }

    uint16_t len = XH_PROTO_HDR_LEN;
    if (!xh_proto_begin(out, cap, xh_proto_next_seq(), 0, XH_PROTO_MSG_REPORT)) {
        return 0;
    }

    if (g_sht30_state.online) {
        if (!xh_proto_put_u8(out, cap, &len, XH_TLV_ONLINE, 1) ||
            !xh_proto_put_i32(out, cap, &len, XH_TLV_TEMP100, g_sht30_state.temp100) ||
            !xh_proto_put_i32(out, cap, &len, XH_TLV_HUMI100, g_sht30_state.humi100)) {
            return 0;
        }
    }
    if (g_fan_state.online) {
        if (!xh_proto_put_u8(out, cap, &len, XH_TLV_SWITCH, g_fan_state.on ? 1 : 0) ||
            !xh_proto_put_u8(out, cap, &len, XH_TLV_FAN_LEVEL, g_fan_state.level)) {
            return 0;
        }
    }

    if (!xh_proto_finish(out, cap, &len)) {
        return 0;
    }
    return len;
}

uint16_t xh_sensor_state_pack_presence_snapshot(uint8_t *out, uint16_t cap)
{
    if (out == NULL || cap < XH_PROTO_HDR_LEN || !g_presence_state.online) {
        return 0;
    }
    uint16_t len = XH_PROTO_HDR_LEN;
    if (!xh_proto_begin(out, cap, xh_proto_next_seq(), XH_MODULE_ID_LD2401, XH_PROTO_MSG_REPORT) ||
        !xh_proto_put_u8(out, cap, &len, XH_TLV_ONLINE, 1) ||
        !xh_proto_put_u8(out, cap, &len, XH_TLV_PRESENCE, g_presence_state.present ? 1 : 0) ||
        !xh_proto_put_i32(out, cap, &len, XH_TLV_AGE_MS, (int32_t)g_presence_state.age_ms) ||
        !xh_proto_finish(out, cap, &len)) {
        return 0;
    }
    return len;
}

uint16_t xh_sensor_state_pack_alarm_snapshot(uint8_t *out, uint16_t cap)
{
    if (out == NULL || cap < XH_PROTO_HDR_LEN || !g_alarm_state.online) {
        return 0;
    }
    uint16_t len = XH_PROTO_HDR_LEN;
    if (!xh_proto_begin(out, cap, xh_proto_next_seq(), XH_MODULE_ID_ALARM, XH_PROTO_MSG_REPORT) ||
        !xh_proto_put_u8(out, cap, &len, XH_TLV_ONLINE, 1) ||
        !xh_proto_put_u8(out, cap, &len, XH_TLV_SWITCH, g_alarm_state.on ? 1 : 0) ||
        !xh_proto_put_u8(out, cap, &len, XH_TLV_ALARM_MODE, g_alarm_state.mode) ||
        !xh_proto_put_u8(out, cap, &len, XH_TLV_GPIO_OUTPUT, g_alarm_state.gpio_output) ||
        !xh_proto_put_u8(out, cap, &len, XH_TLV_TIMEOUT_PROTECTED,
            g_alarm_state.timeout_protected ? 1 : 0) ||
        !xh_proto_finish(out, cap, &len)) {
        return 0;
    }
    return len;
}

uint16_t xh_sensor_state_pack_light_snapshot(uint8_t *out, uint16_t cap)
{
    if (out == NULL || cap < XH_PROTO_HDR_LEN || !g_light_state.online) {
        return 0;
    }
    uint8_t rgb[4] = {
        g_light_state.r,
        g_light_state.g,
        g_light_state.b,
        g_light_state.brightness,
    };
    uint16_t len = XH_PROTO_HDR_LEN;
    if (!xh_proto_begin(out, cap, xh_proto_next_seq(), XH_MODULE_ID_LIGHT, XH_PROTO_MSG_REPORT) ||
        !xh_proto_put_u8(out, cap, &len, XH_TLV_ONLINE, 1) ||
        !xh_proto_put_u8(out, cap, &len, XH_TLV_SWITCH, g_light_state.on ? 1 : 0) ||
        !xh_proto_put_bytes(out, cap, &len, XH_TLV_RGB, rgb, sizeof(rgb)) ||
        !xh_proto_put_u8(out, cap, &len, XH_TLV_GPIO_OUTPUT, g_light_state.gpio_output) ||
        !xh_proto_finish(out, cap, &len)) {
        return 0;
    }
    return len;
}

uint16_t xh_sensor_state_pack_mq2_snapshot(uint8_t *out, uint16_t cap)
{
    if (out == NULL || cap < XH_PROTO_HDR_LEN || !g_mq2_state.online) {
        return 0;
    }
    uint16_t len = XH_PROTO_HDR_LEN;
    if (!xh_proto_begin(out, cap, xh_proto_next_seq(), XH_MODULE_ID_MQ2, XH_PROTO_MSG_REPORT) ||
        !xh_proto_put_u8(out, cap, &len, XH_TLV_ONLINE, 1) ||
        !xh_proto_put_u8(out, cap, &len, XH_TLV_VALID, g_mq2_state.valid ? 1 : 0) ||
        !xh_proto_put_u8(out, cap, &len, XH_TLV_WARMUP, g_mq2_state.warmup ? 1 : 0) ||
        !xh_proto_put_u8(out, cap, &len, XH_TLV_SMOKE_ALARM, g_mq2_state.alarm ? 1 : 0) ||
        !xh_proto_put_u32(out, cap, &len, XH_TLV_RAW_VALUE, g_mq2_state.raw_value) ||
        !xh_proto_put_u8(out, cap, &len, XH_TLV_ADC_VALID, g_mq2_state.adc_valid ? 1 : 0) ||
        !xh_proto_put_u32(out, cap, &len, XH_TLV_ADC_RAW, g_mq2_state.adc_raw) ||
        !xh_proto_put_u32(out, cap, &len, XH_TLV_ADC_AVG, g_mq2_state.adc_avg) ||
        !xh_proto_put_u16(out, cap, &len, XH_TLV_ADC_ERROR, g_mq2_state.adc_error) ||
        !xh_proto_finish(out, cap, &len)) {
        return 0;
    }
    return len;
}

uint16_t xh_sensor_state_pack_bh1750_snapshot(uint8_t *out, uint16_t cap)
{
    if (out == NULL || cap < XH_PROTO_HDR_LEN || !g_bh1750_state.online) {
        return 0;
    }
    uint16_t len = XH_PROTO_HDR_LEN;
    if (!xh_proto_begin(out, cap, xh_proto_next_seq(), XH_MODULE_ID_BH1750, XH_PROTO_MSG_REPORT) ||
        !xh_proto_put_u8(out, cap, &len, XH_TLV_ONLINE, 1) ||
        !xh_proto_put_u32(out, cap, &len, XH_TLV_LUX100, g_bh1750_state.lux100) ||
        !xh_proto_put_u16(out, cap, &len, XH_TLV_ERROR_CODE, g_bh1750_state.error_code) ||
        !xh_proto_finish(out, cap, &len)) {
        return 0;
    }
    return len;
}

uint16_t xh_sensor_state_pack_sht30_snapshot(uint8_t *out, uint16_t cap)
{
    if (out == NULL || cap < XH_PROTO_HDR_LEN || !g_sht30_state.online) {
        return 0;
    }
    uint16_t len = XH_PROTO_HDR_LEN;
    if (!xh_proto_begin(out, cap, xh_proto_next_seq(), XH_MODULE_ID_SHT30, XH_PROTO_MSG_REPORT) ||
        !xh_proto_put_u8(out, cap, &len, XH_TLV_ONLINE, 1) ||
        !xh_proto_put_i32(out, cap, &len, XH_TLV_TEMP100, g_sht30_state.temp100) ||
        !xh_proto_put_i32(out, cap, &len, XH_TLV_HUMI100, g_sht30_state.humi100) ||
        !xh_proto_finish(out, cap, &len)) {
        return 0;
    }
    return len;
}

uint16_t xh_sensor_state_pack_fan_snapshot(uint8_t *out, uint16_t cap)
{
    if (out == NULL || cap < XH_PROTO_HDR_LEN || !g_fan_state.online) {
        return 0;
    }
    uint16_t len = XH_PROTO_HDR_LEN;
    if (!xh_proto_begin(out, cap, xh_proto_next_seq(), XH_MODULE_ID_FAN, XH_PROTO_MSG_REPORT) ||
        !xh_proto_put_u8(out, cap, &len, XH_TLV_ONLINE, 1) ||
        !xh_proto_put_u8(out, cap, &len, XH_TLV_SWITCH, g_fan_state.on ? 1 : 0) ||
        !xh_proto_put_u8(out, cap, &len, XH_TLV_FAN_LEVEL, g_fan_state.level) ||
        !xh_proto_finish(out, cap, &len)) {
        return 0;
    }
    return len;
}
