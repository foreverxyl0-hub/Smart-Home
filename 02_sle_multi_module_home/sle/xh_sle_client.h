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

#ifndef __SLE_CLIENT_H__
#define __SLE_CLIENT_H__

#include <stdbool.h>

#include "sle_common.h"
#include "sle_connection_manager.h"
#include "sle_device_discovery.h"
#include "sle_errcode.h"
#include "sle_ssap_client.h"

#if USE_OHOS_API
#include "ohos_sle_common.h"
#include "ohos_sle_connection_manager.h"
#include "ohos_sle_device_discovery.h"
#include "ohos_sle_errcode.h"
#include "ohos_sle_ssap_client.h"
#include "ohos_sle_ssap_server.h"
#endif

#ifndef SLE_SERVER_NAME
#define SLE_SERVER_NAME  "XH_TH_FAN"
#endif

#ifndef SLE_1_CLIENT_M_SERVER
#define SLE_1_CLIENT_M_SERVER  1
#endif

#ifndef SLE_CLIENT_TX_BUF_LEN
#define SLE_CLIENT_TX_BUF_LEN 64
#endif

typedef enum {
    SLE_CLIENT_SLOT_IDLE = 0,
    SLE_CLIENT_SLOT_CONNECTING,
    SLE_CLIENT_SLOT_CONNECTED,
} sle_client_slot_state_t;

typedef struct {
    uint16_t conn_id;
    uint16_t property_handle;
    uint16_t ccc_handle;
    uint8_t property_ready;
    uint8_t ntf_subscribed;
    uint8_t ccc_desc_pending;
    uint8_t ccc_value_fallback_tried;
    uint8_t ccc_prime_pending;
    uint8_t ccc_value_pending;
    uint8_t ccc_verify_pending;
    uint8_t ccc_verify_retries;
    ssapc_find_service_result_t find_service_result;
} sle_conn_and_service_t;

typedef struct {
    sle_client_slot_state_t state;
    uint8_t module_id;
    uint8_t tx_pending;
    uint8_t tx_fallback_tried;
    uint8_t tx_seq;
    uint16_t tx_len;
    uint8_t tx_buf[SLE_CLIENT_TX_BUF_LEN];
    sle_addr_t server_addr;
    sle_conn_and_service_t service;
} sle_client_conn_slot_t;

typedef struct {
    uint8_t found;
    uint8_t module_id;
    uint8_t slot;
    uint16_t conn_id;
    uint16_t property_handle;
    uint16_t ccc_handle;
    uint8_t property_ready;
    uint8_t ntf_subscribed;
    uint8_t addr[SLE_ADDR_LEN];
} sle_client_module_diag_t;

void sle_client_init(void);
void sle_start_scan(void);

sle_client_conn_slot_t *sle_get_client_slots(void);
sle_conn_and_service_t *sle_get_conn_and_service(void);
ssapc_write_param_t *sle_get_send_param(void);
uint32_t sle_client_get_max_slots(void);
uint32_t sle_client_get_conn_count(void);
uint32_t sle_client_get_ready_count(void);
uint32_t sle_client_get_ntf_subscribed_count(void);
bool sle_client_get_module_diag(uint8_t module_id, sle_client_module_diag_t *out);
bool sle_client_module_tx_pending(uint8_t module_id, uint8_t seq);
errcode_t sle_client_retry_pending_write_cmd(uint8_t module_id);
errcode_t sle_client_broadcast_msg(const uint8_t *data, uint16_t len);
errcode_t sle_client_send_msg_to_module(uint8_t module_id, const uint8_t *data, uint16_t len);
void sle_set_server_name(char *name);
void sle_client_set_local_address(uint8_t* addr, uint8_t len);
void sle_client_get_server_address(uint16_t conn_id, uint8_t* addr, uint8_t len);
uint8_t sle_client_get_module_id(uint16_t conn_id);

int sle_client_send_data(uint8_t *data, uint16_t len);
int sle_client_recv_data(uint16_t conn_id, uint8_t *data, uint16_t len);

#endif  //  __SLE_CLIENT_H__
