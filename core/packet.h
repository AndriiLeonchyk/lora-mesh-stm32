#ifndef MESH_PACKET_H
#define MESH_PACKET_H

#include <stddef.h>
#include <stdint.h>

#define MESH_PROTOCOL_VERSION 1
#define MESH_ADDR_BROADCAST   0xFFFF

#define MESH_HEADER_SIZE      10
#define MESH_CRC_SIZE         2
#define MESH_MAX_FRAME_SIZE   255
#define MESH_MAX_PAYLOAD      (MESH_MAX_FRAME_SIZE - MESH_HEADER_SIZE - MESH_CRC_SIZE)

typedef enum {
    MESH_TYPE_DATA  = 0x01,
    MESH_TYPE_ACK   = 0x02,
    MESH_TYPE_HELLO = 0x03
} mesh_type_t;

typedef enum {
    MESH_OK            =  0,
    MESH_ERR_ARGS      = -1,  /* NULL pointer or invalid argument */
    MESH_ERR_TOO_SHORT = -2,  /* frame shorter than header + CRC */
    MESH_ERR_TOO_LONG  = -3,  /* payload does not fit */
    MESH_ERR_VERSION   = -4,  /* unsupported protocol version */
    MESH_ERR_LENGTH    = -5,  /* len field does not match frame size */
    MESH_ERR_CRC       = -6   /* checksum mismatch */
} mesh_status_t;

/* In-memory representation. Serialization is done field by field,
 * so struct padding and CPU endianness do not affect the wire format. */
typedef struct {
    uint8_t  version;
    uint8_t  type;
    uint16_t src;
    uint16_t dst;
    uint16_t msg_id;
    uint8_t  ttl;
    uint8_t  len;
    uint8_t  payload[MESH_MAX_PAYLOAD];
} mesh_packet_t;

/* Writes the packet (header + payload + CRC) into buf.
 * Returns the number of bytes written, or a negative mesh_status_t. */
int mesh_packet_serialize(const mesh_packet_t *pkt, uint8_t *buf, size_t buf_size);

/* Parses and validates a received frame.
 * Returns MESH_OK or a negative mesh_status_t. */
int mesh_packet_parse(const uint8_t *buf, size_t len, mesh_packet_t *pkt);

#endif /* MESH_PACKET_H */