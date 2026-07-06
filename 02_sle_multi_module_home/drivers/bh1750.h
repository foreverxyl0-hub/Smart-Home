#ifndef BH1750_H
#define BH1750_H

#include <stdint.h>

#include "i2c.h"

#ifndef BH1750_I2C_ADDR
#define BH1750_I2C_ADDR 0x23
#endif
#ifndef BH1750_I2C_ADDR_ALT
#define BH1750_I2C_ADDR_ALT 0x5C
#endif

void bh1750_set_i2c_bus(i2c_bus_t bus);
void bh1750_set_i2c_addr(uint16_t addr);
uint16_t bh1750_get_i2c_addr(void);
uint32_t bh1750_init(void);
uint32_t bh1750_read_lux100(uint32_t *lux100);

#endif
