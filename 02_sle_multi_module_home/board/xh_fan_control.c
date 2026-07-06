#include "xh_fan_control.h"

#include <stdio.h>

#include "xh_sle_hub.h"
#include "xh_sensor_state.h"

static xh_fan_target_t g_fan_target = XH_FAN_TARGET_LOCAL_GPIO;

void xh_fan_control_init(xh_fan_target_t target)
{
    g_fan_target = target;
    if (g_fan_target == XH_FAN_TARGET_LOCAL_GPIO) {
        printf("[xh_fan_control] local_gpio disabled in hub control file\r\n");
    } else {
        printf("[xh_fan_control] target=sle_module\r\n");
    }
}

bool xh_fan_control_set_on(bool on)
{
    if (g_fan_target != XH_FAN_TARGET_LOCAL_GPIO) {
        xh_sensor_state_set_fan_commanded(on);
        return xh_sle_hub_send_fan_control(on);
    }

    return false;
}

bool xh_fan_control_get_on(void)
{
    if (g_fan_target != XH_FAN_TARGET_LOCAL_GPIO) {
        return false;
    }
    return false;
}
