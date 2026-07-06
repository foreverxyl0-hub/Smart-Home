#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "cmsis_os2.h"
#include "soc_osal.h"

#include "fan_gpio.h"
#include "fan_key.h"
#include "xh_module_ids.h"
#include "xh_sle_module_server.h"
#include "xh_sle_proto.h"

#define XH_FAN_MODULE_STACK_SIZE 0x3000
#define XH_FAN_MODULE_PRIO 25
#define XH_FAN_TICK_MS 50
#define XH_FAN_KEY_ENABLE_DELAY_MS 2500

static void xh_fan_report(bool on)
{
    uint8_t frame[XH_PROTO_MAX_LEN] = {0};
    uint16_t len = XH_PROTO_HDR_LEN;
    uint8_t seq = xh_proto_next_seq();
    if (xh_proto_begin(frame, sizeof(frame), seq, XH_MODULE_ID_FAN, XH_PROTO_MSG_REPORT) &&
        xh_proto_put_u8(frame, sizeof(frame), &len, XH_TLV_ONLINE, 1) &&
        xh_proto_put_u8(frame, sizeof(frame), &len, XH_TLV_SWITCH, on ? 1 : 0) &&
        xh_proto_put_u8(frame, sizeof(frame), &len, XH_TLV_FAN_LEVEL, on ? 1 : 0) &&
        xh_proto_finish(frame, sizeof(frame), &len)) {
        printf("[xh_fan_module] report fan=%s seq=%u\r\n",
            on ? "on" : "off", (unsigned int)seq);
        (void)xh_sle_module_server_report(frame, len);
    }
}

static void xh_fan_control_cb(uint16_t conn_id, const uint8_t *data, uint16_t len)
{
    (void)conn_id;
    xh_proto_msg_t msg = {0};
    if (!xh_proto_decode(data, len, &msg) || msg.msg != XH_PROTO_MSG_CONTROL ||
        msg.module_id != XH_MODULE_ID_FAN) {
        (void)xh_sle_module_server_ack(0, XH_MODULE_ID_FAN, XH_ACK_INVALID);
        return;
    }

    uint8_t sw = 0;
    if (!xh_proto_get_u8(&msg, XH_TLV_SWITCH, &sw)) {
        (void)xh_sle_module_server_ack(msg.seq, XH_MODULE_ID_FAN, XH_ACK_INVALID);
        return;
    }

    fan_gpio_set_on(sw != 0);
    bool on = fan_gpio_get_on();
    printf("[xh_fan_module] control fan=%s seq=%u\r\n", on ? "on" : "off", (unsigned int)msg.seq);
    (void)xh_sle_module_server_ack(msg.seq, XH_MODULE_ID_FAN, XH_ACK_OK);
    xh_fan_report(on);
}

static void xh_fan_task(void)
{
    printf("[xh_fan_module] start build=%s %s key_delay=%u tick=%u\r\n",
        __DATE__, __TIME__, (unsigned int)XH_FAN_KEY_ENABLE_DELAY_MS,
        (unsigned int)XH_FAN_TICK_MS);
    fan_gpio_init();
    fan_gpio_set_on(false);
    (void)xh_sle_module_server_init(XH_MODULE_ID_FAN, xh_fan_control_cb);

    osal_msleep(XH_FAN_KEY_ENABLE_DELAY_MS);
    fan_gpio_set_on(false);
    fan_key_init();
    xh_fan_report(false);

    uint32_t tick = 0;
    while (1) {
        bool key_fan_on = false;
        const char *key_source = "unknown";
        if (fan_key_consume_toggled(&key_fan_on, &key_source)) {
            printf("[xh_fan_module] key fan=%s source=%s\r\n",
                key_fan_on ? "on" : "off", key_source);
            xh_fan_report(key_fan_on);
        }
        if ((tick++ % 100U) == 0U) {
            printf("[xh_fan_module] status conn=%u fan=%s\r\n",
                (unsigned int)sle_server_get_conn_count(),
                fan_gpio_get_on() ? "on" : "off");
        }
        osal_msleep(XH_FAN_TICK_MS);
    }
}

void xh_fan_module_app_start(void)
{
    osThreadAttr_t attr = {0};
    attr.name = "xh_fan_mod";
    attr.stack_size = XH_FAN_MODULE_STACK_SIZE;
    attr.priority = XH_FAN_MODULE_PRIO;
    if (osThreadNew((osThreadFunc_t)xh_fan_task, NULL, &attr) == NULL) {
        printf("[xh_fan_module] create task fail\r\n");
    }
}
