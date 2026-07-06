#include "bh1750.h"

#include <stdio.h>

#include "soc_osal.h"

#ifndef BH1750_I2C_BUS_ID
#define BH1750_I2C_BUS_ID 1
#endif

#define BH1750_CMD_POWER_ON 0x01
#define BH1750_CMD_RESET 0x07
#define BH1750_CMD_ONE_TIME_H_RES 0x20
#define BH1750_ONE_TIME_H_RES_WAIT_MS 180

static i2c_bus_t g_bh1750_i2c_bus = (i2c_bus_t)BH1750_I2C_BUS_ID;
static uint16_t g_bh1750_i2c_addr = BH1750_I2C_ADDR;

void bh1750_set_i2c_bus(i2c_bus_t bus)
{
    g_bh1750_i2c_bus = bus;
}

void bh1750_set_i2c_addr(uint16_t addr)
{
    g_bh1750_i2c_addr = addr;
}

uint16_t bh1750_get_i2c_addr(void)
{
    return g_bh1750_i2c_addr;
}

static uint32_t bh1750_write_cmd(uint8_t cmd)
{
    i2c_data_t data = {0};
    data.send_buf = &cmd;
    data.send_len = 1;
    return (uint32_t)uapi_i2c_master_write(g_bh1750_i2c_bus, g_bh1750_i2c_addr, &data);
}

static uint32_t bh1750_read_bytes(uint8_t *buf, uint32_t len)
{
    i2c_data_t data = {0};
    data.receive_buf = buf;
    data.receive_len = len;
    return (uint32_t)uapi_i2c_master_read(g_bh1750_i2c_bus, g_bh1750_i2c_addr, &data);
}

uint32_t bh1750_init(void)
{
    uint16_t addrs[2] = { BH1750_I2C_ADDR, BH1750_I2C_ADDR_ALT };
    uint32_t last_ret = 0x80001314U;

    for (uint32_t i = 0; i < 2; i++) {
        g_bh1750_i2c_addr = addrs[i];
        uint32_t ret = bh1750_write_cmd(BH1750_CMD_POWER_ON);
        printf("[xh_bh1750_module] probe addr=0x%02X power_on ret=0x%x\r\n",
            (unsigned int)g_bh1750_i2c_addr, (unsigned int)ret);
        if (ret != 0) {
            last_ret = ret;
            continue;
        }
        osal_msleep(10);
        ret = bh1750_write_cmd(BH1750_CMD_RESET);
        printf("[xh_bh1750_module] probe addr=0x%02X reset ret=0x%x\r\n",
            (unsigned int)g_bh1750_i2c_addr, (unsigned int)ret);
        if (ret == 0) {
            return 0;
        }
        last_ret = ret;
    }
    return last_ret;
}

uint32_t bh1750_read_lux100(uint32_t *lux100)
{
    if (lux100 == NULL) {
        return 1;
    }

    uint32_t ret = bh1750_write_cmd(BH1750_CMD_POWER_ON);
    if (ret != 0) {
        return ret;
    }

    ret = bh1750_write_cmd(BH1750_CMD_ONE_TIME_H_RES);
    if (ret != 0) {
        return ret;
    }

    osal_msleep(BH1750_ONE_TIME_H_RES_WAIT_MS);

    uint8_t buf[2] = {0};
    ret = bh1750_read_bytes(buf, sizeof(buf));
    if (ret != 0) {
        return ret;
    }

    uint32_t raw = ((uint32_t)buf[0] << 8) | (uint32_t)buf[1];
    *lux100 = (raw * 1000U + 6U) / 12U;
    return 0;
}
