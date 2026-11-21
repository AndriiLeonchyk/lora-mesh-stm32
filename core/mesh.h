#ifndef MESH_MESH_H
#define MESH_MESH_H

#include <stdint.h>

#include "dedup.h"
#include "packet.h"

/* What a node should do with a received packet. The values are bit flags
 * and can be combined: a broadcast is both delivered and forwarded. */
#define MESH_ACTION_DROP     0x00u  /* ignore the packet */
#define MESH_ACTION_DELIVER  0x01u  /* pass the payload to the application */
#define MESH_ACTION_FORWARD  0x02u  /* retransmit the copy in *fwd */

/* Why a packet was dropped. Useful for logs and statistics. */
typedef enum {
    MESH_DROP_NONE = 0,     /* the packet was not dropped */
    MESH_DROP_INVALID,      /* NULL node/pkt/fwd or ttl == 0 */
    MESH_DROP_OWN,          /* our own packet came back to us */
    MESH_DROP_DUPLICATE,    /* already seen this (src, msg_id) */
    MESH_DROP_TTL_EXPIRED   /* not for us and no hops left to forward */
} mesh_drop_reason_t;

/* State of one mesh node. */
typedef struct {
    uint16_t     addr;         /* this node's address */
    uint16_t     next_msg_id;  /* id for the next packet this node creates */
    mesh_dedup_t seen;         /* recently seen messages */
} mesh_node_t;

void mesh_node_init(mesh_node_t *node, uint16_t addr);

/* Builds a new packet originating at this node and records it as seen,
 * so the node will not forward its own packet when it hears it again.
 * Returns MESH_OK or a negative mesh_status_t. */
int mesh_node_build(mesh_node_t *node, mesh_packet_t *pkt, uint8_t type,
                    uint16_t dst, uint8_t ttl,
                    const uint8_t *data, uint8_t len);

/* Decides what to do with a received (already CRC-checked) packet.
 * Returns a combination of MESH_ACTION_* flags.
 * If MESH_ACTION_FORWARD is set, *fwd holds the copy to retransmit
 * (same packet, ttl decreased by one).
 * If reason is not NULL, it receives why the packet was dropped. */
uint8_t mesh_node_handle_rx(mesh_node_t *node, const mesh_packet_t *pkt,
                            mesh_packet_t *fwd, mesh_drop_reason_t *reason);

/* Random delay before a retransmission, in [min_ms, max_ms].
 * random_value comes from the platform (a hardware RNG, timer noise,
 * or rand() on a PC), so this function stays deterministic and testable. */
uint32_t mesh_backoff_ms(uint32_t random_value, uint32_t min_ms, uint32_t max_ms);

#endif /* MESH_MESH_H */
