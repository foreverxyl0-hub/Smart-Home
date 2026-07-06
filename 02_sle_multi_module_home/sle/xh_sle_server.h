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

#ifndef __SLE_SERVER_H__
#define __SLE_SERVER_H__

#include <stdint.h>
#include "errcode.h"

#include "sle_common.h"
#include "sle_connection_manager.h"
#include "sle_device_discovery.h"
#include "sle_errcode.h"
#include "sle_ssap_server.h"

#if USE_OHOS_API  // [1]:OH API/[0]:SDK API
#include "ohos_sle_common.h"
#include "ohos_sle_connection_manager.h"
#include "ohos_sle_device_discovery.h"
#include "ohos_sle_errcode.h"
#include "ohos_sle_ssap_client.h"
#include "ohos_sle_ssap_server.h"
#endif

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

#define NAME_MAX_LENGTH               32
#ifndef SLE_SERVER_NAME
#define SLE_SERVER_NAME               "XH_TH_FAN"
#endif

#ifndef SLE_N_CLIENT_1_SERVER
#define SLE_N_CLIENT_1_SERVER          1
#endif

/* Advertise Handle */
#define SLE_ADV_HANDLE_DEFAULT         1

/* Service UUID */
#define SLE_UUID_SERVER_SERVICE        0x2222

/* Property UUID */
#define SLE_UUID_SERVER_NTF_REPORT     0x2323

/* Client Characteristic Configuration */
#define SLE_UUID_GATT_CCCD             0x2902

/* Property Property */
#define SLE_UUID_TEST_PROPERTIES  (SSAP_PERMISSION_READ | SSAP_PERMISSION_WRITE)

/* Keep this aligned with the verified 01 SHT30 server: value supports read/write/notify. */
#define SLE_UUID_TEST_OPERATION_INDICATION                                        \
    (SSAP_OPERATE_INDICATION_BIT_READ | SSAP_OPERATE_INDICATION_BIT_WRITE |       \
     SSAP_OPERATE_INDICATION_BIT_NOTIFY)

#define SLE_UUID_CCCD_OPERATION_INDICATION                                        \
    (SSAP_OPERATE_INDICATION_BIT_READ | SSAP_OPERATE_INDICATION_BIT_WRITE)

/* Descriptor Property */
#define SLE_UUID_TEST_DESCRIPTOR   (SSAP_PERMISSION_READ | SSAP_PERMISSION_WRITE)

typedef struct {
    uint16_t conn_id;
    uint8_t  in_use;
    uint8_t  cccd_live[2];
    sle_addr_t client_addr;
} sle_server_conn_slot_t;

errcode_t sle_server_init(void);
uint32_t sle_server_get_max_slots(void);
uint32_t sle_server_get_conn_count(void);
errcode_t sle_server_broadcast_msg(const uint8_t *data, uint16_t len);
errcode_t sle_server_send_notify_to_conn(uint16_t conn_id, const uint8_t *data, uint16_t len);
errcode_t sle_enable_server_cbk(void);
errcode_t sle_server_get_client_address(uint16_t conn_id, uint8_t* addr, uint8_t len);

int sle_server_send_data(uint8_t *data, uint16_t len);
int sle_server_recv_data(uint16_t conn_id, uint8_t *data, uint16_t len);

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif  //  __SLE_SERVER_H__
