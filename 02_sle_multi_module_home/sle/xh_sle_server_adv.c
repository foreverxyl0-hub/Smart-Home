/*
MIT License

Copyright (c) 2026 Shenzhen Open Source Co-Creation Technology Co., Ltd. (AtomGit)

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include "string.h"
#include "securec.h"
#include "errcode.h"
#include "osal_addr.h"
#include "osal_debug.h"
#include "osal_task.h"

#include "xh_sle_server_adv.h"

#define LOG_TAG              ""  // "[sle_server_adv] "


#define SLE_CONN_INTV_MIN_DEFAULT               0x64
#define SLE_CONN_INTV_MAX_DEFAULT               0x64
#define SLE_ADV_INTERVAL_MIN_DEFAULT            0xC8
#define SLE_ADV_INTERVAL_MAX_DEFAULT            0xC8
#define SLE_CONN_SUPERVISION_TIMEOUT_DEFAULT    0x1F4
#define SLE_CONN_MAX_LATENCY                    0x1F3
#define SLE_ADV_TX_POWER                        10    // 0x0A
#define SLE_ADV_HANDLE_DEFAULT                  1
#define SLE_ADV_DATA_LEN_MAX                    251

static uint8_t sle_server_name[NAME_MAX_LENGTH] = SLE_SERVER_NAME;
static uint16_t sle_set_adv_server_name(uint8_t *adv_data, uint16_t max_len)
{
    uint8_t *server_name = sle_server_name;
    uint8_t len = strlen(sle_server_name) + 1;

    printf("%s[%s] [%d][0x%02X][%s]\r\n", LOG_TAG, __func__, len, len, server_name);

    adv_data[0] = len + 1;
    adv_data[1] = SLE_ADV_DATA_TYPE_COMPLETE_LOCAL_NAME;   // 0x0B

    errno_t ret = memcpy_s(&adv_data[2], max_len-2, server_name, len);
    if (ret != EOK) {
        printf("%s[%s] memcpy fail, ret[0x%02X]\r\n", LOG_TAG, __func__, ret);
        return 0;
    }
    return len+2;
}

static uint8_t g_local_addr[SLE_ADDR_LEN] = {0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC};
errcode_t sle_server_set_local_address(uint8_t* addr, uint8_t len)
{
    sle_addr_t local_address;
    local_address.type = SLE_ADDRESS_TYPE_PUBLIC;  // sle_common.h

    if ((addr == NULL) || (len < SLE_ADDR_LEN)) {
        random_mac_addr(g_local_addr);
    } else {
        (void)memcpy_s(g_local_addr, SLE_ADDR_LEN, addr, len);
    }

    printf("%s[%s] [%02X:%02X:%02X:%02X:%02X:%02X]\r\n",
        LOG_TAG, __func__,
        g_local_addr[0], g_local_addr[1], g_local_addr[2],
        g_local_addr[3], g_local_addr[4], g_local_addr[5]);

    return ERRCODE_SLE_SUCCESS;
}

errcode_t sle_server_get_local_address(uint8_t* addr, uint8_t len)
{
    if ((addr == NULL) || (len == 0)) {
        printf("%s[%s] invalid args\r\n", LOG_TAG, __func__);
        return ERRCODE_INVALID_PARAM;
    }

    errno_t ret = memcpy_s(addr, len, g_local_addr, SLE_ADDR_LEN);
    if (ret != EOK) {
        printf("%s[%s] memcpy fail, ret[0x%X]\r\n", LOG_TAG, __func__, ret);
        return ERRCODE_MEMCPY;
    }

    return ERRCODE_SUCC;
}

static uint16_t sle_set_adv_data(uint8_t *adv_data)
{
    errno_t ret = 0;
    size_t len = 0;
    uint16_t idx = 0;

    len = sizeof(struct sle_adv_common_value);
    struct sle_adv_common_value adv_disc_level = {
        .type   = SLE_ADV_DATA_TYPE_DISCOVERY_LEVEL,  // [0x01] DISCOVERY_LEVEL 发现级别
        .length = len - 1,                            // [0x02] 结构体大小，不包含type字节
        .value  = SLE_ANNOUNCE_LEVEL_NORMAL,          // [0x01] 一般可发现 Normal
    };
    ret = memcpy_s(&adv_data[idx], SLE_ADV_DATA_LEN_MAX-idx, &adv_disc_level, len);
    if (ret != EOK) {
        printf("%s[%s] adv_disc_level memcpy fail, ret[0x%X]\r\n", LOG_TAG, __func__, ret);
        return 0;
    }
    idx += len;

    len = sizeof(struct sle_adv_common_value);
    struct sle_adv_common_value adv_access_mode = {
        .type = SLE_ADV_DATA_TYPE_ACCESS_MODE,    // [0x02] ACCESS_MODE 访问模式
        .length = len - 1,                        // [0x02] 结构体大小，不包含type字节
        .value = 0,                               // [0x00] 无访问模式
    };
    ret = memcpy_s(&adv_data[idx], SLE_ADV_DATA_LEN_MAX-idx, &adv_access_mode, len);
    if (ret != EOK) {
        printf("%s[%s] adv_access_mode memcpy fail, ret[0x%X]\r\n", LOG_TAG, __func__, ret);
        return 0;
    }
    idx += len;

    return idx;
}

static uint16_t sle_set_scan_response_data(uint8_t *scan_rsp_data)
{
    errno_t ret;
    uint16_t idx = 0;
    size_t scan_rsp_data_len = sizeof(struct sle_adv_common_value);

    struct sle_adv_common_value tx_power_level = {
        .type = SLE_ADV_DATA_TYPE_TX_POWER_LEVEL,  // [0x0C] TX_POWER_LEVEL
        .length = scan_rsp_data_len - 1,           // [0x02] 结构体大小，不包含type字节
        .value = SLE_ADV_TX_POWER,                 // [0x0A] SLE_ADV_TX_POWER[10]
    };
    ret = memcpy_s(scan_rsp_data, SLE_ADV_DATA_LEN_MAX, &tx_power_level, scan_rsp_data_len);
    if (ret != EOK)
    {
        printf("%s[%s] tx_power_level memcpy fail, ret[0x%X]\r\n", LOG_TAG, __func__, ret);
        return 0;
    }
    idx += scan_rsp_data_len;
    idx += sle_set_adv_server_name(&scan_rsp_data[idx], SLE_ADV_DATA_LEN_MAX - idx);
    return idx;
}

#if USE_OHOS_API
static int sle_set_default_announce_param(void)
{
    printf("%s[%s] ServerAddr[%02X:%02X:%02X:%02X:%02X:%02X]\r\n",
           LOG_TAG, __func__,
           g_local_addr[0], g_local_addr[1], g_local_addr[2],
           g_local_addr[3], g_local_addr[4], g_local_addr[5]);

    SleAnnounceParam param       = {0};
    param.announceHandle         = SLE_ADV_HANDLE_DEFAULT;
    param.announceMode           = SLE_ANNOUNCE_MODE_CONNECTABLE_SCANABLE;
    param.announceGtRole         = SLE_ANNOUNCE_ROLE_T_CAN_NEGO;
    param.announceLevel          = SLE_ANNOUNCE_LEVEL_NORMAL;
    param.announceChannelMap     = SLE_ADV_CHANNEL_MAP_DEFAULT;
    param.announceIntervalMin    = SLE_ADV_INTERVAL_MIN_DEFAULT;
    param.announceIntervalMax    = SLE_ADV_INTERVAL_MAX_DEFAULT;
    param.connIntervalMin        = SLE_CONN_INTV_MIN_DEFAULT;
    param.connIntervalMax        = SLE_CONN_INTV_MAX_DEFAULT;
    param.connMaxLatency         = SLE_CONN_MAX_LATENCY;
    param.connSupervisionTimeout = SLE_CONN_SUPERVISION_TIMEOUT_DEFAULT;
    param.ownAddr.type           = SLE_ADDRESS_TYPE_PUBLIC;

    errno_t ret = memcpy_s(param.ownAddr.addr, SLE_ADDR_LEN, g_local_addr, SLE_ADDR_LEN);
    if (ret != EOK) {
        printf("%s[%s] data memcpy fail, ret[0x%X]\r\n", LOG_TAG, __func__, ret);
        return 0;
    }

    return SleSetAnnounceParam(param.announceHandle, &param);
}
#else
static int sle_set_default_announce_param(void)
{
    printf("%s[%s] ServerAddr[%02X:%02X:%02X:%02X:%02X:%02X]\r\n",
           LOG_TAG, __func__,
           g_local_addr[0], g_local_addr[1], g_local_addr[2],
           g_local_addr[3], g_local_addr[4], g_local_addr[5]);

    sle_announce_param_t param     = {0};
    param.announce_handle          = SLE_ADV_HANDLE_DEFAULT;
    param.announce_mode            = SLE_ANNOUNCE_MODE_CONNECTABLE_SCANABLE;
    param.announce_gt_role         = SLE_ANNOUNCE_ROLE_T_CAN_NEGO;
    param.announce_level           = SLE_ANNOUNCE_LEVEL_NORMAL;
    param.announce_channel_map     = SLE_ADV_CHANNEL_MAP_DEFAULT;
    param.announce_interval_min    = SLE_ADV_INTERVAL_MIN_DEFAULT;
    param.announce_interval_max    = SLE_ADV_INTERVAL_MAX_DEFAULT;
    param.conn_interval_min        = SLE_CONN_INTV_MIN_DEFAULT;
    param.conn_interval_max        = SLE_CONN_INTV_MAX_DEFAULT;
    param.conn_max_latency         = SLE_CONN_MAX_LATENCY;
    param.conn_supervision_timeout = SLE_CONN_SUPERVISION_TIMEOUT_DEFAULT;
    param.own_addr.type            = SLE_ADDRESS_TYPE_PUBLIC;

    errno_t ret = memcpy_s(param.own_addr.addr, SLE_ADDR_LEN, g_local_addr, SLE_ADDR_LEN);
    if (ret != EOK) {
        printf("%s[%s] data memcpy fail, ret[0x%X]\r\n", LOG_TAG, __func__, ret);
        return 0;
    }

    return sle_set_announce_param(param.announce_handle, &param);
}
#endif

#if USE_OHOS_API
static int sle_set_default_announce_data(void)
{
    errcode_t ret;
    uint8_t announce_data_len = 0;
    uint8_t seek_data_len = 0;

    uint8_t adv_handle = SLE_ADV_HANDLE_DEFAULT;
    uint8_t announce_data[SLE_ADV_DATA_LEN_MAX] = {0};
    uint8_t seek_rsp_data[SLE_ADV_DATA_LEN_MAX] = {0};
    uint8_t data_index = 0;

    SleAnnounceData data = {0};
    announce_data_len    = sle_set_adv_data(announce_data);
    data.announceData    = announce_data;
    data.announceDataLen = announce_data_len;

    printf("%s[%s] data.announce_data: [%d][", LOG_TAG, __func__, data.announceDataLen);
    for (data_index = 0; data_index < data.announceDataLen; data_index++) {
        printf("0x%02X ", data.announceData[data_index]);
    }
    printf("]\r\n");

    seek_data_len       = sle_set_scan_response_data(seek_rsp_data);
    data.seekRspData    = seek_rsp_data;
    data.seekRspDataLen = seek_data_len;
    printf("%s[%s] seek_rsp_data: [%d][0x%02X][", LOG_TAG, __func__, data.seekRspDataLen, data.seekRspDataLen);
    for (data_index = 0; data_index < data.seekRspDataLen; data_index++) {
        printf("0x%02X ", data.seekRspData[data_index]);
    }
    printf("]\r\n");

    ret = SleSetAnnounceData(adv_handle, &data);
    if (ret == ERRCODE_SLE_SUCCESS) {
        printf("%s[%s] SleSetAnnounceData success.\r\n", LOG_TAG, __func__);
    } else {
        printf("%s[%s] SleSetAnnounceData fail. ret[0x%X]\r\n", LOG_TAG, __func__, ret);
    }

    return ERRCODE_SLE_SUCCESS;
}
#else
static int sle_set_default_announce_data(void)
{
    errcode_t ret;
    uint8_t announce_data_len = 0;
    uint8_t seek_data_len = 0;

    uint8_t adv_handle = SLE_ADV_HANDLE_DEFAULT;
    uint8_t announce_data[SLE_ADV_DATA_LEN_MAX] = {0};
    uint8_t seek_rsp_data[SLE_ADV_DATA_LEN_MAX] = {0};
    uint8_t data_index = 0;

    sle_announce_data_t data = {0};
    announce_data_len    = sle_set_adv_data(announce_data);
    data.announce_data    = announce_data;
    data.announce_data_len = announce_data_len;

    printf("%s[data.announce_data: [%d][", LOG_TAG, data.announce_data_len);
    for (data_index = 0; data_index < data.announce_data_len; data_index++) {
        printf("0x%02X ", data.announce_data[data_index]);
    }
    printf("]\r\n");

    seek_data_len          = sle_set_scan_response_data(seek_rsp_data);
    data.seek_rsp_data     = seek_rsp_data;
    data.seek_rsp_data_len = seek_data_len;
    printf("%s[seek_rsp_data: [%d][0x%02X][", LOG_TAG, data.seek_rsp_data_len, data.seek_rsp_data_len);
    for (data_index = 0; data_index < data.seek_rsp_data_len; data_index++) {
        printf("0x%02X ", data.seek_rsp_data[data_index]);
    }
    printf("]\r\n");

    ret = sle_set_announce_data(adv_handle, &data);
    if (ret == ERRCODE_SLE_SUCCESS) {
        printf("%s[%s] sle_set_announce_data success.\r\n", LOG_TAG, __func__);
    } else {
        printf("%s[%s] sle_set_announce_data fail. ret[0x%X]\r\n", LOG_TAG, __func__, ret);
    }

    return ERRCODE_SLE_SUCCESS;
}
#endif

////////////////////////////////////////////////////////
static void sle_server_sle_enable_cbk(errcode_t status)
{
    printf("%s[%s] status[0x%02X] -> sle_enable_server_cbk\r\n", LOG_TAG, __func__, status);
    sle_enable_server_cbk();
}

static void sle_server_sle_disable_cbk(errcode_t status)
{
    printf("%s[%s]\r\n", LOG_TAG, __func__);
}

static void sle_server_announce_enable_cbk(uint32_t announce_id, errcode_t status)
{
    printf("%s[%s] announce_id[0x%02X], state[0x%02X]\r\n", LOG_TAG, __func__, announce_id, status);
}

static void sle_server_announce_disable_cbk(uint32_t announce_id, errcode_t status)
{
    printf("%s[%s] announce_id[0x%02X], state[0x%02X]\r\n", LOG_TAG, __func__, announce_id, status);
}

static void sle_server_announce_terminal_cbk(uint32_t announce_id)
{
    printf("%s[%s] announce_id[0x%02X]\r\n", LOG_TAG, __func__, announce_id);
}

static void sle_server_announce_remove_cbk(uint32_t announce_id, errcode_t status)
{
    printf("%s[%s] announce_id[0x%02X]\r\n", LOG_TAG, __func__, announce_id);
}
static void sle_server_seek_enable_cbk(errcode_t status)
{
    printf("%s[%s]\r\n", LOG_TAG, __func__);
}
static void sle_server_seek_disable_cbk(errcode_t status)
{
    printf("%s[%s]\r\n", LOG_TAG, __func__);
}

static void sle_server_seek_result_cbk(sle_seek_result_info_t *seek_result_data)
{
    printf("%s[%s]\r\n", LOG_TAG, __func__);
}
static void sle_server_sle_dfr_cbk(void)
{
    printf("%s[%s]\r\n", LOG_TAG, __func__);
}
errcode_t sle_announce_register_cbks(void)
{
    errcode_t ret = ERRCODE_FAIL;
#if USE_OHOS_API
    SleAnnounceSeekCallbacks seek_cbks = {0};
    seek_cbks.sleEnableCb           = sle_server_sle_enable_cbk;
    seek_cbks.sleDisableCb          = sle_server_sle_disable_cbk;
    seek_cbks.sleAnnounceEnableCb   = sle_server_announce_enable_cbk;
    seek_cbks.sleAnnounceDisableCb  = sle_server_announce_disable_cbk;
    seek_cbks.sleAnnounceTerminalCb = sle_server_announce_terminal_cbk;
    seek_cbks.sleSeekResultCb       = sle_server_seek_result_cbk;
    ret = SleAnnounceSeekRegisterCallbacks(&seek_cbks);
#else
    sle_announce_seek_callbacks_t seek_cbks = {0};
    seek_cbks.sle_enable_cb        = sle_server_sle_enable_cbk;
    seek_cbks.sle_disable_cb       = sle_server_sle_disable_cbk;
    seek_cbks.announce_enable_cb   = sle_server_announce_enable_cbk;
    seek_cbks.announce_disable_cb  = sle_server_announce_disable_cbk;
    seek_cbks.announce_terminal_cb = sle_server_announce_terminal_cbk;
    seek_cbks.announce_remove_cb   = sle_server_announce_remove_cbk;
    seek_cbks.seek_enable_cb       = sle_server_seek_enable_cbk;
    seek_cbks.seek_disable_cb      = sle_server_seek_disable_cbk;
    seek_cbks.seek_result_cb       = sle_server_seek_result_cbk;
    ret = sle_announce_seek_register_callbacks(&seek_cbks);
#endif

    return ret;
}
////////////////////////////////////////////////////////

errcode_t sle_server_adv_init(void)
{
    errcode_t ret = ERRCODE_FAIL;

    sle_set_default_announce_param();
    sle_set_default_announce_data();

#if USE_OHOS_API
    ret = SleStartAnnounce(SLE_ADV_HANDLE_DEFAULT);
#else
    ret = sle_start_announce(SLE_ADV_HANDLE_DEFAULT);
#endif

    printf("%s[%s] sle_start_announce %s: ret[0x%X]\r\n",
            LOG_TAG, __func__, (ret == ERRCODE_SUCC)?"OK":"NG", ret);

    return ret;
}
