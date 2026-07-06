#ifndef SHT30_H
#define SHT30_H

#include <stdint.h>
#include "i2c.h"

#define SHT30_I2C_ADDR 0x44

void sht30_set_i2c_bus(i2c_bus_t bus);
uint32_t SHT30_Calibrate(void);
uint32_t SHT30_StartMeasure(void);
uint32_t SHT30_GetMeasureResult(float *temp, float *humi);

#endif
