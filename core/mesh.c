#include "mesh.h"

#include <stddef.h>
#include <string.h>

void mesh_node_init(mesh_node_t *node, uint16_t addr)
{
    if (node == NULL) {
        return;
    }
    node->addr = addr;
    node->next_msg_id = 0;
    mesh_dedup_init(&node->seen);
}

int mesh_node_build(mesh_node_t *node, mesh_packet_t *pkt, uint8_t type,
                    uint16_t dst, uint8_t ttl,
                    const uint8_t *data, uint8_t len)
{
    if (node == NULL || pkt == NULL || (data == NULL && len > 0) || ttl == 0) {
        return MESH_ERR_ARGS;
    }
    if (len > MESH_MAX_PAYLOAD) {
        return MESH_ERR_TOO_LONG;
    }

    memset(pkt, 0, sizeof(*pkt));
    pkt->version = MESH_PROTOCOL_VERSION;
    pkt->type    = type;
    pkt->src     = node->addr;
    pkt->dst     = dst;
    pkt->msg_id  = node->next_msg_id++;   /* wraps from 0xFFFF to 0 */
    pkt->ttl     = ttl;
    pkt->len     = len;
    if (len > 0) {
        memcpy(pkt->payload, data, len);
    }

    /* Remember our own message, so an echo from a neighbor is ignored. */
    mesh_dedup_add(&node->seen, pkt->src, pkt->msg_id);
    return MESH_OK;
}

static uint8_t drop(mesh_drop_reason_t *reason, mesh_drop_reason_t why)
{
    if (reason != NULL) {
        *reason = why;
    }
    return MESH_ACTION_DROP;
}

uint8_t mesh_node_handle_rx(mesh_node_t *node, const mesh_packet_t *pkt,
                            mesh_packet_t *fwd, mesh_drop_reason_t *reason)
{
    if (reason != NULL) {
        *reason = MESH_DROP_NONE;
    }
    if (node == NULL || pkt == NULL || fwd == NULL || pkt->ttl == 0) {
        return drop(reason, MESH_DROP_INVALID);
    }

    /* 1. Our own packet, retransmitted back to us by a neighbor. */
    if (pkt->src == node->addr) {
        return drop(reason, MESH_DROP_OWN);
    }

    /* 2. Already seen: another neighbor forwarded the same message. */
    if (mesh_dedup_check_and_add(&node->seen, pkt->src, pkt->msg_id)) {
        return drop(reason, MESH_DROP_DUPLICATE);
    }

    const int for_me    = (pkt->dst == node->addr);
    const int broadcast = (pkt->dst == MESH_ADDR_BROADCAST);
    uint8_t actions = MESH_ACTION_DROP;

    /* 3. Deliver if the message is addressed to us or to everyone. */
    if (for_me || broadcast) {
        actions |= MESH_ACTION_DELIVER;
    }

    /* 4. Forward if someone else still needs it and hops are left. */
    if (!for_me && pkt->ttl > 1) {
        *fwd = *pkt;
        fwd->ttl = (uint8_t)(pkt->ttl - 1u);
        actions |= MESH_ACTION_FORWARD;
    }

    if (actions == MESH_ACTION_DROP) {
        return drop(reason, MESH_DROP_TTL_EXPIRED);
    }
    return actions;
}

uint32_t mesh_backoff_ms(uint32_t random_value, uint32_t min_ms, uint32_t max_ms)
{
    if (max_ms <= min_ms) {
        return min_ms;
    }
    uint32_t span = max_ms - min_ms;
    if (span == UINT32_MAX) {           /* span + 1 would overflow to 0 */
        return min_ms + random_value;
    }
    return min_ms + (random_value % (span + 1u));
}
