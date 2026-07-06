#include "xh_module_app.h"

#include <stdio.h>
#include <unistd.h>

#include "cmsis_os2.h"
#include "ohos_init.h"
#include "soc_osal.h"
#include "xh_fan_control.h"
#include "xh_module_ids.h"
#include "xh_scene_rule.h"
#include "xh_sensor_state.h"
#include "xh_sle_hub.h"

#ifndef XH_BUILD_ROLE_HUB
#define XH_BUILD_ROLE_HUB 1
#endif

#ifndef XH_BUILD_MODULE_ID
#define XH_BUILD_MODULE_ID 0
#endif

#ifndef XH_ENABLE_SCENE_RULE
#define XH_ENABLE_SCENE_RULE 0
#endif

#ifndef XH_ENABLE_FAN_POWER_ON_SELF_TEST
#define XH_ENABLE_FAN_POWER_ON_SELF_TEST 1
#endif

#ifndef XH_ENABLE_FAN_KEY
#define XH_ENABLE_FAN_KEY 1
#endif

#ifndef XH_FAN_USE_LOCAL_GPIO
#define XH_FAN_USE_LOCAL_GPIO 1
#endif

#define XH_MODULE_APP_STACK_SIZE 0x1000
#define XH_MODULE_APP_PRIO 26

static void xh_module_app_task(void)
{
    printf("[xh_module_app] start\r\n");
    xh_sensor_state_init();

#if XH_BUILD_ROLE_HUB
    xh_fan_control_init(XH_FAN_TARGET_SLE_MODULE);
    xh_scene_rule_init();
    xh_sle_hub_start();
#else
#if XH_BUILD_MODULE_ID == XH_MODULE_ID_SHT30
    extern void xh_sht30_module_app_start(void);
    xh_sht30_module_app_start();
#elif XH_BUILD_MODULE_ID == XH_MODULE_ID_MQ2
    extern void xh_mq2_module_app_start(void);
    xh_mq2_module_app_start();
#elif XH_BUILD_MODULE_ID == XH_MODULE_ID_FAN
    extern void xh_fan_module_app_start(void);
    xh_fan_module_app_start();
#elif XH_BUILD_MODULE_ID == XH_MODULE_ID_LD2401
    extern void xh_ld2401_module_app_start(void);
    xh_ld2401_module_app_start();
#elif XH_BUILD_MODULE_ID == XH_MODULE_ID_BH1750
    extern void xh_bh1750_module_app_start(void);
    xh_bh1750_module_app_start();
#elif XH_BUILD_MODULE_ID == XH_MODULE_ID_ALARM
    extern void xh_alarm_module_app_start(void);
    xh_alarm_module_app_start();
#elif XH_BUILD_MODULE_ID == XH_MODULE_ID_LIGHT
    extern void xh_light_module_app_start(void);
    xh_light_module_app_start();
#else
    printf("[xh_module_app] unsupported module id=%d\r\n", XH_BUILD_MODULE_ID);
#endif
#endif

    while (1) {
#if XH_BUILD_ROLE_HUB
        xh_scene_rule_tick();
        xh_sle_hub_tick();
#endif
        osal_msleep(1000);
    }
}

void xh_module_app_start(void)
{
    osThreadAttr_t attr = {0};
    attr.name = "xh_module_app";
    attr.stack_size = XH_MODULE_APP_STACK_SIZE;
    attr.priority = XH_MODULE_APP_PRIO;
    if (osThreadNew((osThreadFunc_t)xh_module_app_task, NULL, &attr) == NULL) {
        printf("[xh_module_app] create task fail\r\n");
        return;
    }
    printf("[xh_module_app] create task ok\r\n");
}

static void xh_module_app_entry(void)
{
    xh_module_app_start();
}

SYS_RUN(xh_module_app_entry);
