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

#include <stdbool.h>
#include <string.h>

#include "sle_device_discovery.h"
#include "sle_connection_manager.h"
#include "sle_errcode.h"
#include "sle_ssap_client.h"
#include "sle_ssap_server.h"

#include "ohos_sle_common.h"
#include "ohos_sle_connection_manager.h"
#include "ohos_sle_device_discovery.h"
#include "ohos_sle_errcode.h"
#include "ohos_sle_ssap_client.h"
#include "ohos_sle_ssap_server.h"

sle_announce_seek_callbacks_t g_seek_cbk = {0};
sle_connection_callbacks_t g_connect_cbk = {0};
ssaps_callbacks_t g_ssaps_cbk = {0};

__attribute__((weak)) bool EnableSle(void)
{
    errcode_t ret = enable_sle();
    if ((ret == ERRCODE_SLE_SUCCESS) ||
        (ret == ERRCODE_SLE_CONTINUE) ||
        (ret == ERRCODE_SLE_DIRECT_RETURN)) {
        return true;
    } else {
        return false;
    }
}

__attribute__((weak)) bool DisableSle(void)
{
    errcode_t ret = disable_sle();
    if ((ret == ERRCODE_SLE_SUCCESS) ||
        (ret == ERRCODE_SLE_CONTINUE) ||
        (ret == ERRCODE_SLE_DIRECT_RETURN)) {
        return true;
    } else {
        return false;
    }
}

__attribute__((weak)) ErrCodeType SleSetLocalAddr(SleAddr *addr)
{
    return (ErrCodeType)sle_set_local_addr((sle_addr_t *)addr);
}

__attribute__((weak)) ErrCodeType SleGetLocalAddr(SleAddr *addr)
{
    return (ErrCodeType)sle_get_local_addr((sle_addr_t *)addr);
}

__attribute__((weak)) ErrCodeType SleSetLocalName(const uint8_t *name, uint8_t len)
{
    return (ErrCodeType)sle_set_local_name(name, len);
}

__attribute__((weak)) ErrCodeType SleGetLocalName(uint8_t *name, uint8_t *len)
{
    return (ErrCodeType)sle_get_local_name(name, len);
}

__attribute__((weak)) ErrCodeType SleSetAnnounceData(uint8_t announce_id, const SleAnnounceData *data)
{
    return (ErrCodeType)sle_set_announce_data(announce_id, (sle_announce_data_t *)data);
}

__attribute__((weak)) ErrCodeType SleSetAnnounceParam(uint8_t announce_id, const SleAnnounceParam *param)
{
    return (ErrCodeType)sle_set_announce_param(announce_id, (sle_announce_param_t *)param);
}

__attribute__((weak)) ErrCodeType SleStartAnnounce(uint8_t announce_id)
{
    return (ErrCodeType)sle_start_announce(announce_id);
}

__attribute__((weak)) ErrCodeType SleStopAnnounce(uint8_t announce_id)
{
    return (ErrCodeType)sle_stop_announce(announce_id);
}

__attribute__((weak)) ErrCodeType SleSetSeekParam(SleSeekParam *param)
{
    return (ErrCodeType)sle_set_seek_param((sle_seek_param_t *)param);
}

__attribute__((weak)) ErrCodeType SleStartSeek(void)
{
    return (ErrCodeType)sle_start_seek();
}

__attribute__((weak)) ErrCodeType SleStopSeek(void)
{
    return (ErrCodeType)sle_stop_seek();
}

__attribute__((weak)) ErrCodeType SleAnnounceSeekRegisterCallbacks(SleAnnounceSeekCallbacks *func)
{
    (void)memset(&g_seek_cbk, 0, sizeof(sle_announce_seek_callbacks_t));
    g_seek_cbk.sle_enable_cb        = (sle_enable_callback)           (func->sleEnableCb);
    g_seek_cbk.sle_disable_cb       = (sle_disable_callback)          (func->sleDisableCb);
    g_seek_cbk.announce_enable_cb   = (sle_announce_enable_callback)  (func->sleAnnounceEnableCb);
    g_seek_cbk.announce_disable_cb  = (sle_announce_disable_callback) (func->sleAnnounceDisableCb);
    g_seek_cbk.announce_terminal_cb = (sle_announce_terminal_callback)(func->sleAnnounceTerminalCb);
    g_seek_cbk.seek_result_cb       = (sle_seek_result_callback)      (func->sleSeekResultCb);
    return (ErrCodeType)sle_announce_seek_register_callbacks(&g_seek_cbk);
}

__attribute__((weak)) ErrCodeType SleConnectRemoteDevice(const SleAddr *addr)
{
    return (ErrCodeType)sle_connect_remote_device((sle_addr_t *)addr);
}

__attribute__((weak)) ErrCodeType SleDisconnectRemoteDevice(const SleAddr *addr)
{
    return (ErrCodeType)sle_disconnect_remote_device((sle_addr_t *)addr);
}

__attribute__((weak)) ErrCodeType SleUpdateConnectParam(SleConnectionParamUpdate *params)
{
    return (ErrCodeType)sle_update_connect_param((sle_connection_param_update_t *)params);
}

__attribute__((weak)) ErrCodeType SlePairRemoteDevice(const SleAddr *addr)
{
    return (ErrCodeType)sle_pair_remote_device((sle_addr_t *)addr);
}

__attribute__((weak)) ErrCodeType SleRemovePairedRemoteDevice(const SleAddr *addr)
{
    return (ErrCodeType)sle_remove_paired_remote_device((sle_addr_t *)addr);
}

__attribute__((weak)) ErrCodeType SleRemoveAllPairs(void)
{
    return (ErrCodeType)sle_remove_all_pairs();
}

__attribute__((weak)) ErrCodeType SleGetPairedDevicesNum(uint16_t *number)
{
    return (ErrCodeType)sle_get_paired_devices_num(number);
}

__attribute__((weak)) ErrCodeType SleGetPairedDevices(SleAddr *addr, uint16_t *number)
{
    return (ErrCodeType)sle_get_paired_devices((sle_addr_t *)addr, number);
}

__attribute__((weak)) ErrCodeType SleGetPairState(const SleAddr *addr, uint8_t *state)
{
    return (ErrCodeType)sle_get_pair_state((sle_addr_t *)addr, state);
}

__attribute__((weak)) ErrCodeType SleReadRemoteDeviceRssi(uint16_t conn_id)
{
    return (ErrCodeType)sle_read_remote_device_rssi(conn_id);
}

__attribute__((weak)) ErrCodeType SleConnectionRegisterCallbacks(SleConnectionCallbacks *func)
{
    (void)memset(&g_connect_cbk, 0, sizeof(sle_connection_callbacks_t));
    g_connect_cbk.connect_state_changed_cb    = (sle_connect_state_changed_callback)   func->connectStateChangedCb;
    g_connect_cbk.connect_param_update_req_cb = (sle_connect_param_update_req_callback)func->connectParamUpdateReqCb;
    g_connect_cbk.connect_param_update_cb     = (sle_connect_param_update_callback)    func->connectParamUpdateCb;
    g_connect_cbk.auth_complete_cb            = (sle_auth_complete_callback)           func->authCompleteCb;
    g_connect_cbk.pair_complete_cb            = (sle_pair_complete_callback)           func->pairCompleteCb;
    g_connect_cbk.read_rssi_cb                = (sle_read_rssi_callback)               func->readRssiCb;
    return (ErrCodeType)sle_connection_register_callbacks(&g_connect_cbk);
}

__attribute__((weak)) errcode_t SsapcRegisterClient(SleUuid *app_uuid, uint8_t *client_id)
{
    return ssapc_register_client((sle_uuid_t *)app_uuid, client_id);
}

__attribute__((weak)) errcode_t SsapcUnregisterClient(uint8_t client_id)
{
    return ssapc_unregister_client(client_id);
}

__attribute__((weak)) errcode_t SsapcFindStructure(uint8_t client_id, uint16_t conn_id, SsapcFindStructureParam *param)
{
    return ssapc_find_structure(client_id, conn_id, (ssapc_find_structure_param_t *)param);
}

__attribute__((weak)) errcode_t SsapcReadReq(uint8_t client_id, uint16_t conn_id, uint16_t handle, uint8_t type)
{
    return ssapc_read_req(client_id, conn_id, handle, type);
}

__attribute__((weak)) ErrCodeType SsapcWriteReq(uint8_t client_id, uint16_t conn_id, SsapcWriteParam *param)
{
    return (ErrCodeType)ssapc_write_req(client_id, conn_id, (ssapc_write_param_t *)param);
}

__attribute__((weak)) errcode_t SsapWriteReq(uint8_t client_id, uint16_t conn_id, ssapc_write_param_t *param)
{
    return ssapc_write_req(client_id, conn_id, param);
}

__attribute__((weak)) errcode_t SsapcExchangeInfoReq(uint8_t client_id, uint16_t conn_id, SsapcExchangeInfo *param)
{
    return ssapc_exchange_info_req(client_id, conn_id, (ssap_exchange_info_t *)param);
}

__attribute__((weak)) ErrCodeType SsapsRegisterServer(SleUuid *app_uuid, uint8_t *server_id)
{
    return (ErrCodeType)ssaps_register_server((sle_uuid_t *)app_uuid, server_id);
}

__attribute__((weak)) ErrCodeType SsapsUnregisterServer(uint8_t server_id)
{
    return (ErrCodeType)ssaps_unregister_server(server_id);
}

__attribute__((weak)) ErrCodeType SsapsAddServiceSync(uint8_t server_id, SleUuid *service_uuid, bool is_primary, uint16_t *handle)
{
    return (ErrCodeType)ssaps_add_service_sync(server_id, (sle_uuid_t *)service_uuid, is_primary, handle);
}

__attribute__((weak)) ErrCodeType SsapsAddPropertySync(uint8_t server_id, uint16_t service_handle, SsapsPropertyInfo *property, uint16_t *handle)
{
    return (ErrCodeType)ssaps_add_property_sync(server_id, service_handle, (ssaps_property_info_t *)property, handle);
}

__attribute__((weak)) ErrCodeType SsapsAddDescriptorSync(uint8_t server_id, uint16_t service_handle, uint16_t property_handle, SsapsDescInfo *descriptor)
{
    return (ErrCodeType)ssaps_add_descriptor_sync(server_id, service_handle, property_handle, (ssaps_desc_info_t *)descriptor);
}

__attribute__((weak)) ErrCodeType SsapsStartService(uint8_t server_id, uint16_t service_handle)
{
    return (ErrCodeType)ssaps_start_service(server_id, service_handle);
}

__attribute__((weak)) ErrCodeType SsapsRegisterCallbacks(SsapsCallbacks *func)
{
    (void)memset(&g_ssaps_cbk, 0, sizeof(ssaps_callbacks_t));
    g_ssaps_cbk.add_service_cb        = (ssaps_add_service_callback)       func->addServiceCb;
    g_ssaps_cbk.add_property_cb       = (ssaps_add_property_callback)      func->addPropertyCb;
    g_ssaps_cbk.add_descriptor_cb     = (ssaps_add_descriptor_callback)    func->addDescriptorCb;
    g_ssaps_cbk.start_service_cb      = (ssaps_start_service_callback)     func->startServiceCb;
    g_ssaps_cbk.delete_all_service_cb = (ssaps_delete_all_service_callback)func->deleteAllServiceCb;
    g_ssaps_cbk.read_request_cb       = (ssaps_read_request_callback)      func->readRequestCb;
    g_ssaps_cbk.write_request_cb      = (ssaps_write_request_callback)     func->writeRequestCb;
    g_ssaps_cbk.mtu_changed_cb        = (ssaps_mtu_changed_callback)       func->mtuChangedCb;
    return (ErrCodeType)ssaps_register_callbacks(&g_ssaps_cbk);
}

__attribute__((weak)) ErrCodeType SsapsNotifyIndicate(uint8_t server_id, uint16_t conn_id, SsapsNtfInd *param)
{
    return (ErrCodeType)ssaps_notify_indicate(server_id, conn_id, (ssaps_ntf_ind_t *)param);
}

__attribute__((weak)) ErrCodeType SsapsSendResponse(uint8_t server_id, uint16_t conn_id, SsapsSendRsp *param)
{
    return (ErrCodeType)ssaps_send_response(server_id, conn_id, (ssaps_send_rsp_t *)param);
}
