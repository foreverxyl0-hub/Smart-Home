#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "cmsis_os2.h"
#include "soc_osal.h"

#include "ld2401_gpio.h"
#include "xh_module_ids.h"
#include "xh_sle_module_server.h"
#include "xh_sle_proto.h"

#define XH_LD2401_STACK_SIZE 0x2400
#define XH_LD2401_PRIO 25

#ifndef XH_LD2401_POLL_MS
#define XH_LD2401_POLL_MS 50
#endif

#ifndef XH_LD2401_DEBOUNCE_MS
#define XH_LD2401_DEBOUNCE_MS 200
#endif

#ifndef XH_LD2401_HEARTBEAT_MS
#define XH_LD2401_HEARTBEAT_MS 10000
#endif

static void xh_ld2401_report(bool present, uint32_t age_ms)
{
    uint8_t frame[XH_PROTO_MAX_LEN] = {0};
    uint16_t len = XH_PROTO_HDR_LEN;
    uint8_t seq = xh_proto_next_seq();
    if (xh_proto_begin(frame, sizeof(frame), seq, XH_MODULE_ID_LD2401, XH_PROTO_MSG_REPORT) &&
        xh_proto_put_u8(frame, sizeof(frame), &len, XH_TLV_ONLINE, 1) &&
        xh_proto_put_u8(frame, sizeof(frame), &len, XH_TLV_PRESENCE, present ? 1 : 0) &&
        xh_proto_put_i32(frame, sizeof(frame), &len, XH_TLV_AGE_MS, (int32_t)age_ms) &&
        xh_proto_finish(frame, sizeof(frame), &len)) {
        (void)xh_sle_module_server_report(frame, len);
    }
}

static void xh_ld2401_control_cb(uint16_t conn_id, const uint8_t *data, uint16_t len)
{
    (void)conn_id;
    (void)data;
    (void)len;
    (void)xh_sle_module_server_ack(0, XH_MODULE_ID_LD2401, XH_ACK_UNSUPPORTED);
}

static void xh_ld2401_task(void)
{
    printf("[xh_ld2401_module] start poll=%u debounce=%u heartbeat=%u\r\n",
        (unsigned int)XH_LD2401_POLL_MS,
        (unsigned int)XH_LD2401_DEBOUNCE_MS,
        (unsigned int)XH_LD2401_HEARTBEAT_MS);

    ld2401_gpio_init();
    (void)xh_sle_module_server_init(XH_MODULE_ID_LD2401, xh_ld2401_control_cb);

    bool stable = ld2401_gpio_is_present();
    bool candidate = stable;
    uint32_t candidate_ms = 0;
    uint32_t since_report_ms = 0;
    uint32_t stable_age_ms = 0;
    xh_ld2401_report(stable, stable_age_ms);

    while (1) {
        bool now = ld2401_gpio_is_present();
        if (now != candidate) {
            candidate = now;
            candidate_ms = 0;
        } else if (candidate_ms < XH_LD2401_DEBOUNCE_MS) {
            candidate_ms += XH_LD2401_POLL_MS;
        }

        stable_age_ms += XH_LD2401_POLL_MS;
        since_report_ms += XH_LD2401_POLL_MS;

        if (candidate != stable && candidate_ms >= XH_LD2401_DEBOUNCE_MS) {
            stable = candidate;
            stable_age_ms = 0;
            since_report_ms = 0;
            printf("[xh_ld2401_module] present=%u\r\n", stable ? 1 : 0);
            xh_ld2401_report(stable, stable_age_ms);
        } else if (since_report_ms >= XH_LD2401_HEARTBEAT_MS) {
            since_report_ms = 0;
            printf("[xh_ld2401_module] heartbeat conn=%u present=%u age=%u\r\n",
                (unsigned int)sle_server_get_conn_count(),
                stable ? 1 : 0,
                (unsigned int)stable_age_ms);
            xh_ld2401_report(stable, stable_age_ms);
        }

        osal_msleep(XH_LD2401_POLL_MS);
    }
}

void xh_ld2401_module_app_start(void)
{
    osThreadAttr_t attr = {0};
    attr.name = "xh_ld2401";
    attr.stack_size = XH_LD2401_STACK_SIZE;
    attr.priority = XH_LD2401_PRIO;
    if (osThreadNew((osThreadFunc_t)xh_ld2401_task, NULL, &attr) == NULL) {
        printf("[xh_ld2401_module] create task fail\r\n");
    }
}
