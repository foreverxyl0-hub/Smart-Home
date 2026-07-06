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

#include "cmsis_os2.h"
#include "ohos_init.h"
#include "securec.h"
#include "sle_common.h"
#include "osal_debug.h"
#include "sle_errcode.h"
#include "osal_addr.h"
#include "osal_task.h"

#include "pinctrl.h"
#include "errcode.h"

#include "xh_sle_server_adv.h"
#include "xh_sle_server.h"

#define LOG_TAG                 ""  //"[sle_server] "

#define UUID_LEN_2  2
static uint8_t g_sle_uuid_len2[UUID_LEN_2]   = {0x12, 0x34};
static uint8_t g_sle_uuid_base[SLE_UUID_LEN] = {
    0x37, 0xBE, 0xA8, 0x80, 0xFC, 0x70, 0x11, 0xEA,
    0xB7, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};
#define NTF_BUF_LEN  64
static uint8_t g_ntf_buf[NTF_BUF_LEN] = {0x00};

static uint8_t  g_server_id       = 0;
static uint16_t g_service_handle  = 0;
static uint16_t g_property_handle = 0;
static uint16_t g_cccd_handle     = 0;

/*
  每个 Server 最多同时连接 SLE_N_CLIENT_1_SERVER 个 Client,
  每个 Client 对应一个 sle_server_conn_slot_t 结构体，即一个连接槽
*/
static sle_server_conn_slot_t g_server_conn_slots[SLE_N_CLIENT_1_SERVER] = {0};

uint32_t sle_server_get_max_slots(void)
{
    return SLE_N_CLIENT_1_SERVER;
}

uint32_t sle_server_get_conn_count(void)
{
    uint32_t n = 0;
    for (uint32_t i = 0; i < sle_server_get_max_slots(); i++) {
        if (g_server_conn_slots[i].in_use != 0) {
            n++;
        }
    }
    return n;
}

static void sle_uuid_setu2(uint16_t u2, sle_uuid_t *out)
{
    memcpy_s(out->uuid, SLE_UUID_LEN, g_sle_uuid_base, SLE_UUID_LEN);
    out->len = UUID_LEN_2;
    out->uuid[SLE_UUID_LEN-UUID_LEN_2]   = (uint8_t)(u2);  // LSB
    out->uuid[SLE_UUID_LEN-UUID_LEN_2+1] = (uint8_t)((u2) >> 0x8);  // MSB
}

static void sle_uuid_print(sle_uuid_t *uuid)
{
    if (uuid == NULL) {
        printf("%s[%s] uuid is null\r\n", LOG_TAG, __func__);
        return;
    }
    if (uuid->len == UUID_LEN_2) {
        printf("%s[%s] uuid: %02X %02X\r\n",
            LOG_TAG, __func__, uuid->uuid[14], uuid->uuid[15]);
    } else if (uuid->len == SLE_UUID_LEN) {  // SLE_UUID_LEN[16]
        printf("%s[%s] uuid: \r\n", LOG_TAG, __func__);
        printf("%s  0x%02x 0x%02x 0x%02x \r\n",
            LOG_TAG, uuid->uuid[0], uuid->uuid[1], uuid->uuid[2], uuid->uuid[3]);
        printf("%s  0x%02x 0x%02x 0x%02x \r\n",
            LOG_TAG, uuid->uuid[4], uuid->uuid[5], uuid->uuid[6], uuid->uuid[7]);
        printf("%s  0x%02x 0x%02x 0x%02x \r\n",
            LOG_TAG, uuid->uuid[8], uuid->uuid[9], uuid->uuid[10], uuid->uuid[11]);
        printf("%s  0x%02x 0x%02x 0x%02x \r\n",
            LOG_TAG, uuid->uuid[12], uuid->uuid[13], uuid->uuid[14], uuid->uuid[15]);
    }
}

static int32_t sle_server_conn_index_by_id(uint16_t conn_id)
{
    for (uint32_t i = 0; i < sle_server_get_max_slots(); i++) {
        if (g_server_conn_slots[i].in_use != 0 && g_server_conn_slots[i].conn_id == conn_id) {
            return (int32_t)i;
        }
    }
    return -1;
}
errcode_t sle_server_get_client_address(uint16_t conn_id, uint8_t* addr, uint8_t len)
{
    if ((addr == NULL)||(len == 0)) {
        printf("%s[%s] invalid args\r\n", LOG_TAG, __func__);
        return;
    }
    int32_t slot = sle_server_conn_index_by_id(conn_id);
    if (slot < 0) {
        printf("%s[%s] invalid conn_id\r\n", LOG_TAG, __func__);
        return;
    }
    (void)memcpy_s(addr, len, &g_server_conn_slots[slot].client_addr.addr, SLE_ADDR_LEN);
}

static int32_t sle_server_conn_alloc(uint16_t conn_id, const sle_addr_t *addr)
{
    for (uint32_t i = 0; i < sle_server_get_max_slots(); i++) {
        if (g_server_conn_slots[i].in_use == 0) {
            g_server_conn_slots[i].in_use = 1;
            g_server_conn_slots[i].conn_id = conn_id;
            g_server_conn_slots[i].cccd_live[0] = 0x03;
            g_server_conn_slots[i].cccd_live[1] = 0x00;
            (void)memcpy_s(&g_server_conn_slots[i].client_addr, sizeof(sle_addr_t),
                   addr, sizeof(sle_addr_t));
            return (int32_t)i;
        }
    }
    return -1;
}

static void sle_server_conn_remove(uint16_t conn_id)
{
    int32_t idx = sle_server_conn_index_by_id(conn_id);
    if (idx < 0) {
        return;
    }
    memset_s(&g_server_conn_slots[idx], sizeof(g_server_conn_slots[idx]), 0, sizeof(g_server_conn_slots[idx]));
}

static void sle_server_restart_announce(void)
{
    errcode_t ret = ERRCODE_FAIL;
#if USE_OHOS_API
    ret = SleStartAnnounce(SLE_ADV_HANDLE_DEFAULT);
#else
    ret = sle_start_announce(SLE_ADV_HANDLE_DEFAULT);
#endif
    printf("%s[%s] max_slots[%d] conn_count[%d] ret[0x%02X]:%s\r\n",
        LOG_TAG, __func__, sle_server_get_max_slots(), sle_server_get_conn_count(),
        ret, (ret == ERRCODE_SLE_SUCCESS)?"OK":"NG");
}

////////////////////////////////////////////////////////
static void ssaps_add_service_cbk(uint8_t server_id, sle_uuid_t *uuid,
                                  uint16_t handle, errcode_t status)
{
    printf("%s[%s] server_id[0x%02X] handle[0x%02X] status[0x%02X]\r\n",
        LOG_TAG, __func__, server_id, handle, status);
    sle_uuid_print(uuid);
}

static void ssaps_add_property_cbk(uint8_t server_id, sle_uuid_t *uuid,
                                   uint16_t service_handle, uint16_t handle,
                                   errcode_t status)
{
    printf("%s[%s] server_id[0x%02X] service_handle[0x%02X] handle[0x%02X] status[0x%02X]\r\n",
           LOG_TAG, __func__, server_id, service_handle, handle, status);
    sle_uuid_print(uuid);
}

static void ssaps_add_descriptor_cbk(uint8_t server_id, sle_uuid_t *uuid,
                                     uint16_t service_handle, uint16_t property_handle,
                                     errcode_t status)
{
    printf("%s[%s] "
        "server_id[0x%02X] service_handle[0x%02X] property_handle[0x%02X] status[0x%02X]\r\n",
        LOG_TAG, __func__, server_id, service_handle, property_handle, status);
    sle_uuid_print(uuid);
}

static void ssaps_start_service_cbk(uint8_t server_id, uint16_t handle, errcode_t status)
{
    printf("%s[%s] server_id[0x%02X] handle[0x%02X] status[0x%02X]\r\n",
        LOG_TAG, __func__, server_id, handle, status);
}

static void ssaps_delete_all_service_cbk(uint8_t server_id, errcode_t status)
{
    printf("%s[%s] server_id[0x%02X] status[0x%02X]\r\n",
        LOG_TAG, __func__, server_id, status);
}

static void ssaps_mtu_changed_cbk(uint8_t server_id, uint16_t conn_id,
                                  ssap_exchange_info_t *info, errcode_t status)
{
    printf("%s[%s] "
        "server_id[0x%02X] conn_id[0x%02X] status[0x%02X] mtu_size[0x%02X]\r\n",
        LOG_TAG, __func__, server_id, conn_id, status, info->mtu_size);
}

static uint16_t sle_server_cccd_handle(void)
{
    if (g_cccd_handle != 0U) {
        return g_cccd_handle;
    }
    if (g_property_handle != 0U && g_property_handle < 0xFFFFU) {
        return (uint16_t)(g_property_handle + 1U);
    }
    return 0U;
}

static void sle_server_update_cccd_for_conn(uint16_t conn_id, const uint8_t *value, uint16_t length)
{
    int32_t idx = sle_server_conn_index_by_id(conn_id);
    if (idx < 0 || value == NULL || length < 2) {
        return;
    }
    g_server_conn_slots[idx].cccd_live[0] = value[0];
    g_server_conn_slots[idx].cccd_live[1] = value[1];
    printf("%s[%s] conn_id[0x%04X] CCC[%02X %02X]\r\n",
           LOG_TAG, __func__, conn_id, g_server_conn_slots[idx].cccd_live[0], g_server_conn_slots[idx].cccd_live[1]);
}

static void sle_ssaps_send_read_rsp(uint8_t server_id, uint16_t conn_id, uint16_t request_id, const uint8_t *value, uint16_t value_len)
{
    errcode_t ret = ERRCODE_FAIL;
#if USE_OHOS_API
    SsapsSendRsp rsp = {0};
    rsp.requestId = request_id;
    rsp.status    = (uint8_t)ERRCODE_SLE_SUCCESS;
    rsp.valueLen  = value_len;
    rsp.value     = (uint8_t *)value;
    ret           = (errcode_t)SsapsSendResponse(server_id, conn_id, &rsp);
#else
    ssaps_send_rsp_t rsp = {0};
    rsp.request_id = request_id;
    rsp.status     = (uint8_t)ERRCODE_SLE_SUCCESS;
    rsp.value_len  = value_len;
    rsp.value      = (uint8_t *)value;
    ret            = ssaps_send_response(server_id, conn_id, &rsp);
#endif
    printf("%s[%s] server_id[0x%02X] conn_id[0x%02X] len[%u] ret[0x%02X]\r\n",
           LOG_TAG, __func__, server_id, conn_id, (unsigned)value_len, ret);
}

void ssaps_read_request_cbk(uint8_t server_id, uint16_t conn_id, ssaps_req_read_cb_t *read_cb_para, errcode_t status)
{
    if (read_cb_para == NULL) {
        return;
    }

    printf("%s[%s ] "
        "server_id[0x%02X] conn_id[0x%02X] status[0x%02X] "
        "handle[0x%04X] type[0x%02X] need_rsp[%u]\r\n",
        LOG_TAG, __func__, server_id, conn_id, status,
        read_cb_para->handle, read_cb_para->type, (unsigned)read_cb_para->need_rsp);

    if (status != ERRCODE_SUCC && status != (errcode_t)ERRCODE_SLE_SUCCESS) {
        return;
    }
    if (!read_cb_para->need_rsp) {
        return;
    }

    uint16_t cccd_hdl = sle_server_cccd_handle();
    if ((cccd_hdl != 0U) && (read_cb_para->handle == cccd_hdl)) {
        static const uint8_t cccd_default[2] = {0x03, 0x00};
        int32_t cidx = sle_server_conn_index_by_id(conn_id);
        const uint8_t *cccd_val = (cidx >= 0) ? g_server_conn_slots[cidx].cccd_live : cccd_default;

        printf("%s[%s ] sle_ssaps_send_read_rsp(cccd_hdl[0x%04X])\r\n", LOG_TAG, __func__, cccd_hdl);
        sle_ssaps_send_read_rsp(server_id, conn_id, read_cb_para->request_id,
                                cccd_val, (uint16_t)sizeof(cccd_default));
        return;
    }

    if ((read_cb_para->handle == g_property_handle) &&
        (read_cb_para->type == SSAP_PROPERTY_TYPE_VALUE)) {
        printf("%s[%s ] sle_ssaps_send_read_rsp(handle[0x%04X])\r\n", LOG_TAG, __func__, g_property_handle);
        sle_ssaps_send_read_rsp(server_id, conn_id, read_cb_para->request_id, g_ntf_buf, NTF_BUF_LEN);
    }
}

static bool sle_server_payload_looks_like_ccc(const uint8_t *value, uint16_t length)
{
    if (value == NULL || length != 2) {
        return false;
    }
    if (value[1] != 0) {
        return false;
    }
    return (value[0] == 0x01u || value[0] == 0x02u || value[0] == 0x03u);
}

static void sle_ssaps_send_write_rsp(uint8_t server_id, uint16_t conn_id, uint16_t request_id, uint8_t st)
{
#if USE_OHOS_API
    SsapsSendRsp rsp = {0};
    rsp.requestId = request_id;
    rsp.status    = st;
    rsp.valueLen  = 0;
    rsp.value     = NULL;
    (void)SsapsSendResponse(server_id, conn_id, &rsp);
#else
    ssaps_send_rsp_t rsp = {0};
    rsp.request_id = request_id;
    rsp.status     = st;
    rsp.value_len  = 0;
    rsp.value      = NULL;
    (void)ssaps_send_response(server_id, conn_id, &rsp);
#endif
}

void ssaps_write_request_cbk(uint8_t server_id, uint16_t conn_id,
                             ssaps_req_write_cb_t *write_cb_para, errcode_t status)
{
    if (write_cb_para == NULL) {
        printf("%s[%s] write_cb_para is NULL\r\n", LOG_TAG, __func__);
        return;
    }

    printf("%s[%s] ------------------------------------------<0\r\n", LOG_TAG, __func__);
    printf("%s[%s] "
        "server_id[0x%02X] conn_id[0x%02X] handle[0x%04X] type[0x%02X] need_rsp[%d] status[0x%02X] len[%d]\r\n",
        LOG_TAG, __func__, server_id, conn_id, write_cb_para->handle, write_cb_para->type,
        write_cb_para->need_rsp, status, write_cb_para->length);

    if (write_cb_para->type == SSAP_DESCRIPTOR_CLIENT_CONFIGURATION) {
        printf("%s[%s] CCCD DESC write handle[0x%04X] len[%u]\r\n",
               LOG_TAG, __func__, write_cb_para->handle, (unsigned)write_cb_para->length);
        sle_server_update_cccd_for_conn(conn_id, write_cb_para->value, write_cb_para->length);
        if (write_cb_para->need_rsp) {
            sle_ssaps_send_write_rsp(server_id, conn_id, write_cb_para->request_id, (uint8_t)ERRCODE_SLE_SUCCESS);
        }
        printf("%s[%s] ------------------------------------------>1\r\n", LOG_TAG, __func__);
        return;
    }

    if (write_cb_para->handle != g_property_handle ||
        write_cb_para->type   != SSAP_PROPERTY_TYPE_VALUE) {
        printf("%s[%s] Not-Target, handle[0x%04X] expect[0x%02X]\r\n",
               LOG_TAG, __func__, write_cb_para->handle, g_property_handle);
        if (write_cb_para->need_rsp) {
            sle_ssaps_send_write_rsp(server_id, conn_id, write_cb_para->request_id, (uint8_t)ERRCODE_SLE_SUCCESS);
        }
        printf("%s[%s] ------------------------------------------>2\r\n", LOG_TAG, __func__);
        return;
    }

    if (write_cb_para->value == NULL || write_cb_para->length == 0) {
        if (write_cb_para->need_rsp) {
            sle_ssaps_send_write_rsp(server_id, conn_id, write_cb_para->request_id, (uint8_t)ERRCODE_SLE_SUCCESS);
        }
        printf("%s[%s] ------------------------------------------>3\r\n", LOG_TAG, __func__);
        return;
    }

    if (sle_server_payload_looks_like_ccc(write_cb_para->value, write_cb_para->length)) {
        printf("%s[%s] CCC subscribe via VALUE handle[0x%04X] [0x%02X 0x%02X]\r\n",
               LOG_TAG, __func__, write_cb_para->handle, write_cb_para->value[0], write_cb_para->value[1]);
        sle_server_update_cccd_for_conn(conn_id, write_cb_para->value, write_cb_para->length);
        if (write_cb_para->need_rsp) {
            sle_ssaps_send_write_rsp(server_id, conn_id, write_cb_para->request_id, (uint8_t)ERRCODE_SLE_SUCCESS);
        }
        printf("%s[%s] ------------------------------------------>4\r\n", LOG_TAG, __func__);
        return;
    }

    int handled = sle_server_recv_data(conn_id, write_cb_para->value, write_cb_para->length);

    if (write_cb_para->need_rsp) {
        sle_ssaps_send_write_rsp(server_id, conn_id, write_cb_para->request_id, (uint8_t)ERRCODE_SLE_SUCCESS);
    }

    if (handled <= 0) {
        printf("%s[%s] sle_server_send_notify_to_conn(default rsp msg:[null, 0])\r\n", LOG_TAG, __func__);
        sle_server_send_notify_to_conn(conn_id, NULL, 0);
    }
    printf("%s[%s] ------------------------------------------>5\r\n", LOG_TAG, __func__);
}

static errcode_t sle_ssaps_register_cbks(void)
{
    errcode_t ret = ERRCODE_SLE_FAIL;
#if USE_OHOS_API
    SsapsCallbacks ssaps_cbk     = {0};
    ssaps_cbk.addServiceCb       = ssaps_add_service_cbk;
    ssaps_cbk.addPropertyCb      = ssaps_add_property_cbk;
    ssaps_cbk.addDescriptorCb    = ssaps_add_descriptor_cbk;
    ssaps_cbk.startServiceCb     = ssaps_start_service_cbk;
    ssaps_cbk.deleteAllServiceCb = ssaps_delete_all_service_cbk;
    ssaps_cbk.mtuChangedCb       = ssaps_mtu_changed_cbk;
    ssaps_cbk.readRequestCb      = ssaps_read_request_cbk;
    ssaps_cbk.writeRequestCb     = ssaps_write_request_cbk;
    ret = SsapsRegisterCallbacks(&ssaps_cbk);
#else
    ssaps_callbacks_t ssaps_cbk     = {0};
    ssaps_cbk.add_service_cb        = ssaps_add_service_cbk;
    ssaps_cbk.add_property_cb       = ssaps_add_property_cbk;
    ssaps_cbk.add_descriptor_cb     = ssaps_add_descriptor_cbk;
    ssaps_cbk.start_service_cb      = ssaps_start_service_cbk;
    ssaps_cbk.delete_all_service_cb = ssaps_delete_all_service_cbk;
    ssaps_cbk.mtu_changed_cb        = ssaps_mtu_changed_cbk;
    ssaps_cbk.read_request_cb       = ssaps_read_request_cbk;
    ssaps_cbk.write_request_cb      = ssaps_write_request_cbk;
    ret = ssaps_register_callbacks(&ssaps_cbk);
#endif
    return ret;
}
////////////////////////////////////////////////////////


////////////////////////////////////////////////////////
errcode_t sle_server_send_notify_to_conn(uint16_t conn_id, const uint8_t *data, uint16_t len)
{
    errcode_t ret = ERRCODE_SLE_FAIL;
    uint16_t plen = len;

    memset_s(g_ntf_buf, NTF_BUF_LEN, 0, NTF_BUF_LEN);
    if (data == NULL || len == 0) {
        uint8_t local_address[SLE_ADDR_LEN] = {0};
        sle_server_get_local_address(local_address, SLE_ADDR_LEN);
        snprintf_s(g_ntf_buf, NTF_BUF_LEN, NTF_BUF_LEN-1,
                  "Default ACK_From_Server: conn_id[0x%02X] addr[%02X]\0",
                  conn_id, local_address[SLE_ADDR_LEN-1]);
        plen = strlen(g_ntf_buf);
    } else {
        if (plen > NTF_BUF_LEN) {
            plen = NTF_BUF_LEN;
        }
        if (memcpy_s(g_ntf_buf, NTF_BUF_LEN, data, plen) != EOK) {
            printf("%s[%s] payload copy failed: len[%d]\r\n", LOG_TAG, __func__, plen);
            return ERRCODE_SLE_FAIL;
        }
    }

    sle_uuid_t char_uuid = {0};
    sle_uuid_setu2(SLE_UUID_SERVER_NTF_REPORT, &char_uuid);

    ssaps_ntf_ind_by_uuid_t nbu = {0};
    nbu.uuid.len = char_uuid.len;
    if (memcpy_s(nbu.uuid.uuid, SLE_UUID_LEN, char_uuid.uuid, SLE_UUID_LEN) != EOK) {
        return ERRCODE_SLE_FAIL;
    }
    nbu.start_handle = g_service_handle;
    nbu.end_handle   = 0xFFFF;
    nbu.type         = SSAP_PROPERTY_TYPE_VALUE;
    nbu.value_len    = plen;
    nbu.value        = g_ntf_buf;

    printf("%s[%s] >>>> server_id[0x%02X] conn[0x%02X] len[%d]{%s}\r\n",
        LOG_TAG, __func__, g_server_id, conn_id, plen, (char*)g_ntf_buf);

    ret = ssaps_notify_indicate_by_uuid(g_server_id, conn_id, &nbu);
    if ((ret == ERRCODE_SUCC) || (ret == (errcode_t)ERRCODE_SLE_SUCCESS)) {
        return ERRCODE_SLE_SUCCESS;
    } else {
        printf("%s[%s] ssaps_notify_indicate_by_uuid failed: ret[0x%X]\r\n", LOG_TAG, __func__, ret);
    }

    ssaps_ntf_ind_t ind = {0};
    ind.handle    = g_property_handle;
    ind.type      = SSAP_PROPERTY_TYPE_VALUE;
    ind.value_len = plen;
    ind.value     = g_ntf_buf;

    ret = ssaps_notify_indicate(g_server_id, conn_id, &ind);
    if ((ret == ERRCODE_SUCC) || (ret == (errcode_t)ERRCODE_SLE_SUCCESS)) {
        return ERRCODE_SLE_SUCCESS;
    } else {
        printf("%s[%s] ssaps_notify_indicate failed: ret[0x%X]\r\n", LOG_TAG, __func__, ret);
    }

    return ret;
}

static void sle_connect_state_changed_cbk(uint16_t conn_id,
                                          const sle_addr_t *addr,
                                          sle_acb_state_t conn_state,
                                          sle_pair_state_t pair_state,
                                          sle_disc_reason_t disc_reason)
{
    printf("%s[%s] "
        "conn_id[0x%02X] conn_state[0x%02X] pair_state[0x%02X] disc_reason[0x%02X]\r\n",
        LOG_TAG, __func__, conn_id, conn_state, pair_state, disc_reason);
    printf("%s[%s] client addr[%02X:%02X:%02X:%02X:%02X:%02X]\r\n",
        LOG_TAG, __func__,
        addr->addr[0], addr->addr[1], addr->addr[2],
        addr->addr[3], addr->addr[4], addr->addr[5]);

    if (conn_state == SLE_ACB_STATE_CONNECTED) {  // 0x01: OH_SLE_ACB_STATE_CONNECTED
        int32_t idx = sle_server_conn_alloc(conn_id, addr);
        printf("%s[%s] SLE_ACB_STATE_CONNECTED: max_slots[%d] conn_count[%d] conn_slot[%d] client_addr[%02X]\r\n",
               LOG_TAG, __func__, sle_server_get_max_slots(), sle_server_get_conn_count(), idx, addr->addr[SLE_ADDR_LEN-1]);

        if (sle_server_get_conn_count() < sle_server_get_max_slots()) {
            sle_server_restart_announce();
        }
    } else if (conn_state == SLE_ACB_STATE_DISCONNECTED) {  // 0x02: OH_SLE_ACB_STATE_DISCONNECTED
        sle_server_conn_remove(conn_id);
        sle_server_restart_announce();
    }
}

static void sle_connect_param_update_req_cbk(uint16_t conn_id, errcode_t status,
                                             const sle_connection_param_update_req_t *param)
{
    printf("%s[%s] "
        "conn_id[0x%02X] status[0x%02X] interval_min[0x%02X] "
        "interval_max[0x%02X] max_latency[0x%02X] supervision_timeout[0x%02X]\r\n",
        LOG_TAG, __func__, conn_id, status, param->interval_min,
        param->interval_max, param->max_latency, param->supervision_timeout);
}

static void sle_connect_param_update_cbk(uint16_t conn_id, errcode_t status,
                                         const sle_connection_param_update_evt_t *param)
{
    printf("%s[%s] conn_id[0x%02X] status[0x%02X] "
        "interval[0x%02X] latency[0x%02X] supervision[0x%02X]\r\n",
        LOG_TAG, __func__, conn_id, status,
        param->interval, param->latency, param->supervision);
}

static void sle_auth_complete_cbk(uint16_t conn_id, const sle_addr_t *addr,
                                  errcode_t status, const sle_auth_info_evt_t *evt)
{
    printf("%s[%s] conn_id[0x%02X] status[0x%02X]\r\n", LOG_TAG, __func__, conn_id, status);
    printf("%s[%s] client addr[%02X:%02X:%02X:%02X:%02X:%02X]\r\n",
           LOG_TAG, __func__,
           addr->addr[0], addr->addr[1], addr->addr[2],
           addr->addr[3], addr->addr[4], addr->addr[5]);
    printf("%s[%s] link_key[0x%02X] crypto_algo[0x%02X] key_deriv_algo[0x%02X] integr_chk_ind[0x%02X]\r\n",
           LOG_TAG, __func__,
           evt->link_key, evt->crypto_algo, evt->key_deriv_algo, evt->integr_chk_ind);
}

static void sle_pair_complete_cbk(uint16_t conn_id, const sle_addr_t *addr, errcode_t status)
{
    printf("%s[%s] conn_id[0x%02x] status[0x%02x]\r\n",
            LOG_TAG, __func__, conn_id, status);
    printf("%s[%s] client addr[%02x:%02x:%02x:%02x:%02x:%02x]\r\n",
           LOG_TAG, __func__,
           addr->addr[0], addr->addr[1], addr->addr[2],
           addr->addr[3], addr->addr[4], addr->addr[5]);
}

static void sle_read_rssi_cbk(uint16_t conn_id, int8_t rssi, errcode_t status)
{
    printf("%s[%s] conn_id[0x%02X] rssi[0x%02X] status[0x%02X]\r\n",
        LOG_TAG, __func__, conn_id, rssi, status);
}

static errcode_t sle_conn_register_cbks(void)
{
    errcode_t ret = ERRCODE_SLE_FAIL;
#if USE_OHOS_API
    SleConnectionCallbacks conn_cbks  = {0};
    conn_cbks.connectStateChangedCb   = sle_connect_state_changed_cbk;
    conn_cbks.connectParamUpdateReqCb = sle_connect_param_update_req_cbk;
    conn_cbks.connectParamUpdateCb    = sle_connect_param_update_cbk;
    conn_cbks.authCompleteCb          = sle_auth_complete_cbk;
    conn_cbks.pairCompleteCb          = sle_pair_complete_cbk;
    conn_cbks.readRssiCb              = sle_read_rssi_cbk;
    ret = SleConnectionRegisterCallbacks(&conn_cbks);
#else
    sle_connection_callbacks_t conn_cbks  = {0};
    conn_cbks.connect_state_changed_cb    = sle_connect_state_changed_cbk;
    conn_cbks.connect_param_update_req_cb = sle_connect_param_update_req_cbk;
    conn_cbks.connect_param_update_cb     = sle_connect_param_update_cbk;
    conn_cbks.auth_complete_cb            = sle_auth_complete_cbk;
    conn_cbks.pair_complete_cb            = sle_pair_complete_cbk;
    conn_cbks.read_rssi_cb                = sle_read_rssi_cbk;
    ret = sle_connection_register_callbacks(&conn_cbks);
#endif
    return ret;
}
////////////////////////////////////////////////////////

static errcode_t sle_uuid_server_service_add(void)
{
    errcode_t ret = ERRCODE_SLE_FAIL;
    sle_uuid_t service_uuid = {0};
    sle_uuid_setu2(SLE_UUID_SERVER_SERVICE, &service_uuid);  // 星闪服务UUID: 0x2222
#if USE_OHOS_API
    ret = SsapsAddServiceSync(g_server_id, &service_uuid, 1, &g_service_handle);
#else
    ret = ssaps_add_service_sync(g_server_id, &service_uuid, 1, &g_service_handle);
#endif
    return ret;
}

#if USE_OHOS_API
static errcode_t sle_uuid_server_property_add(void)
{
    errcode_t ret = ERRCODE_SLE_FAIL;
    SsapsPropertyInfo property = {0};
    sle_uuid_setu2(SLE_UUID_SERVER_NTF_REPORT, &property.uuid);  // 星闪属性UUID: 0x2323
    property.permissions       = SLE_UUID_TEST_PROPERTIES;
    property.operateIndication = SLE_UUID_TEST_OPERATION_INDICATION;
    property.valueLen          = NTF_BUF_LEN;
    property.value             = (uint8_t *)osal_vmalloc(NTF_BUF_LEN);
    if (property.value == NULL) {
        return ERRCODE_SLE_FAIL;
    }
    if (memcpy_s(property.value, NTF_BUF_LEN, g_ntf_buf, NTF_BUF_LEN) != EOK) {
        osal_vfree(property.value);
        return ERRCODE_SLE_FAIL;
    }

    ret = SsapsAddPropertySync(g_server_id, g_service_handle, &property, &g_property_handle);
    if (ret != ERRCODE_SLE_SUCCESS) {
        printf("%s[%s] SsapsAddPropertySync failed: ret[0x%X]\r\n", LOG_TAG, __func__, ret);
        osal_vfree(property.value);
        return ERRCODE_SLE_FAIL;
    }

    uint8_t ntf_value[] = {0x03, 0x00};
    SsapsDescInfo descriptor = {0};
    sle_uuid_setu2(SLE_UUID_GATT_CCCD, &descriptor.uuid);  // 星闪属性UUID: 0x2902
    descriptor.permissions       = SLE_UUID_TEST_DESCRIPTOR;
    descriptor.type              = SSAP_DESCRIPTOR_CLIENT_CONFIGURATION;
    descriptor.operateIndication = SLE_UUID_CCCD_OPERATION_INDICATION;
    descriptor.valueLen          = (uint16_t)sizeof(ntf_value);
    descriptor.value             = (uint8_t *)osal_vmalloc(sizeof(ntf_value));

    if (descriptor.value == NULL) {
        osal_vfree(property.value);
        return ERRCODE_SLE_FAIL;
    }
    if (memcpy_s(descriptor.value, sizeof(ntf_value), ntf_value, sizeof(ntf_value)) != EOK) {
        osal_vfree(property.value);
        osal_vfree(descriptor.value);
        return ERRCODE_SLE_FAIL;
    }

    ret = SsapsAddDescriptorSync(g_server_id, g_service_handle, g_property_handle, &descriptor);
    if (ret != ERRCODE_SLE_SUCCESS) {
        printf("%s[%s] SsapsAddDescriptorSync failed: ret[0x%X]\r\n", LOG_TAG, __func__, ret);
        osal_vfree(property.value);
        osal_vfree(descriptor.value);
        return ERRCODE_SLE_FAIL;
    }

    osal_vfree(property.value);
    osal_vfree(descriptor.value);
    g_cccd_handle = (uint16_t)(g_property_handle + 1U);
    printf("%s[%s] property[0x%04X] cccd[0x%04X]\r\n",
           LOG_TAG, __func__, g_property_handle, g_cccd_handle);
    return ERRCODE_SLE_SUCCESS;
}
#else
static errcode_t sle_uuid_server_property_add(void)
{
    errcode_t ret = ERRCODE_SLE_FAIL;
    ssaps_property_info_t property = {0};
    sle_uuid_setu2(SLE_UUID_SERVER_NTF_REPORT, &property.uuid);  // 星闪属性UUID: 0x2323
    property.permissions        = SLE_UUID_TEST_PROPERTIES;
    property.operate_indication = SLE_UUID_TEST_OPERATION_INDICATION;
    property.value_len          = NTF_BUF_LEN;
    property.value              = (uint8_t *)osal_vmalloc(NTF_BUF_LEN);

    if (property.value == NULL) {
        return ERRCODE_SLE_FAIL;
    }
    if (memcpy_s(property.value, NTF_BUF_LEN, g_ntf_buf, NTF_BUF_LEN) != EOK) {
        osal_vfree(property.value);
        return ERRCODE_SLE_FAIL;
    }

    ret = ssaps_add_property_sync(g_server_id, g_service_handle, &property, &g_property_handle);
    if (ret != ERRCODE_SLE_SUCCESS) {
        printf("%s[%s] ssaps_add_property_sync failed: ret[0x%X]\r\n", LOG_TAG, __func__, ret);
        osal_vfree(property.value);
        return ERRCODE_SLE_FAIL;
    }

    uint8_t ntf_value[] = {0x03, 0x00};
    ssaps_desc_info_t  descriptor = {0};
    sle_uuid_setu2(SLE_UUID_GATT_CCCD, &descriptor.uuid);  // 星闪属性UUID: 0x2902
    descriptor.permissions        = SLE_UUID_TEST_DESCRIPTOR;
    descriptor.type               = SSAP_DESCRIPTOR_CLIENT_CONFIGURATION;
    descriptor.operate_indication = SLE_UUID_CCCD_OPERATION_INDICATION;
    descriptor.value_len          = (uint16_t)sizeof(ntf_value);
    descriptor.value              = (uint8_t *)osal_vmalloc(sizeof(ntf_value));

    if (descriptor.value == NULL) {
        osal_vfree(property.value);
        return ERRCODE_SLE_FAIL;
    }
    if (memcpy_s(descriptor.value, sizeof(ntf_value), ntf_value, sizeof(ntf_value)) != EOK) {
        osal_vfree(property.value);
        osal_vfree(descriptor.value);
        return ERRCODE_SLE_FAIL;
    }

    ret = ssaps_add_descriptor_sync(g_server_id, g_service_handle, g_property_handle, &descriptor);
    if (ret != ERRCODE_SLE_SUCCESS) {
        printf("%s[%s] ssaps add descriptor sync failed: ret[0x%X]\r\n", LOG_TAG, __func__, ret);
        osal_vfree(property.value);
        osal_vfree(descriptor.value);
        return ERRCODE_SLE_FAIL;
    }

    osal_vfree(property.value);
    osal_vfree(descriptor.value);
    g_cccd_handle = (uint16_t)(g_property_handle + 1U);
    printf("%s[%s] property[0x%04X] cccd[0x%04X]\r\n",
            LOG_TAG, __func__, g_property_handle, g_cccd_handle);
    return ERRCODE_SLE_SUCCESS;
}
#endif

static errcode_t sle_server_add(void)
{
    errcode_t ret = ERRCODE_SLE_FAIL;
    sle_uuid_t app_uuid = {0};

    printf("%s[%s] Begin:\r\n", LOG_TAG, __func__);

    app_uuid.len = UUID_LEN_2;
    if (memcpy_s(app_uuid.uuid, app_uuid.len, g_sle_uuid_len2, UUID_LEN_2) != EOK) {
        return ERRCODE_SLE_FAIL;
    }

    // 4-1. 注册 Server
    printf("%s[%s] Step[4-1] Register Server: UUID[%02X %02X]\r\n",
            LOG_TAG, __func__, app_uuid.uuid[0], app_uuid.uuid[1]);
    #if USE_OHOS_API
    ret = SsapsRegisterServer(&app_uuid, &g_server_id);
    #else
    ret = ssaps_register_server(&app_uuid, &g_server_id);
    #endif
    if (ret != ERRCODE_SLE_SUCCESS) {
        printf("%s[%s] Step[4-1] Register Server failed\r\n", LOG_TAG, __func__);
        return ret;
    }

    // 4-2. 添加 Server Service（星闪服务）
    printf("%s[%s] Step[4-2] Add Server Service\r\n", LOG_TAG, __func__);
    if (sle_uuid_server_service_add() != ERRCODE_SLE_SUCCESS) {
        printf("%s[%s] Step[4-2] Add Server Service failed\r\n", LOG_TAG, __func__);
        #if USE_OHOS_API
        SsapsUnregisterServer(g_server_id);
        #else
        ssaps_unregister_server(g_server_id);
        #endif
        return ERRCODE_SLE_FAIL;
    }

    // 4-3. 添加 Server Property+CCCD（星闪属性+客户端特征配置描述符）
    printf("%s[%s] Step[4-3] Add Server Property+CCCD\r\n", LOG_TAG, __func__);
    if (sle_uuid_server_property_add() != ERRCODE_SLE_SUCCESS) {
        printf("%s[%s] Step[4-3] Add Server Property+CCCD failed\r\n", LOG_TAG, __func__);
        #if USE_OHOS_API
        SsapsUnregisterServer(g_server_id);
        #else
        ssaps_unregister_server(g_server_id);
        #endif
        return ERRCODE_SLE_FAIL;
    }
    printf("%s[%s] server_id[0x%02X] service_handle[0x%02X] property_handle[0x%02X]\r\n",
           LOG_TAG, __func__, g_server_id, g_service_handle, g_property_handle);

    // 4-4. 启动 Server Service（星闪服务）
    printf("%s[%s] Step[4-4] Start Server Service\r\n", LOG_TAG, __func__);
    #if USE_OHOS_API
    ret = SsapsStartService(g_server_id, g_service_handle);
    #else
    ret = ssaps_start_service(g_server_id, g_service_handle);
    #endif
    if (ret != ERRCODE_SLE_SUCCESS) {
        printf("%s[%s] Step[4-4] Start Server Service failed\r\n", LOG_TAG, __func__);
        return ERRCODE_SLE_FAIL;
    }
    printf("%s[%s] End. OK\r\n", LOG_TAG, __func__);

    return ERRCODE_SLE_SUCCESS;
}

errcode_t sle_enable_server_cbk(void)
{
    errcode_t ret = ERRCODE_SLE_FAIL;
    ret = sle_server_add();
    if (ret != ERRCODE_SLE_SUCCESS) {
        printf("%s[%s] sle_server_add failed: ret[0x%X]\r\n", LOG_TAG, __func__, ret);
        return ret;
    }
    ret = sle_server_adv_init();
    if (ret != ERRCODE_SLE_SUCCESS) {
        printf("%s[%s] sle_server_adv_init failed: ret[0x%X]\r\n", LOG_TAG, __func__, ret);
        return ret;
    }
    return ERRCODE_SLE_SUCCESS;
}

errcode_t sle_server_broadcast_msg(const uint8_t *data, uint16_t len)
{
    errcode_t ret = ERRCODE_SLE_FAIL;
    errcode_t last = ERRCODE_SLE_FAIL;
    uint8_t sent = 0;

//  printf("%s[%s] conn_cnt[%d] len[%d]{%s} \r\n",
//         LOG_TAG, __func__, sle_server_get_conn_count(), len, (char*)data);

    for (uint32_t i = 0; i < sle_server_get_max_slots(); i++) {
        if (g_server_conn_slots[i].in_use == 0) {
            continue;
        }
        ret = sle_server_send_notify_to_conn(g_server_conn_slots[i].conn_id, data, len);
        if (ret == ERRCODE_SLE_SUCCESS) {
            last = ERRCODE_SLE_SUCCESS;
            sent++;
        }
    }
    if (sent == 0) {
        printf("%s[%s] No active conn\r\n", LOG_TAG, __func__);
    }
    return last;
}

__attribute__((weak)) int sle_server_recv_data(uint16_t conn_id, uint8_t *data, uint16_t len)
{
    if ((data == NULL) || (len == 0)) {
        printf("%s[%s] invalid args\r\n", LOG_TAG, __func__);
        return 0;
    }

    printf("%s[%s] [weak] conn_id[0x%02X] [%d]:%s\r\n", LOG_TAG, __func__, conn_id, len, (char*)data);
    return len;
}

__attribute__((weak)) int sle_server_send_data(uint8_t *data, uint16_t len)
{
    if ((data == NULL) || (len == 0)){
        char buff[24] = {0};
        snprintf(buff, 24, "ACK_From_Server: OK\0");
        printf("%s[%s] [weak] [%d]:%s\r\n", LOG_TAG, __func__, strlen(buff), (char*)buff);
        return sle_server_broadcast_msg((uint8_t *)buff, strlen(buff));
    }

    printf("%s[%s] [weak] [%d]:%s\r\n", LOG_TAG, __func__, len, (char*)data);
    return sle_server_broadcast_msg(data, len);
}


errcode_t sle_server_init(void)
{
    errcode_t ret = ERRCODE_SLE_FAIL;

    ret = sle_announce_register_cbks();    // sle_server_adv.c
    if (ret != ERRCODE_SLE_SUCCESS) {
        printf("%s[%s] sle_announce_register_cbks failed: ret[0x%X]\r\n", LOG_TAG, __func__, ret);
        return ret;
    }

    ret = sle_conn_register_cbks();
    if (ret != ERRCODE_SLE_SUCCESS) {
        printf("%s[%s] sle_conn_register_cbks failed: ret[0x%X]\r\n", LOG_TAG, __func__, ret);
        return ret;
    }

    ret = sle_ssaps_register_cbks();
    if (ret != ERRCODE_SLE_SUCCESS) {
        printf("%s[%s] sle_ssaps_register_cbks failed: ret[0x%X]\r\n", LOG_TAG, __func__, ret);
        return ret;
    }

#if USE_OHOS_API
    if (EnableSle()) {
        ret = ERRCODE_SLE_SUCCESS;
    } else {
        ret = ERRCODE_SLE_FAIL;
    }
#else
    ret = enable_sle();
#endif

    if ((ret == ERRCODE_SLE_SUCCESS) ||
        (ret == ERRCODE_SLE_CONTINUE) ||
        (ret == ERRCODE_SLE_DIRECT_RETURN)) {
        printf("%s[%s] enable sle success!\r\n", LOG_TAG, __func__);
        ret = ERRCODE_SLE_SUCCESS;
    } else {
        printf("%s[%s] enable sle failed!\r\n", LOG_TAG, __func__);
        ret = ERRCODE_SLE_FAIL;
    }

    return ret;
}
