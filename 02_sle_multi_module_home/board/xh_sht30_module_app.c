#include <stdint.h>
#include <stdio.h>

#include "cmsis_os2.h"
#include "errcode.h"
#include "i2c.h"
#include "pinctrl.h"
#include "soc_osal.h"

#include "sht30.h"
#include "xh_module_ids.h"
#include "xh_sle_module_server.h"
#include "xh_sle_proto.h"
#include "xh_sle_server.h"

#ifndef SHT30_I2C_BUS_ID
#define SHT30_I2C_BUS_ID 1
#endif
#ifndef SHT30_I2C_SCL_PIN
#define SHT30_I2C_SCL_PIN 16
#endif
#ifndef SHT30_I2C_SDA_PIN
#define SHT30_I2C_SDA_PIN 15
#endif
#ifndef SHT30_I2C_PIN_MODE
#define SHT30_I2C_PIN_MODE 2
#endif
#ifndef SHT30_I2C_BAUD
#define SHT30_I2C_BAUD 400000
#endif

#define XH_SHT30_SAMPLE_MS 1000
#define XH_SHT30_STACK_SIZE 0x3000
#define XH_SHT30_PRIO 25

static int32_t g_latest_temp100;
static int32_t g_latest_humi100;
static bool g_latest_valid;

static int32_t xh_round_temp100(float temp)
{
    return (int32_t)(temp * 100.0f + (temp >= 0.0f ? 0.5f : -0.5f));
}

static int32_t xh_round_humi100(float humi)
{
    int32_t h100 = (int32_t)(humi * 100.0f + 0.5f);
    return (h100 < 0) ? 0 : h100;
}

static bool xh_sht30_i2c_init(void)
{
    i2c_bus_t bus = (i2c_bus_t)SHT30_I2C_BUS_ID;
    printf("[xh_sht30_module] i2c bus=%d scl=%d sda=%d mode=%d baud=%d\r\n",
        SHT30_I2C_BUS_ID, SHT30_I2C_SCL_PIN, SHT30_I2C_SDA_PIN,
        SHT30_I2C_PIN_MODE, SHT30_I2C_BAUD);

    (void)uapi_i2c_deinit(bus);
    uapi_pin_set_mode((pin_t)SHT30_I2C_SCL_PIN, (pin_mode_t)SHT30_I2C_PIN_MODE);
    uapi_pin_set_mode((pin_t)SHT30_I2C_SDA_PIN, (pin_mode_t)SHT30_I2C_PIN_MODE);
    sht30_set_i2c_bus(bus);

    errcode_t ret = uapi_i2c_master_init(bus, SHT30_I2C_BAUD, 0);
    if (ret != ERRCODE_SUCC) {
        printf("[xh_sht30_module] i2c init fail ret=0x%x\r\n", (unsigned int)ret);
        return false;
    }
    if (SHT30_Calibrate() != ERRCODE_SUCC) {
        printf("[xh_sht30_module] calibrate fail\r\n");
        return false;
    }
    return true;
}

static void xh_sht30_report(int32_t temp100, int32_t humi100)
{
    if (sle_server_get_conn_count() == 0) {
        return;
    }

    uint8_t frame[XH_PROTO_MAX_LEN] = {0};
    uint16_t len = XH_PROTO_HDR_LEN;
    uint8_t seq = xh_proto_next_seq();
    if (xh_proto_begin(frame, sizeof(frame), seq, XH_MODULE_ID_SHT30, XH_PROTO_MSG_REPORT) &&
        xh_proto_put_i32(frame, sizeof(frame), &len, XH_TLV_TEMP100, temp100) &&
        xh_proto_put_i32(frame, sizeof(frame), &len, XH_TLV_HUMI100, humi100) &&
        xh_proto_finish(frame, sizeof(frame), &len)) {
        errcode_t ret = xh_sle_module_server_report(frame, len);
        printf("[xh_sht30_module] report conn=%u len=%u ret=0x%x\r\n",
            (unsigned int)sle_server_get_conn_count(), (unsigned int)len,
            (unsigned int)ret);
    }
}

static void xh_sht30_control_cb(uint16_t conn_id, const uint8_t *data, uint16_t len)
{
    (void)conn_id;
    xh_proto_msg_t msg = {0};
    if (!xh_proto_decode(data, len, &msg) ||
        msg.module_id != XH_MODULE_ID_SHT30 ||
        msg.msg != XH_PROTO_MSG_HEARTBEAT) {
        return;
    }
    if (g_latest_valid) {
        printf("[xh_sht30_module] query seq=%u -> report latest\r\n",
            (unsigned int)msg.seq);
        xh_sht30_report(g_latest_temp100, g_latest_humi100);
    }
}

static void xh_sht30_task(void)
{
    osal_msleep(1000);
    if (!xh_sht30_i2c_init()) {
        return;
    }
    (void)xh_sle_module_server_init(XH_MODULE_ID_SHT30, xh_sht30_control_cb);

    while (1) {
        float temp = 0.0f;
        float humi = 0.0f;
        errcode_t start_ret = (errcode_t)SHT30_StartMeasure();
        if (start_ret == ERRCODE_SUCC) {
            osal_msleep(20);
        }
        errcode_t read_ret = (errcode_t)SHT30_GetMeasureResult(&temp, &humi);
        if (start_ret == ERRCODE_SUCC && read_ret == ERRCODE_SUCC) {
            int32_t temp100 = xh_round_temp100(temp);
            int32_t humi100 = xh_round_humi100(humi);
            g_latest_temp100 = temp100;
            g_latest_humi100 = humi100;
            g_latest_valid = true;
            printf("[xh_sht30_module] temp100=%ld humi100=%ld\r\n", (long)temp100, (long)humi100);
            xh_sht30_report(temp100, humi100);
        } else {
            printf("[xh_sht30_module] measure fail start=0x%x read=0x%x\r\n",
                (unsigned int)start_ret, (unsigned int)read_ret);
        }
        osal_msleep(XH_SHT30_SAMPLE_MS);
    }
}

void xh_sht30_module_app_start(void)
{
    osThreadAttr_t attr = {0};
    attr.name = "xh_sht30_mod";
    attr.stack_size = XH_SHT30_STACK_SIZE;
    attr.priority = XH_SHT30_PRIO;
    if (osThreadNew((osThreadFunc_t)xh_sht30_task, NULL, &attr) == NULL) {
        printf("[xh_sht30_module] create task fail\r\n");
    }
}
