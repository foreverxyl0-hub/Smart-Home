#include "xh_sle_module_server.h"

#include <stddef.h>
#include <stdio.h>

#include "xh_sle_proto.h"
#include "xh_sle_server.h"
#include "xh_sle_server_adv.h"

static uint8_t g_module_id;
static xh_module_control_cb_t g_control_cb;

errcode_t xh_sle_module_server_init(uint8_t module_id, xh_module_control_cb_t cb)
{
    g_module_id = module_id;
    g_control_cb = cb;

#if USE_CUSTOM_MAC_ADDR
    uint8_t server_addr[SLE_ADDR_LEN] = {0xA0, 0xBB, 0xCC, 0xDD, 0xEE, module_id};
    sle_server_set_local_address(server_addr, SLE_ADDR_LEN);
#else
    sle_server_set_local_address(NULL, 0);
#endif
    errcode_t ret = sle_server_init();
    printf("[xh_module_server] init module=0x%02X ret=0x%x\r\n", module_id, (unsigned int)ret);
    return ret;
}

errcode_t xh_sle_module_server_report(const uint8_t *data, uint16_t len)
{
    if (data == NULL || len == 0) {
        return ERRCODE_INVALID_PARAM;
    }
    return sle_server_broadcast_msg(data, len);
}

errcode_t xh_sle_module_server_ack(uint8_t seq, uint8_t module_id, uint16_t err)
{
    uint8_t payload[XH_PROTO_MAX_LEN] = {0};
    uint16_t len = 0;
    if (!xh_proto_pack_ack(payload, sizeof(payload), &len, seq, module_id, err)) {
        return ERRCODE_FAIL;
    }
    return xh_sle_module_server_report(payload, len);
}

int sle_server_recv_data(uint16_t conn_id, uint8_t *data, uint16_t len)
{
    if (data == NULL || len == 0) {
        return 0;
    }
    printf("[xh_module_server] recv module=0x%02X conn=0x%02X len=%u\r\n",
        g_module_id, conn_id, (unsigned int)len);
    if (g_control_cb != NULL) {
        g_control_cb(conn_id, data, len);
    }
    return len;
}
