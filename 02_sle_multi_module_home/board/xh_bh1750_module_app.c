#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "cmsis_os2.h"
#include "errcode.h"
#include "i2c.h"
#include "pinctrl.h"
#include "soc_osal.h"

#include "bh1750.h"
#include "xh_module_ids.h"
#include "xh_sle_module_server.h"
#include "xh_sle_proto.h"
#include "xh_sle_server.h"

#ifndef BH1750_I2C_BUS_ID
#define BH1750_I2C_BUS_ID 1
#endif
#ifndef BH1750_I2C_SCL_PIN
#define BH1750_I2C_SCL_PIN 16
#endif
#ifndef BH1750_I2C_SDA_PIN
#define BH1750_I2C_SDA_PIN 15
#endif
#ifndef BH1750_I2C_PIN_MODE
#define BH1750_I2C_PIN_MODE 2
#endif
#ifndef BH1750_I2C_BAUD
#define BH1750_I2C_BAUD 100000
#endif

#ifndef XH_BH1750_SAMPLE_MS
#define XH_BH1750_SAMPLE_MS 1000
#endif
#ifndef XH_BH1750_REPORT_DELTA_LUX100
#define XH_BH1750_REPORT_DELTA_LUX100 1000
#endif
#ifndef XH_BH1750_HEARTBEAT_MS
#define XH_BH1750_HEARTBEAT_MS 10000
#endif
#ifndef XH_BH1750_REINIT_MS
#define XH_BH1750_REINIT_MS 5000
#endif

#define XH_BH1750_STACK_SIZE 0x2800
#define XH_BH1750_PRIO 25

static uint16_t g_bh1750_last_error;

static uint32_t xh_abs_diff_u32(uint32_t a, uint32_t b)
{
    return (a >= b) ? (a - b) : (b - a);
}

static bool xh_bh1750_i2c_init(void)
{
    i2c_bus_t bus = (i2c_bus_t)BH1750_I2C_BUS_ID;
    printf("[xh_bh1750_module] i2c bus=%d scl=%d sda=%d mode=%d baud=%d addr=0x%02X/0x%02X\r\n",
        BH1750_I2C_BUS_ID, BH1750_I2C_SCL_PIN, BH1750_I2C_SDA_PIN,
        BH1750_I2C_PIN_MODE, BH1750_I2C_BAUD, BH1750_I2C_ADDR, BH1750_I2C_ADDR_ALT);

    (void)uapi_i2c_deinit(bus);
    uapi_pin_set_mode((pin_t)BH1750_I2C_SCL_PIN, (pin_mode_t)BH1750_I2C_PIN_MODE);
    uapi_pin_set_mode((pin_t)BH1750_I2C_SDA_PIN, (pin_mode_t)BH1750_I2C_PIN_MODE);
    bh1750_set_i2c_bus(bus);
    bh1750_set_i2c_addr(BH1750_I2C_ADDR);

    errcode_t ret = uapi_i2c_master_init(bus, BH1750_I2C_BAUD, 0);
    if (ret != ERRCODE_SUCC) {
        printf("[xh_bh1750_module] i2c init fail ret=0x%x\r\n", (unsigned int)ret);
        g_bh1750_last_error = (uint16_t)ret;
        return false;
    }

    uint32_t init_ret = bh1750_init();
    if (init_ret != 0) {
        printf("[xh_bh1750_module] sensor init fail ret=0x%x\r\n", (unsigned int)init_ret);
        g_bh1750_last_error = (uint16_t)init_ret;
        return false;
    }

    printf("[xh_bh1750_module] sensor init ok addr=0x%02X\r\n",
        (unsigned int)bh1750_get_i2c_addr());
    g_bh1750_last_error = 0;
    return true;
}

static void xh_bh1750_report(uint32_t lux100, uint16_t error_code)
{
    uint8_t frame[XH_PROTO_MAX_LEN] = {0};
    uint16_t len = XH_PROTO_HDR_LEN;
    uint8_t seq = xh_proto_next_seq();
    if (xh_proto_begin(frame, sizeof(frame), seq, XH_MODULE_ID_BH1750, XH_PROTO_MSG_REPORT) &&
        xh_proto_put_u8(frame, sizeof(frame), &len, XH_TLV_ONLINE, 1) &&
        xh_proto_put_u32(frame, sizeof(frame), &len, XH_TLV_LUX100, lux100) &&
        xh_proto_put_u16(frame, sizeof(frame), &len, XH_TLV_ERROR_CODE, error_code) &&
        xh_proto_finish(frame, sizeof(frame), &len)) {
        (void)xh_sle_module_server_report(frame, len);
    }
}

static void xh_bh1750_control_cb(uint16_t conn_id, const uint8_t *data, uint16_t len)
{
    (void)conn_id;
    (void)data;
    (void)len;
    (void)xh_sle_module_server_ack(0, XH_MODULE_ID_BH1750, XH_ACK_UNSUPPORTED);
}

static void xh_bh1750_task(void)
{
    printf("[xh_bh1750_module] start sample=%u delta=%u heartbeat=%u reinit=%u\r\n",
        (unsigned int)XH_BH1750_SAMPLE_MS,
        (unsigned int)XH_BH1750_REPORT_DELTA_LUX100,
        (unsigned int)XH_BH1750_HEARTBEAT_MS,
        (unsigned int)XH_BH1750_REINIT_MS);

    osal_msleep(1000);
    bool i2c_ready = xh_bh1750_i2c_init();
    (void)xh_sle_module_server_init(XH_MODULE_ID_BH1750, xh_bh1750_control_cb);

    uint32_t last_report_lux100 = 0;
    uint32_t current_lux100 = 0;
    uint32_t since_report_ms = XH_BH1750_HEARTBEAT_MS;
    uint32_t since_reinit_ms = 0;
    uint32_t err_count = 0;
    uint16_t last_report_error = 0xFFFF;
    bool have_report = false;

    while (1) {
        if (!i2c_ready) {
            since_reinit_ms += XH_BH1750_SAMPLE_MS;
            if (since_reinit_ms >= XH_BH1750_REINIT_MS) {
                since_reinit_ms = 0;
                i2c_ready = xh_bh1750_i2c_init();
            }
        }

        if (i2c_ready) {
            uint32_t lux100 = 0;
            uint32_t read_ret = bh1750_read_lux100(&lux100);
            if (read_ret == 0) {
                current_lux100 = lux100;
                g_bh1750_last_error = 0;
            } else {
                err_count++;
                g_bh1750_last_error = (uint16_t)read_ret;
                printf("[xh_bh1750_module] read fail ret=0x%x err_count=%u\r\n",
                    (unsigned int)read_ret, (unsigned int)err_count);
            }
        }

        since_report_ms += XH_BH1750_SAMPLE_MS;
        bool changed = !have_report ||
            xh_abs_diff_u32(current_lux100, last_report_lux100) >= XH_BH1750_REPORT_DELTA_LUX100;
        bool heartbeat = since_report_ms >= XH_BH1750_HEARTBEAT_MS;
        bool error_changed = g_bh1750_last_error != last_report_error;
        if (changed || heartbeat || error_changed) {
            printf("[xh_bh1750_module] report lux100=%u err=0x%04X conn=%u\r\n",
                (unsigned int)current_lux100, (unsigned int)g_bh1750_last_error,
                (unsigned int)sle_server_get_conn_count());
            xh_bh1750_report(current_lux100, g_bh1750_last_error);
            last_report_lux100 = current_lux100;
            last_report_error = g_bh1750_last_error;
            since_report_ms = 0;
            have_report = true;
        }

        osal_msleep(XH_BH1750_SAMPLE_MS);
    }
}

void xh_bh1750_module_app_start(void)
{
    osThreadAttr_t attr = {0};
    attr.name = "xh_bh1750";
    attr.stack_size = XH_BH1750_STACK_SIZE;
    attr.priority = XH_BH1750_PRIO;
    if (osThreadNew((osThreadFunc_t)xh_bh1750_task, NULL, &attr) == NULL) {
        printf("[xh_bh1750_module] create task fail\r\n");
    }
}
