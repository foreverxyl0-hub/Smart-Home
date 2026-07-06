#ifndef XH_SENSOR_STATE_H
#define XH_SENSOR_STATE_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    bool online;
    uint32_t last_seen_ms;
    int32_t temp100;
    int32_t humi100;
    uint8_t seq;
} xh_sht30_state_t;

typedef struct {
    bool online;
    bool alarm;
    bool valid;
    bool warmup;
    uint32_t raw_value;
    bool adc_valid;
    uint32_t adc_raw;
    uint32_t adc_avg;
    uint16_t adc_error;
    uint32_t last_seen_ms;
    uint8_t seq;
} xh_mq2_state_t;

typedef struct {
    bool online;
    bool on;
    uint8_t level;
    uint32_t last_seen_ms;
    bool commanded_on;
    bool gpio_on;
    uint8_t seq;
} xh_fan_state_t;

typedef struct {
    bool online;
    bool present;
    uint32_t last_seen_ms;
    uint32_t age_ms;
    uint8_t seq;
} xh_presence_state_t;

typedef struct {
    bool online;
    uint32_t lux100;
    uint16_t error_code;
    uint32_t err_count;
    uint32_t last_seen_ms;
    uint8_t seq;
} xh_bh1750_state_t;

typedef struct {
    bool online;
    bool on;
    uint8_t mode;
    uint8_t gpio_output;
    bool timeout_protected;
    uint32_t last_seen_ms;
    uint8_t seq;
} xh_alarm_state_t;

typedef struct {
    bool online;
    bool on;
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t brightness;
    uint8_t gpio_output;
    bool commanded_on;
    uint32_t last_seen_ms;
    uint8_t seq;
} xh_light_state_t;

void xh_sensor_state_init(void);
void xh_sensor_state_update_sht30(int32_t temp100, int32_t humi100);
void xh_sensor_state_update_sht30_seq(int32_t temp100, int32_t humi100, uint8_t seq);
bool xh_sensor_state_get_sht30(xh_sht30_state_t *out);
void xh_sensor_state_update_mq2_seq(bool alarm, bool valid, bool warmup,
                                    uint32_t raw_value, uint8_t seq);
void xh_sensor_state_update_mq2_adc(bool adc_valid, uint32_t adc_raw,
                                    uint32_t adc_avg, uint16_t adc_error);
bool xh_sensor_state_get_mq2(xh_mq2_state_t *out);
void xh_sensor_state_update_fan(bool on, uint8_t level);
void xh_sensor_state_update_fan_seq(bool on, uint8_t level, uint8_t seq);
void xh_sensor_state_set_fan_commanded(bool on);
bool xh_sensor_state_get_fan(xh_fan_state_t *out);
void xh_sensor_state_update_presence_seq(bool present, uint32_t age_ms, uint8_t seq);
bool xh_sensor_state_get_presence(xh_presence_state_t *out);
void xh_sensor_state_update_bh1750_seq(uint32_t lux100, uint16_t error_code, uint8_t seq);
bool xh_sensor_state_get_bh1750(xh_bh1750_state_t *out);
void xh_sensor_state_update_alarm_seq(bool on, uint8_t mode, uint8_t gpio_output,
                                      bool timeout_protected, uint8_t seq);
bool xh_sensor_state_get_alarm(xh_alarm_state_t *out);
void xh_sensor_state_update_light_seq(bool on, uint8_t r, uint8_t g, uint8_t b,
                                      uint8_t brightness, uint8_t gpio_output,
                                      uint8_t seq);
void xh_sensor_state_set_light_commanded(bool on, uint8_t r, uint8_t g,
                                         uint8_t b, uint8_t brightness);
bool xh_sensor_state_get_light(xh_light_state_t *out);
uint16_t xh_sensor_state_pack_snapshot(uint8_t *out, uint16_t cap);
uint16_t xh_sensor_state_pack_sht30_snapshot(uint8_t *out, uint16_t cap);
uint16_t xh_sensor_state_pack_mq2_snapshot(uint8_t *out, uint16_t cap);
uint16_t xh_sensor_state_pack_fan_snapshot(uint8_t *out, uint16_t cap);
uint16_t xh_sensor_state_pack_presence_snapshot(uint8_t *out, uint16_t cap);
uint16_t xh_sensor_state_pack_bh1750_snapshot(uint8_t *out, uint16_t cap);
uint16_t xh_sensor_state_pack_alarm_snapshot(uint8_t *out, uint16_t cap);
uint16_t xh_sensor_state_pack_light_snapshot(uint8_t *out, uint16_t cap);

#endif
