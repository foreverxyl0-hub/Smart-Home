#include "mq2_adc.h"

#include <stdio.h>

#include "adc.h"
#include "pinctrl.h"
#include "soc_osal.h"

#ifndef XH_MQ2_ADC_PIN
#define XH_MQ2_ADC_PIN 7
#endif

#ifndef XH_MQ2_ADC_PIN_MODE
#define XH_MQ2_ADC_PIN_MODE 0
#endif

#ifndef XH_MQ2_ADC_ENABLE
#define XH_MQ2_ADC_ENABLE 0
#endif

#ifndef XH_MQ2_ADC_CHANNEL
#define XH_MQ2_ADC_CHANNEL 0
#endif

#ifndef XH_MQ2_ADC_AVG_SHIFT
#define XH_MQ2_ADC_AVG_SHIFT 3
#endif

#ifndef XH_MQ2_ADC_SCAN_MS
#define XH_MQ2_ADC_SCAN_MS 200
#endif

static bool g_mq2_adc_inited;
static bool g_mq2_adc_ready;
static uint32_t g_mq2_adc_avg;
static uint32_t g_mq2_adc_last;
static uint32_t g_mq2_adc_count;

static void mq2_adc_scan_cb(uint8_t channel, uint32_t *buffer, uint32_t length, bool *next)
{
    if (next != NULL) {
        *next = false;
    }
    if (channel != (uint8_t)XH_MQ2_ADC_CHANNEL || buffer == NULL || length == 0) {
        return;
    }

    uint32_t sum = 0;
    for (uint32_t i = 0; i < length; i++) {
        sum += buffer[i];
    }
    g_mq2_adc_last = sum / length;
    g_mq2_adc_count += length;
}

void mq2_adc_init(void)
{
    if (g_mq2_adc_inited) {
        return;
    }
    g_mq2_adc_inited = true;

#if XH_MQ2_ADC_ENABLE
    uapi_pin_set_mode((pin_t)XH_MQ2_ADC_PIN, (pin_mode_t)XH_MQ2_ADC_PIN_MODE);
    errcode_t ret = uapi_adc_init(ADC_CLOCK_500KHZ);
    if (ret != ERRCODE_SUCC) {
        printf("[mq2_adc] init fail pin=%u mode=%u channel=%u ret=0x%x\r\n",
            (unsigned int)XH_MQ2_ADC_PIN, (unsigned int)XH_MQ2_ADC_PIN_MODE,
            (unsigned int)XH_MQ2_ADC_CHANNEL, (unsigned int)ret);
        return;
    }

    uapi_adc_power_en(AFE_GADC_MODE, true);
    g_mq2_adc_ready = true;
#endif

    printf("[mq2_adc] init enable=%u pin=%u mode=%u channel=%u ready=%u\r\n",
        (unsigned int)XH_MQ2_ADC_ENABLE, (unsigned int)XH_MQ2_ADC_PIN,
        (unsigned int)XH_MQ2_ADC_PIN_MODE, (unsigned int)XH_MQ2_ADC_CHANNEL,
        g_mq2_adc_ready ? 1U : 0U);
}

mq2_adc_sample_t mq2_adc_sample(void)
{
    mq2_adc_sample_t sample = {
        .enabled = (XH_MQ2_ADC_ENABLE != 0),
        .valid = false,
        .channel = (uint8_t)XH_MQ2_ADC_CHANNEL,
        .raw = 0,
        .avg = g_mq2_adc_avg,
        .error = 0,
    };

    if (!g_mq2_adc_inited) {
        mq2_adc_init();
    }
    if (!sample.enabled) {
        sample.error = 1;
        return sample;
    }
    if (!g_mq2_adc_ready) {
        sample.error = 2;
        return sample;
    }

    adc_scan_config_t config = {
        .type = 0,
        .threshold_l = 0.0f,
        .threshold_h = 3.6f,
        .freq = 0,
    };
    g_mq2_adc_last = 0;
    g_mq2_adc_count = 0;
    errcode_t ret = uapi_adc_auto_scan_ch_enable((uint8_t)XH_MQ2_ADC_CHANNEL,
        config, mq2_adc_scan_cb);
    if (ret != ERRCODE_SUCC) {
        sample.error = 3;
        return sample;
    }
    osal_msleep(XH_MQ2_ADC_SCAN_MS);
    ret = uapi_adc_auto_scan_ch_disable((uint8_t)XH_MQ2_ADC_CHANNEL);
    if (ret != ERRCODE_SUCC || g_mq2_adc_count == 0) {
        sample.error = 4;
        return sample;
    }

    sample.valid = true;
    sample.raw = g_mq2_adc_last;
    if (g_mq2_adc_avg == 0) {
        g_mq2_adc_avg = sample.raw;
    } else {
        g_mq2_adc_avg = g_mq2_adc_avg -
            (g_mq2_adc_avg >> XH_MQ2_ADC_AVG_SHIFT) +
            (sample.raw >> XH_MQ2_ADC_AVG_SHIFT);
    }
    sample.avg = g_mq2_adc_avg;
    return sample;
}
