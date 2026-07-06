#ifndef XH_SLE_MODULE_SERVER_H
#define XH_SLE_MODULE_SERVER_H

#include <stdint.h>
#include "errcode.h"

typedef void (*xh_module_control_cb_t)(uint16_t conn_id, const uint8_t *data, uint16_t len);

errcode_t xh_sle_module_server_init(uint8_t module_id, xh_module_control_cb_t cb);
errcode_t xh_sle_module_server_report(const uint8_t *data, uint16_t len);
errcode_t xh_sle_module_server_ack(uint8_t seq, uint8_t module_id, uint16_t err);

#endif
