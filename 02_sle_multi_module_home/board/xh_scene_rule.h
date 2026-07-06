#ifndef XH_SCENE_RULE_H
#define XH_SCENE_RULE_H

#include <stdbool.h>
#include <stdint.h>

void xh_scene_rule_init(void);
void xh_scene_rule_tick(void);
bool xh_scene_rule_set_mode(uint8_t mode, const char *source);
uint8_t xh_scene_rule_get_mode(void);
uint8_t xh_scene_rule_get_cause(void);
uint16_t xh_scene_rule_pack_report(uint8_t *out, uint16_t cap);
void xh_scene_rule_on_sensor_update(uint8_t module_id);
void xh_scene_rule_on_manual_fan_control(bool on, const char *source);
void xh_scene_rule_on_fan_report_transition(bool was_on, bool is_on, const char *source);

#endif
