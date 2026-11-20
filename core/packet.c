#include "packet.h"
#include "crc16.h"

#include <string.h>

static void put_u16_le(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)(v >> 8);
}

static uint16_t get_u16_le(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

int mesh_packet_serialize(const mesh_packet_t *pkt, uint8_t *buf, size_t buf_size)
{
    if (pkt == NULL || buf == NULL) {
        return MESH_ERR_ARGS;
    }
    if (pkt->len > MESH_MAX_PAYLOAD) {
        return MESH_ERR_TOO_LONG;
    }

    size_t total = (size_t)MESH_HEADER_SIZE + pkt->len + MESH_CRC_SIZE;
    if (buf_size < total) {
        return MESH_ERR_TOO_LONG;
    }

    buf[0] = pkt->version;
    buf[1] = pkt->type;
    put_u16_le(&buf[2], pkt->src);
    put_u16_le(&buf[4], pkt->dst);
    put_u16_le(&buf[6], pkt->msg_id);
    buf[8] = pkt->ttl;
    buf[9] = pkt->len;

    if (pkt->len > 0) {
        memcpy(&buf[MESH_HEADER_SIZE], pkt->payload, pkt->len);
    }

    size_t body = (size_t)MESH_HEADER_SIZE + pkt->len;
    put_u16_le(&buf[body], crc16_ccitt(buf, body));

    return (int)total;
}

int mesh_packet_parse(const uint8_t *buf, size_t len, mesh_packet_t *pkt)
{
    if (buf == NULL || pkt == NULL) {
        return MESH_ERR_ARGS;
    }
    if (len < (size_t)MESH_HEADER_SIZE + MESH_CRC_SIZE) {
        return MESH_ERR_TOO_SHORT;
    }
    if (len > MESH_MAX_FRAME_SIZE) {
        return MESH_ERR_TOO_LONG;
    }

    /* Verify the CRC first: nothing else in the frame can be trusted before that. */
    size_t body = len - MESH_CRC_SIZE;
    if (crc16_ccitt(buf, body) != get_u16_le(&buf[body])) {
        return MESH_ERR_CRC;
    }

    if (buf[0] != MESH_PROTOCOL_VERSION) {
        return MESH_ERR_VERSION;
    }
    if ((size_t)buf[9] != body - MESH_HEADER_SIZE) {
        return MESH_ERR_LENGTH;
    }

    memset(pkt, 0, sizeof(*pkt));
    pkt->version = buf[0];
    pkt->type    = buf[1];
    pkt->src     = get_u16_le(&buf[2]);
    pkt->dst     = get_u16_le(&buf[4]);
    pkt->msg_id  = get_u16_le(&buf[6]);
    pkt->ttl     = buf[8];
    pkt->len     = buf[9];
    memcpy(pkt->payload, &buf[MESH_HEADER_SIZE], pkt->len);

    return MESH_OK;
}