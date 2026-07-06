#include "xh_sle_proto.h"

#include <string.h>

static uint8_t g_xh_proto_seq;

uint8_t xh_proto_next_seq(void)
{
    g_xh_proto_seq++;
    if (g_xh_proto_seq == 0) {
        g_xh_proto_seq = 1;
    }
    return g_xh_proto_seq;
}

static void xh_put_le16(uint8_t *out, uint16_t v)
{
    out[0] = (uint8_t)(v & 0xFFU);
    out[1] = (uint8_t)((v >> 8) & 0xFFU);
}

static void xh_put_le32(uint8_t *out, int32_t v)
{
    uint32_t u = (uint32_t)v;
    out[0] = (uint8_t)(u & 0xFFU);
    out[1] = (uint8_t)((u >> 8) & 0xFFU);
    out[2] = (uint8_t)((u >> 16) & 0xFFU);
    out[3] = (uint8_t)((u >> 24) & 0xFFU);
}

static void xh_put_ule32(uint8_t *out, uint32_t v)
{
    out[0] = (uint8_t)(v & 0xFFU);
    out[1] = (uint8_t)((v >> 8) & 0xFFU);
    out[2] = (uint8_t)((v >> 16) & 0xFFU);
    out[3] = (uint8_t)((v >> 24) & 0xFFU);
}

static uint16_t xh_get_le16(const uint8_t *p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static int32_t xh_get_le32(const uint8_t *p)
{
    uint32_t v = (uint32_t)p[0] |
                 ((uint32_t)p[1] << 8) |
                 ((uint32_t)p[2] << 16) |
                 ((uint32_t)p[3] << 24);
    return (int32_t)v;
}

static uint32_t xh_get_ule32(const uint8_t *p)
{
    return (uint32_t)p[0] |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

bool xh_proto_begin(uint8_t *out, uint16_t cap, uint8_t seq, uint8_t module_id, uint8_t msg)
{
    if (out == NULL || cap < XH_PROTO_HDR_LEN) {
        return false;
    }
    out[0] = (uint8_t)XH_PROTO_MAGIC0;
    out[1] = (uint8_t)XH_PROTO_MAGIC1;
    out[2] = XH_PROTO_VERSION;
    out[3] = seq;
    out[4] = module_id;
    out[5] = msg;
    out[6] = 0;
    return true;
}

static bool xh_proto_put_raw(uint8_t *out, uint16_t cap, uint16_t *len,
                             uint8_t type, const uint8_t *value, uint8_t value_len)
{
    if (out == NULL || len == NULL || value == NULL || *len < XH_PROTO_HDR_LEN) {
        return false;
    }
    if ((uint16_t)(*len + 2U + value_len) > cap) {
        return false;
    }
    out[*len] = type;
    out[*len + 1U] = value_len;
    (void)memcpy(&out[*len + 2U], value, value_len);
    *len = (uint16_t)(*len + 2U + value_len);
    return true;
}

bool xh_proto_put_u8(uint8_t *out, uint16_t cap, uint16_t *len, uint8_t type, uint8_t value)
{
    return xh_proto_put_raw(out, cap, len, type, &value, 1);
}

bool xh_proto_put_u16(uint8_t *out, uint16_t cap, uint16_t *len, uint8_t type, uint16_t value)
{
    uint8_t b[2] = {0};
    xh_put_le16(b, value);
    return xh_proto_put_raw(out, cap, len, type, b, sizeof(b));
}

bool xh_proto_put_i32(uint8_t *out, uint16_t cap, uint16_t *len, uint8_t type, int32_t value)
{
    uint8_t b[4] = {0};
    xh_put_le32(b, value);
    return xh_proto_put_raw(out, cap, len, type, b, sizeof(b));
}

bool xh_proto_put_u32(uint8_t *out, uint16_t cap, uint16_t *len, uint8_t type, uint32_t value)
{
    uint8_t b[4] = {0};
    xh_put_ule32(b, value);
    return xh_proto_put_raw(out, cap, len, type, b, sizeof(b));
}

bool xh_proto_put_bytes(uint8_t *out, uint16_t cap, uint16_t *len, uint8_t type,
                        const uint8_t *value, uint8_t value_len)
{
    return xh_proto_put_raw(out, cap, len, type, value, value_len);
}

bool xh_proto_finish(uint8_t *out, uint16_t cap, uint16_t *len)
{
    if (out == NULL || len == NULL || *len < XH_PROTO_HDR_LEN || *len > cap) {
        return false;
    }
    out[6] = (uint8_t)(*len - XH_PROTO_HDR_LEN);
    return true;
}

bool xh_proto_decode(const uint8_t *data, uint16_t len, xh_proto_msg_t *out)
{
    if (data == NULL || out == NULL || len < XH_PROTO_HDR_LEN) {
        return false;
    }
    if (data[0] != (uint8_t)XH_PROTO_MAGIC0 || data[1] != (uint8_t)XH_PROTO_MAGIC1 ||
        data[2] != XH_PROTO_VERSION) {
        return false;
    }
    if ((uint16_t)(XH_PROTO_HDR_LEN + data[6]) > len) {
        return false;
    }
    out->ver = data[2];
    out->seq = data[3];
    out->module_id = data[4];
    out->msg = data[5];
    out->tlv_len = data[6];
    out->tlv = &data[XH_PROTO_HDR_LEN];
    return true;
}

static bool xh_proto_find(const xh_proto_msg_t *msg, uint8_t type,
                          const uint8_t **value, uint8_t *value_len)
{
    if (msg == NULL || value == NULL || value_len == NULL || msg->tlv == NULL) {
        return false;
    }
    uint16_t off = 0;
    while ((uint16_t)(off + 2U) <= msg->tlv_len) {
        uint8_t t = msg->tlv[off];
        uint8_t l = msg->tlv[off + 1U];
        off = (uint16_t)(off + 2U);
        if ((uint16_t)(off + l) > msg->tlv_len) {
            return false;
        }
        if (t == type) {
            *value = &msg->tlv[off];
            *value_len = l;
            return true;
        }
        off = (uint16_t)(off + l);
    }
    return false;
}

bool xh_proto_get_u8(const xh_proto_msg_t *msg, uint8_t type, uint8_t *value)
{
    const uint8_t *p = NULL;
    uint8_t len = 0;
    if (value == NULL || !xh_proto_find(msg, type, &p, &len) || len < 1) {
        return false;
    }
    *value = p[0];
    return true;
}

bool xh_proto_get_u16(const xh_proto_msg_t *msg, uint8_t type, uint16_t *value)
{
    const uint8_t *p = NULL;
    uint8_t len = 0;
    if (value == NULL || !xh_proto_find(msg, type, &p, &len) || len < 2) {
        return false;
    }
    *value = xh_get_le16(p);
    return true;
}

bool xh_proto_get_i32(const xh_proto_msg_t *msg, uint8_t type, int32_t *value)
{
    const uint8_t *p = NULL;
    uint8_t len = 0;
    if (value == NULL || !xh_proto_find(msg, type, &p, &len) || len < 4) {
        return false;
    }
    *value = xh_get_le32(p);
    return true;
}

bool xh_proto_get_u32(const xh_proto_msg_t *msg, uint8_t type, uint32_t *value)
{
    const uint8_t *p = NULL;
    uint8_t len = 0;
    if (value == NULL || !xh_proto_find(msg, type, &p, &len) || len < 4) {
        return false;
    }
    *value = xh_get_ule32(p);
    return true;
}

bool xh_proto_get_bytes(const xh_proto_msg_t *msg, uint8_t type,
                        const uint8_t **value, uint8_t *value_len)
{
    return xh_proto_find(msg, type, value, value_len);
}

bool xh_proto_pack_ack(uint8_t *out, uint16_t cap, uint16_t *len,
                       uint8_t seq, uint8_t module_id, uint16_t err)
{
    if (out == NULL || len == NULL) {
        return false;
    }
    if (!xh_proto_begin(out, cap, seq, module_id, XH_PROTO_MSG_ACK)) {
        return false;
    }
    *len = XH_PROTO_HDR_LEN;
    if (!xh_proto_put_u16(out, cap, len, XH_TLV_ERROR_CODE, err)) {
        return false;
    }
    return xh_proto_finish(out, cap, len);
}
