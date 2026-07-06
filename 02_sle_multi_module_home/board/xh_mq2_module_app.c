#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "cmsis_os2.h"
#include "soc_osal.h"

#include "mq2_adc.h"
#include "mq2_gpio.h"
#include "xh_module_ids.h"
#include "xh_sle_module_server.h"
#include "xh_sle_proto.h"
#include "xh_sle_server.h"

#ifndef XH_MQ2_POLL_MS
#define XH_MQ2_POLL_MS 100
#endif

#ifndef XH_MQ2_DEBOUNCE_COUNT
#define XH_MQ2_DEBOUNCE_COUNT 3
#endif

#ifndef XH_MQ2_WARMUP_MS
#define XH_MQ2_WARMUP_MS 60000
#endif

#ifndef XH_MQ2_HEARTBEAT_MS
#define XH_MQ2_HEARTBEAT_MS 10000
#endif

#define XH_MQ2_STACK_SIZE 0x2400
#define XH_MQ2_PRIO 25

static void xh_mq2_report(bool alarm, bool warmup, uint32_t raw_value,
                          const mq2_adc_sample_t *adc)
{
    uint8_t frame[XH_PROTO_MAX_LEN] = {0};
    uint16_t len = XH_PROTO_HDR_LEN;
    uint8_t seq = xh_proto_next_seq();
    bool valid = !warmup;
    uint8_t adc_valid = (adc != NULL && adc->valid) ? 1 : 0;
    uint32_t adc_raw = (adc != NULL) ? adc->raw : 0;
    uint32_t adc_avg = (adc != NULL) ? adc->avg : 0;
    uint16_t adc_error = (adc != NULL) ? adc->error : 1;

    if (xh_proto_begin(frame, sizeof(frame), seq, XH_MODULE_ID_MQ2, XH_PROTO_MSG_REPORT) &&
        xh_proto_put_u8(frame, sizeof(frame), &len, XH_TLV_ONLINE, 1) &&
        xh_proto_put_u8(frame, sizeof(frame), &len, XH_TLV_VALID, valid ? 1 : 0) &&
        xh_proto_put_u8(frame, sizeof(frame), &len, XH_TLV_WARMUP, warmup ? 1 : 0) &&
        xh_proto_put_u8(frame, sizeof(frame), &len, XH_TLV_SMOKE_ALARM,
            (valid && alarm) ? 1 : 0) &&
        xh_proto_put_u32(frame, sizeof(frame), &len, XH_TLV_RAW_VALUE, raw_value) &&
        xh_proto_put_u8(frame, sizeof(frame), &len, XH_TLV_ADC_VALID, adc_valid) &&
        xh_proto_put_u32(frame, sizeof(frame), &len, XH_TLV_ADC_RAW, adc_raw) &&
        xh_proto_put_u32(frame, sizeof(frame), &len, XH_TLV_ADC_AVG, adc_avg) &&
        xh_proto_put_u16(frame, sizeof(frame), &len, XH_TLV_ADC_ERROR, adc_error) &&
        xh_proto_finish(frame, sizeof(frame), &len)) {
        (void)xh_sle_module_server_report(frame, len);
    }
}

static void xh_mq2_control_cb(uint16_t conn_id, const uint8_t *data, uint16_t len)
{
    (void)conn_id;
    (void)data;
    (void)len;
    (void)xh_sle_module_server_ack(0, XH_MODULE_ID_MQ2, XH_ACK_UNSUPPORTED);
}

static void xh_mq2_task(void)
{
    printf("[xh_mq2_module] start poll=%u debounce=%u warmup=%u heartbeat=%u\r\n",
        (unsigned int)XH_MQ2_POLL_MS,
        (unsigned int)XH_MQ2_DEBOUNCE_COUNT,
        (unsigned int)XH_MQ2_WARMUP_MS,
        (unsigned int)XH_MQ2_HEARTBEAT_MS);

    mq2_gpio_init();
    mq2_adc_init();
    (void)xh_sle_module_server_init(XH_MODULE_ID_MQ2, xh_mq2_control_cb);

    bool stable_alarm = mq2_gpio_is_alarm();
    bool candidate_alarm = stable_alarm;
    uint32_t candidate_count = 0;
    uint32_t elapsed_ms = 0;
    uint32_t since_report_ms = XH_MQ2_HEARTBEAT_MS;
    bool last_report_alarm = stable_alarm;
    bool last_report_warmup = true;

    while (1) {
        bool now_alarm = mq2_gpio_is_alarm();
        if (now_alarm == candidate_alarm) {
            if (candidate_count < XH_MQ2_DEBOUNCE_COUNT) {
                candidate_count++;
            }
        } else {
            candidate_alarm = now_alarm;
            candidate_count = 1;
        }

        bool changed = false;
        if (candidate_count >= XH_MQ2_DEBOUNCE_COUNT && stable_alarm != candidate_alarm) {
            stable_alarm = candidate_alarm;
            changed = true;
        }

        bool warmup = elapsed_ms < XH_MQ2_WARMUP_MS;
        since_report_ms += XH_MQ2_POLL_MS;
        if (changed || since_report_ms >= XH_MQ2_HEARTBEAT_MS || warmup != last_report_warmup) {
            uint32_t raw_value = (uint32_t)mq2_gpio_get_level();
            mq2_adc_sample_t adc = mq2_adc_sample();
            printf("[xh_mq2_module] report alarm=%u valid=%u warmup=%u raw=%u adc_valid=%u adc_raw=%u adc_avg=%u adc_err=0x%04X conn=%u\r\n",
                stable_alarm ? 1 : 0, warmup ? 0 : 1, warmup ? 1 : 0,
                (unsigned int)raw_value, adc.valid ? 1 : 0,
                (unsigned int)adc.raw, (unsigned int)adc.avg,
                (unsigned int)adc.error, (unsigned int)sle_server_get_conn_count());
            xh_mq2_report(stable_alarm, warmup, raw_value, &adc);
            last_report_alarm = stable_alarm;
            last_report_warmup = warmup;
            since_report_ms = 0;
        }

        (void)last_report_alarm;
        osal_msleep(XH_MQ2_POLL_MS);
        elapsed_ms += XH_MQ2_POLL_MS;
    }
}

void xh_mq2_module_app_start(void)
{
    osThreadAttr_t attr = {0};
    attr.name = "xh_mq2";
    attr.stack_size = XH_MQ2_STACK_SIZE;
    attr.priority = XH_MQ2_PRIO;
    if (osThreadNew((osThreadFunc_t)xh_mq2_task, NULL, &attr) == NULL) {
        printf("[xh_mq2_module] create task fail\r\n");
        return;
    }
    printf("[xh_mq2_module] create task ok\r\n");
}
