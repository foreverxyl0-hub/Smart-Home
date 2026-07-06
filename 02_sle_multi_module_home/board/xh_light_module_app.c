#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "cmsis_os2.h"
#include "soc_osal.h"

#include "ws2812.h"
#include "xh_module_ids.h"
#include "xh_sle_module_server.h"
#include "xh_sle_proto.h"

#define XH_LIGHT_STACK_SIZE 0x2800
#define XH_LIGHT_PRIO 25

#ifndef XH_LIGHT_HEARTBEAT_MS
#define XH_LIGHT_HEARTBEAT_MS 10000
#endif

#ifndef XH_LIGHT_DIN_PIN
#define XH_LIGHT_DIN_PIN 9
#endif

#ifndef XH_LIGHT_LED_COUNT
#define XH_LIGHT_LED_COUNT 6
#endif

#ifndef XH_WS2812_DIAG_ENABLE
#define XH_WS2812_DIAG_ENABLE 1
#endif

#ifndef XH_WS2812_DIAG_PROFILE_COUNT
#define XH_WS2812_DIAG_PROFILE_COUNT 5
#endif

static bool g_light_report_pending;

static void xh_light_dump_hex(const char *tag, const uint8_t *data, uint16_t len)
{
    printf("%s len=%u hex=", tag, (unsigned int)len);
    if (data == NULL) {
        printf("(null)\r\n");
        return;
    }
    for (uint16_t i = 0; i < len; i++) {
        printf("%02X", data[i]);
    }
    printf("\r\n");
}

static void xh_light_report(void)
{
    uint8_t frame[XH_PROTO_MAX_LEN] = {0};
    uint16_t len = XH_PROTO_HDR_LEN;
    uint8_t seq = xh_proto_next_seq();
    ws2812_color_t color = ws2812_get_color();
    uint8_t rgb[4] = {
        color.r,
        color.g,
        color.b,
        color.brightness,
    };

    if (xh_proto_begin(frame, sizeof(frame), seq, XH_MODULE_ID_LIGHT, XH_PROTO_MSG_REPORT) &&
        xh_proto_put_u8(frame, sizeof(frame), &len, XH_TLV_ONLINE, 1) &&
        xh_proto_put_u8(frame, sizeof(frame), &len, XH_TLV_SWITCH, color.on ? 1 : 0) &&
        xh_proto_put_bytes(frame, sizeof(frame), &len, XH_TLV_RGB, rgb, sizeof(rgb)) &&
        xh_proto_put_u8(frame, sizeof(frame), &len, XH_TLV_GPIO_OUTPUT, color.on ? 1 : 0) &&
        xh_proto_finish(frame, sizeof(frame), &len)) {
        printf("[WS63-LIGHT] report on=%u rgb=%u,%u,%u bright=%u gpio=%u seq=%u\r\n",
            color.on ? 1 : 0, color.r, color.g, color.b, color.brightness,
            color.on ? 1 : 0, (unsigned int)seq);
        (void)xh_sle_module_server_report(frame, len);
    }
}

static void xh_light_boot_self_test(void)
{
    printf("[WS63-LIGHT] boot self-test start din_pin=%u count=%u diag=%u profiles=%u pattern=R-G-B-W-OFF\r\n",
        XH_LIGHT_DIN_PIN, XH_LIGHT_LED_COUNT, XH_WS2812_DIAG_ENABLE,
        XH_WS2812_DIAG_PROFILE_COUNT);
    uint8_t profile_count = XH_WS2812_DIAG_ENABLE ? XH_WS2812_DIAG_PROFILE_COUNT : 1;
    for (uint8_t profile = 0; profile < profile_count; profile++) {
        printf("[WS63-LIGHT] diag profile=%u start\r\n", (unsigned int)profile);
        int ret = ws2812_set_color_profile(255, 0, 0, 128, profile);
        printf("[WS63-LIGHT] diag profile=%u red ret=%d\r\n", (unsigned int)profile, ret);
        osal_msleep(600);
        ret = ws2812_set_color_profile(0, 255, 0, 128, profile);
        printf("[WS63-LIGHT] diag profile=%u green ret=%d\r\n", (unsigned int)profile, ret);
        osal_msleep(600);
        ret = ws2812_set_color_profile(0, 0, 255, 128, profile);
        printf("[WS63-LIGHT] diag profile=%u blue ret=%d\r\n", (unsigned int)profile, ret);
        osal_msleep(600);
        ret = ws2812_set_color_profile(255, 255, 255, 128, profile);
        printf("[WS63-LIGHT] diag profile=%u white ret=%d\r\n", (unsigned int)profile, ret);
        osal_msleep(600);
        ret = ws2812_off_profile(profile);
        printf("[WS63-LIGHT] diag profile=%u off ret=%d\r\n", (unsigned int)profile, ret);
        osal_msleep(500);
    }
    int ret = ws2812_off();
    printf("[WS63-LIGHT] boot self-test done final off ret=%d\r\n", ret);
}

static void xh_light_control_cb(uint16_t conn_id, const uint8_t *data, uint16_t len)
{
    xh_proto_msg_t msg = {0};
    printf("[WS63-LIGHT] recv raw conn=0x%02X len=%u\r\n",
        conn_id, (unsigned int)len);
    xh_light_dump_hex("[WS63-LIGHT] recv raw", data, len);
    if (!xh_proto_decode(data, len, &msg) || msg.msg != XH_PROTO_MSG_CONTROL ||
        msg.module_id != XH_MODULE_ID_LIGHT) {
        printf("[WS63-LIGHT] decode invalid conn=0x%02X len=%u\r\n",
            conn_id, (unsigned int)len);
        (void)xh_sle_module_server_ack(0, XH_MODULE_ID_LIGHT, XH_ACK_INVALID);
        return;
    }
    printf("[WS63-LIGHT] decode ret=1 seq=%u module=%u msg=%u\r\n",
        (unsigned int)msg.seq, (unsigned int)msg.module_id, (unsigned int)msg.msg);

    uint8_t sw = 0;
    const uint8_t *rgb = NULL;
    uint8_t rgb_len = 0;
    uint8_t rgb_value[4] = {0, 0, 0, 0};
    if (!xh_proto_get_u8(&msg, XH_TLV_SWITCH, &sw)) {
        printf("[WS63-LIGHT] invalid tlv seq=%u sw_ok/rgb_len=%u\r\n",
            (unsigned int)msg.seq, (unsigned int)rgb_len);
        (void)xh_sle_module_server_ack(msg.seq, XH_MODULE_ID_LIGHT, XH_ACK_INVALID);
        return;
    }
    if (xh_proto_get_bytes(&msg, XH_TLV_RGB, &rgb, &rgb_len)) {
        if (rgb_len < 4) {
            printf("[WS63-LIGHT] invalid rgb len seq=%u rgb_len=%u\r\n",
                (unsigned int)msg.seq, (unsigned int)rgb_len);
            (void)xh_sle_module_server_ack(msg.seq, XH_MODULE_ID_LIGHT, XH_ACK_INVALID);
            return;
        }
        rgb_value[0] = rgb[0];
        rgb_value[1] = rgb[1];
        rgb_value[2] = rgb[2];
        rgb_value[3] = rgb[3];
    } else if (sw != 0) {
        printf("[WS63-LIGHT] invalid open without rgb seq=%u\r\n",
            (unsigned int)msg.seq);
        (void)xh_sle_module_server_ack(msg.seq, XH_MODULE_ID_LIGHT, XH_ACK_INVALID);
        return;
    }

    printf("[WS63-LIGHT] rx ctrl seq=%u module=7 on=%u rgb=%u,%u,%u bright=%u\r\n",
        (unsigned int)msg.seq, sw, rgb_value[0], rgb_value[1],
        rgb_value[2], rgb_value[3]);
    int ret;
    if (sw == 0) {
        ret = ws2812_off_control();
    } else {
        ret = ws2812_set_color_control(rgb_value[0], rgb_value[1], rgb_value[2], rgb_value[3]);
    }
    printf("[WS63-LIGHT] driver ret=%d din_pin=%u count=%u on=%u rgb=%u,%u,%u bright=%u\r\n",
        ret, XH_LIGHT_DIN_PIN, XH_LIGHT_LED_COUNT, sw,
        rgb_value[0], rgb_value[1], rgb_value[2], rgb_value[3]);
    if (ret != 0) {
        (void)xh_sle_module_server_ack(msg.seq, XH_MODULE_ID_LIGHT, XH_ACK_SEND_FAIL);
        return;
    }
    xh_light_report();
    (void)xh_sle_module_server_ack(msg.seq, XH_MODULE_ID_LIGHT, XH_ACK_OK);
    g_light_report_pending = false;
}

static void xh_light_task(void)
{
    printf("[WS63-LIGHT] boot build=%s %s module=7 din_pin=%u led_count=%u heartbeat=%u\r\n",
        __DATE__, __TIME__, XH_LIGHT_DIN_PIN, XH_LIGHT_LED_COUNT,
        (unsigned int)XH_LIGHT_HEARTBEAT_MS);
    int init_ret = ws2812_init();
    printf("[WS63-LIGHT] ws2812 init result=%d\r\n", init_ret);
    if (init_ret == 0) {
        xh_light_boot_self_test();
    }
    errcode_t server_ret = xh_sle_module_server_init(XH_MODULE_ID_LIGHT, xh_light_control_cb);
    printf("[WS63-LIGHT] server init ret=0x%X\r\n", (unsigned int)server_ret);
    xh_light_report();

    uint32_t since_report_ms = 0;
    while (1) {
        if (g_light_report_pending || since_report_ms >= XH_LIGHT_HEARTBEAT_MS) {
            xh_light_report();
            g_light_report_pending = false;
            since_report_ms = 0;
        }
        osal_msleep(200);
        since_report_ms += 200U;
    }
}

void xh_light_module_app_start(void)
{
    osThreadAttr_t attr = {0};
    attr.name = "xh_light";
    attr.stack_size = XH_LIGHT_STACK_SIZE;
    attr.priority = XH_LIGHT_PRIO;
    if (osThreadNew((osThreadFunc_t)xh_light_task, NULL, &attr) == NULL) {
        printf("[xh_light_module] create task fail\r\n");
        return;
    }
    printf("[xh_light_module] create task ok\r\n");
}
