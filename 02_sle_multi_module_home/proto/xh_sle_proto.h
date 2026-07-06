#ifndef XH_SLE_PROTO_H
#define XH_SLE_PROTO_H

#include <stdbool.h>
#include <stdint.h>

#define XH_PROTO_MAGIC0 'X'
#define XH_PROTO_MAGIC1 'H'
#define XH_PROTO_VERSION 1U
#define XH_PROTO_HDR_LEN 7U
#define XH_PROTO_MAX_LEN 64U

typedef enum {
    XH_PROTO_MSG_HELLO = 0x01,
    XH_PROTO_MSG_REPORT = 0x02,
    XH_PROTO_MSG_CONTROL = 0x03,
    XH_PROTO_MSG_ACK = 0x04,
    XH_PROTO_MSG_HEARTBEAT = 0x05,
} xh_proto_msg_type_t;

typedef enum {
    XH_TLV_ONLINE = 0x01,
    XH_TLV_VALID = 0x02,
    XH_TLV_WARMUP = 0x03,
    XH_TLV_TEMP100 = 0x10,
    XH_TLV_HUMI100 = 0x11,
    XH_TLV_SMOKE_ALARM = 0x12,
    XH_TLV_RAW_VALUE = 0x13,
    XH_TLV_LUX100 = 0x14,
    XH_TLV_PRESENCE = 0x15,
    XH_TLV_ADC_VALID = 0x16,
    XH_TLV_ADC_RAW = 0x17,
    XH_TLV_ADC_AVG = 0x18,
    XH_TLV_SWITCH = 0x20,
    XH_TLV_FAN_LEVEL = 0x21,
    XH_TLV_ALARM_MODE = 0x22,
    XH_TLV_TIMEOUT_PROTECTED = 0x23,
    XH_TLV_RGB = 0x26,
    XH_TLV_GPIO_OUTPUT = 0x28,
    XH_TLV_ERROR_CODE = 0x30,
    XH_TLV_AGE_MS = 0x31,
    XH_TLV_ADC_ERROR = 0x32,
    XH_TLV_SCENE_MODE = 0x40,
    XH_TLV_SCENE_CAUSE = 0x41,
    XH_TLV_SCENE_FLAGS = 0x42,
} xh_tlv_type_t;

typedef enum {
    XH_ACK_OK = 0,
    XH_ACK_INVALID = 1,
    XH_ACK_UNSUPPORTED = 2,
    XH_ACK_OFFLINE = 3,
    XH_ACK_SEND_FAIL = 4,
} xh_ack_code_t;

typedef struct {
    uint8_t ver;
    uint8_t seq;
    uint8_t module_id;
    uint8_t msg;
    uint8_t tlv_len;
    const uint8_t *tlv;
} xh_proto_msg_t;

uint8_t xh_proto_next_seq(void);
bool xh_proto_begin(uint8_t *out, uint16_t cap, uint8_t seq, uint8_t module_id, uint8_t msg);
bool xh_proto_put_u8(uint8_t *out, uint16_t cap, uint16_t *len, uint8_t type, uint8_t value);
bool xh_proto_put_u16(uint8_t *out, uint16_t cap, uint16_t *len, uint8_t type, uint16_t value);
bool xh_proto_put_u32(uint8_t *out, uint16_t cap, uint16_t *len, uint8_t type, uint32_t value);
bool xh_proto_put_i32(uint8_t *out, uint16_t cap, uint16_t *len, uint8_t type, int32_t value);
bool xh_proto_put_bytes(uint8_t *out, uint16_t cap, uint16_t *len, uint8_t type,
                        const uint8_t *value, uint8_t value_len);
bool xh_proto_finish(uint8_t *out, uint16_t cap, uint16_t *len);
bool xh_proto_decode(const uint8_t *data, uint16_t len, xh_proto_msg_t *out);
bool xh_proto_get_u8(const xh_proto_msg_t *msg, uint8_t type, uint8_t *value);
bool xh_proto_get_u16(const xh_proto_msg_t *msg, uint8_t type, uint16_t *value);
bool xh_proto_get_u32(const xh_proto_msg_t *msg, uint8_t type, uint32_t *value);
bool xh_proto_get_i32(const xh_proto_msg_t *msg, uint8_t type, int32_t *value);
bool xh_proto_get_bytes(const xh_proto_msg_t *msg, uint8_t type,
                        const uint8_t **value, uint8_t *value_len);
bool xh_proto_pack_ack(uint8_t *out, uint16_t cap, uint16_t *len,
                       uint8_t seq, uint8_t module_id, uint16_t err);

#endif
