#ifndef XH_SLE_HUB_H
#define XH_SLE_HUB_H

#include <stdbool.h>
#include <stdint.h>

void xh_sle_hub_start(void);
void xh_sle_hub_tick(void);
bool xh_sle_hub_send_fan_control(bool on);
bool xh_sle_hub_send_light_control(bool on, uint8_t r, uint8_t g,
                                   uint8_t b, uint8_t brightness);
bool xh_sle_hub_send_alarm_control(uint8_t mode);
void xh_sle_hub_send_scene_report(bool urgent);
void xh_sensor_uart_handle_query(const uint8_t *payload, uint32_t payload_len);
void xh_sensor_uart_handle_control(const uint8_t *payload, uint32_t payload_len);
void xh_sensor_uart_handle_scene(const uint8_t *payload, uint32_t payload_len);

#endif
