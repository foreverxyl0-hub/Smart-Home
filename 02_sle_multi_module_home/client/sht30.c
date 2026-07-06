#include "sht30.h"

#include <stdio.h>
#include <string.h>

#include "errcode.h"
#include "soc_osal.h"

static i2c_bus_t g_i2c_bus = (i2c_bus_t)SHT30_I2C_BUS_ID;

void sht30_set_i2c_bus(i2c_bus_t bus)
{
    g_i2c_bus = bus;
}

static uint8_t sht30_crc8(const uint8_t *data, uint32_t len)
{
    uint8_t crc = 0xFF;
    for (uint32_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t bit = 0; bit < 8; bit++) {
            if ((crc & 0x80) != 0) {
                crc = (uint8_t)((crc << 1) ^ 0x31);
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

static uint32_t sht30_write_cmd(uint8_t high, uint8_t low)
{
    uint8_t buf[2] = { high, low };
    i2c_data_t data = {0};
    data.send_buf = buf;
    data.send_len = sizeof(buf);
    return (uint32_t)uapi_i2c_master_write(g_i2c_bus, SHT30_I2C_ADDR, &data);
}

static uint32_t sht30_read_bytes(uint8_t *buf, uint32_t len)
{
    i2c_data_t data = {0};
    data.receive_buf = buf;
    data.receive_len = len;
    return (uint32_t)uapi_i2c_master_read(g_i2c_bus, SHT30_I2C_ADDR, &data);
}

uint32_t SHT30_Calibrate(void)
{
    uint32_t ret = sht30_write_cmd(0x30, 0xA2);
    osal_msleep(20);
    return ret;
}

uint32_t SHT30_StartMeasure(void)
{
    return sht30_write_cmd(0x24, 0x00);
}

uint32_t SHT30_GetMeasureResult(float *temp, float *humi)
{
    uint8_t buf[6] = {0};
    if (temp == NULL || humi == NULL) {
        return 1;
    }

    osal_msleep(16);
    uint32_t ret = sht30_read_bytes(buf, sizeof(buf));
    if (ret != 0) {
        return ret;
    }

    if (sht30_crc8(buf, 2) != buf[2] || sht30_crc8(&buf[3], 2) != buf[5]) {
        printf("[demo-th-SHT] SHT30 CRC mismatch: %02x %02x %02x %02x %02x %02x\r\n",
            buf[0], buf[1], buf[2], buf[3], buf[4], buf[5]);
    }

    uint16_t traw = ((uint16_t)buf[0] << 8) | buf[1];
    uint16_t hraw = ((uint16_t)buf[3] << 8) | buf[4];
    *temp = -45.0f + 175.0f * (float)traw / 65535.0f;
    *humi = 100.0f * (float)hraw / 65535.0f;
    return 0;
}
