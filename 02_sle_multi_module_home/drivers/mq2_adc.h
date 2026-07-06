#ifndef MQ2_ADC_H
#define MQ2_ADC_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    bool enabled;
    bool valid;
    uint8_t channel;
    uint32_t raw;
    uint32_t avg;
    uint16_t error;
} mq2_adc_sample_t;

void mq2_adc_init(void);
mq2_adc_sample_t mq2_adc_sample(void);

#endif
