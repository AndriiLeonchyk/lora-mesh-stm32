/* Unit tests for deduplication, forwarding decisions and backoff. Runs on a PC. */
#include <stdio.h>
#include <string.h>

#include "dedup.h"
#include "mesh.h"
#include "packet.h"

static int failures = 0;

#define CHECK(cond)                                                  \
    do {                                                             \
        if (!(cond)) {                                               \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);   \
            failures++;                                              \
        }                                                            \
    } while (0)

/* A packet as it would arrive from the radio after CRC check. */
static mesh_packet_t rx_packet(uint16_t src, uint16_t dst, uint16_t msg_id, uint8_t ttl)
{
    mesh_packet_t p;
    memset(&p, 0, sizeof(p));
    p.version = MESH_PROTOCOL_VERSION;
    p.type    = MESH_TYPE_DATA;
    p.src     = src;
    p.dst     = dst;
    p.msg_id  = msg_id;
    p.ttl     = ttl;
    p.len     = 2;
    p.payload[0] = 0xAB;
    p.payload[1] = 0xCD;
    return p;
}

/* ---------------- dedup ---------------- */

static void test_dedup_new_then_duplicate(void)
{
    mesh_dedup_t d;
    mesh_dedup_init(&d);
    CHECK(mesh_dedup_check_and_add(&d, 1, 100) == false); /* first time */
    CHECK(mesh_dedup_check_and_add(&d, 1, 100) == true);  /* duplicate */
}

static void test_dedup_key_is_src_and_id(void)
{
    mesh_dedup_t d;
    mesh_dedup_init(&d);
    mesh_dedup_add(&d, 1, 100);
    CHECK(mesh_dedup_contains(&d, 1, 100));
    CHECK(!mesh_dedup_contains(&d, 2, 100)); /* same id, other sender */
    CHECK(!mesh_dedup_contains(&d, 1, 101)); /* same sender, other id */
}

static void test_dedup_contains_does_not_add(void)
{
    mesh_dedup_t d;
    mesh_dedup_init(&d);
    CHECK(!mesh_dedup_contains(&d, 5, 5));
    CHECK(!mesh_dedup_contains(&d, 5, 5));
    CHECK(d.count == 0);
}

static void test_dedup_evicts_oldest_when_full(void)
{
    mesh_dedup_t d;
    mesh_dedup_init(&d);
    for (uint16_t i = 0; i < MESH_DEDUP_SIZE; i++) {
        mesh_dedup_add(&d, 7, i);
    }
    CHECK(d.count == MESH_DEDUP_SIZE);
    CHECK(mesh_dedup_contains(&d, 7, 0));

    mesh_dedup_add(&d, 7, 1000);                 /* one more: oldest goes */
    CHECK(d.count == MESH_DEDUP_SIZE);
    CHECK(!mesh_dedup_contains(&d, 7, 0));       /* evicted */
    CHECK(mesh_dedup_contains(&d, 7, 1));        /* still there */
    CHECK(mesh_dedup_contains(&d, 7, 1000));     /* newest */
}

/* ---------------- forwarding decisions ---------------- */

static void test_unicast_for_me_delivered_not_forwarded(void)
{
    mesh_node_t n;
    mesh_packet_t fwd;
    mesh_drop_reason_t why;
    mesh_node_init(&n, 0x0002, 0);

    mesh_packet_t p = rx_packet(0x0001, 0x0002, 1, 5);
    uint8_t a = mesh_node_handle_rx(&n, &p, &fwd, &why);
    CHECK(a == MESH_ACTION_DELIVER);
    CHECK(why == MESH_DROP_NONE);
}

static void test_unicast_for_other_forwarded_with_ttl_minus_one(void)
{
    mesh_node_t n;
    mesh_packet_t fwd;
    mesh_node_init(&n, 0x0002, 0);

    mesh_packet_t p = rx_packet(0x0001, 0x0003, 1, 5);
    uint8_t a = mesh_node_handle_rx(&n, &p, &fwd, NULL);
    CHECK(a == MESH_ACTION_FORWARD);
    CHECK(fwd.ttl == 4);
    /* everything else is unchanged */
    CHECK(fwd.src == p.src && fwd.dst == p.dst && fwd.msg_id == p.msg_id);
    CHECK(fwd.len == p.len && memcmp(fwd.payload, p.payload, p.len) == 0);
}

static void test_broadcast_delivered_and_forwarded(void)
{
    mesh_node_t n;
    mesh_packet_t fwd;
    mesh_node_init(&n, 0x0002, 0);

    mesh_packet_t p = rx_packet(0x0001, MESH_ADDR_BROADCAST, 1, 3);
    uint8_t a = mesh_node_handle_rx(&n, &p, &fwd, NULL);
    CHECK(a == (MESH_ACTION_DELIVER | MESH_ACTION_FORWARD));
    CHECK(fwd.ttl == 2);
}

static void test_broadcast_last_hop_delivered_only(void)
{
    mesh_node_t n;
    mesh_packet_t fwd;
    mesh_node_init(&n, 0x0002, 0);

    mesh_packet_t p = rx_packet(0x0001, MESH_ADDR_BROADCAST, 1, 1);
    CHECK(mesh_node_handle_rx(&n, &p, &fwd, NULL) == MESH_ACTION_DELIVER);
}

static void test_for_other_ttl_one_dropped(void)
{
    mesh_node_t n;
    mesh_packet_t fwd;
    mesh_drop_reason_t why;
    mesh_node_init(&n, 0x0002, 0);

    mesh_packet_t p = rx_packet(0x0001, 0x0003, 1, 1);
    CHECK(mesh_node_handle_rx(&n, &p, &fwd, &why) == MESH_ACTION_DROP);
    CHECK(why == MESH_DROP_TTL_EXPIRED);
}

static void test_duplicate_dropped(void)
{
    mesh_node_t n;
    mesh_packet_t fwd;
    mesh_drop_reason_t why;
    mesh_node_init(&n, 0x0002, 0);

    mesh_packet_t p = rx_packet(0x0001, 0x0003, 7, 5);
    CHECK(mesh_node_handle_rx(&n, &p, &fwd, &why) == MESH_ACTION_FORWARD);

    /* the same message arrives again via another neighbor, with lower ttl */
    p.ttl = 4;
    CHECK(mesh_node_handle_rx(&n, &p, &fwd, &why) == MESH_ACTION_DROP);
    CHECK(why == MESH_DROP_DUPLICATE);
}

static void test_own_packet_echo_dropped(void)
{
    mesh_node_t n;
    mesh_packet_t out, fwd;
    mesh_drop_reason_t why;
    mesh_node_init(&n, 0x0002, 0);

    const uint8_t data[] = { 1, 2, 3 };
    CHECK(mesh_node_build(&n, &out, MESH_TYPE_DATA, MESH_ADDR_BROADCAST, 3,
                          data, sizeof(data)) == MESH_OK);

    /* a neighbor retransmits our broadcast back to us */
    out.ttl = 2;
    CHECK(mesh_node_handle_rx(&n, &out, &fwd, &why) == MESH_ACTION_DROP);
    CHECK(why == MESH_DROP_OWN);
}

static void test_invalid_input_dropped(void)
{
    mesh_node_t n;
    mesh_packet_t fwd;
    mesh_drop_reason_t why;
    mesh_node_init(&n, 0x0002, 0);

    mesh_packet_t p = rx_packet(0x0001, 0x0002, 1, 0); /* ttl 0 */
    CHECK(mesh_node_handle_rx(&n, &p, &fwd, &why) == MESH_ACTION_DROP);
    CHECK(why == MESH_DROP_INVALID);

    p.ttl = 3;
    CHECK(mesh_node_handle_rx(NULL, &p, &fwd, &why) == MESH_ACTION_DROP);
    CHECK(mesh_node_handle_rx(&n, NULL, &fwd, &why) == MESH_ACTION_DROP);
    CHECK(mesh_node_handle_rx(&n, &p, NULL, &why) == MESH_ACTION_DROP);
    CHECK(why == MESH_DROP_INVALID);
}

/* ---------------- building packets ---------------- */

static void test_build_fills_fields_and_increments_id(void)
{
    mesh_node_t n;
    mesh_packet_t a, b;
    mesh_node_init(&n, 0x0042, 0);

    const uint8_t data[] = { 'h', 'i' };
    CHECK(mesh_node_build(&n, &a, MESH_TYPE_DATA, 0x0007, 4, data, 2) == MESH_OK);
    CHECK(mesh_node_build(&n, &b, MESH_TYPE_DATA, 0x0007, 4, data, 2) == MESH_OK);

    CHECK(a.version == MESH_PROTOCOL_VERSION);
    CHECK(a.src == 0x0042 && a.dst == 0x0007 && a.ttl == 4 && a.len == 2);
    CHECK(a.payload[0] == 'h' && a.payload[1] == 'i');
    CHECK(b.msg_id == (uint16_t)(a.msg_id + 1));
}

static void test_build_msg_id_wraps(void)
{
    mesh_node_t n;
    mesh_packet_t a, b;
    mesh_node_init(&n, 0x0001, 0xFFFF);
    CHECK(mesh_node_build(&n, &a, MESH_TYPE_HELLO, MESH_ADDR_BROADCAST, 1, NULL, 0) == MESH_OK);
    CHECK(mesh_node_build(&n, &b, MESH_TYPE_HELLO, MESH_ADDR_BROADCAST, 1, NULL, 0) == MESH_OK);
    CHECK(a.msg_id == 0xFFFF);
    CHECK(b.msg_id == 0x0000);
}

static void test_build_rejects_bad_arguments(void)
{
    mesh_node_t n;
    mesh_packet_t p;
    uint8_t big[MESH_MAX_PAYLOAD + 1];
    memset(big, 0, sizeof(big));
    mesh_node_init(&n, 1, 0);

    CHECK(mesh_node_build(&n, &p, MESH_TYPE_DATA, 2, 0, NULL, 0) == MESH_ERR_ARGS);    /* ttl 0 */
    CHECK(mesh_node_build(&n, &p, MESH_TYPE_DATA, 2, 3, NULL, 5) == MESH_ERR_ARGS);    /* no data */
    CHECK(mesh_node_build(NULL, &p, MESH_TYPE_DATA, 2, 3, NULL, 0) == MESH_ERR_ARGS);
    CHECK(mesh_node_build(&n, NULL, MESH_TYPE_DATA, 2, 3, NULL, 0) == MESH_ERR_ARGS);
    CHECK(mesh_node_build(&n, &p, MESH_TYPE_DATA, 2, 3, big,
                          (uint8_t)(MESH_MAX_PAYLOAD + 1)) == MESH_ERR_TOO_LONG);
}

static void test_build_starts_from_first_msg_id(void)
{
    mesh_node_t n;
    mesh_packet_t p;
    mesh_node_init(&n, 0x0001, 0x1234);
    CHECK(mesh_node_build(&n, &p, MESH_TYPE_DATA, 2, 3, NULL, 0) == MESH_OK);
    CHECK(p.msg_id == 0x1234);
}

static void test_build_rejects_unknown_type(void)
{
    mesh_node_t n;
    mesh_packet_t p;
    mesh_node_init(&n, 1, 0);
    CHECK(mesh_node_build(&n, &p, 0x7F, 2, 3, NULL, 0) == MESH_ERR_TYPE);
    CHECK(n.next_msg_id == 0); /* a failed build does not use up an id */
}

/* ---------------- reboot ---------------- */

/* Node 1 sends 5 messages, then reboots. Its neighbor (node 2) still
 * remembers (1, 0..4). This shows why the first msg_id must change at boot. */
static void test_reboot_same_start_id_is_dropped(void)
{
    mesh_node_t n1, n2;
    mesh_packet_t p, fwd;
    mesh_drop_reason_t why;
    mesh_node_init(&n1, 1, 0);
    mesh_node_init(&n2, 2, 0);
    for (int i = 0; i < 5; i++) {
        CHECK(mesh_node_build(&n1, &p, MESH_TYPE_DATA, 2, 3, NULL, 0) == MESH_OK);
        CHECK(mesh_node_handle_rx(&n2, &p, &fwd, NULL) == MESH_ACTION_DELIVER);
    }

    mesh_node_init(&n1, 1, 0);  /* reboot, counter starts from 0 again */
    CHECK(mesh_node_build(&n1, &p, MESH_TYPE_DATA, 2, 3, NULL, 0) == MESH_OK);
    CHECK(mesh_node_handle_rx(&n2, &p, &fwd, &why) == MESH_ACTION_DROP);
    CHECK(why == MESH_DROP_DUPLICATE);  /* a new message is lost */
}

static void test_reboot_new_start_id_is_delivered(void)
{
    mesh_node_t n1, n2;
    mesh_packet_t p, fwd;
    mesh_node_init(&n1, 1, 0);
    mesh_node_init(&n2, 2, 0);
    for (int i = 0; i < 5; i++) {
        CHECK(mesh_node_build(&n1, &p, MESH_TYPE_DATA, 2, 3, NULL, 0) == MESH_OK);
        CHECK(mesh_node_handle_rx(&n2, &p, &fwd, NULL) == MESH_ACTION_DELIVER);
    }

    mesh_node_init(&n1, 1, 0xA5C3);  /* reboot with a random start */
    CHECK(mesh_node_build(&n1, &p, MESH_TYPE_DATA, 2, 3, NULL, 0) == MESH_OK);
    CHECK(mesh_node_handle_rx(&n2, &p, &fwd, NULL) == MESH_ACTION_DELIVER);
}

/* ---------------- backoff ---------------- */

static void test_backoff_stays_in_range(void)
{
    int out_of_range = 0;
    for (uint32_t r = 0; r < 100000; r += 7) {
        uint32_t ms = mesh_backoff_ms(r * 2654435761u, 20, 120);
        if (ms < 20 || ms > 120) {
            out_of_range++;
        }
    }
    CHECK(out_of_range == 0);
}

static void test_backoff_covers_both_ends(void)
{
    CHECK(mesh_backoff_ms(0, 20, 120) == 20);
    CHECK(mesh_backoff_ms(100, 20, 120) == 120);
    CHECK(mesh_backoff_ms(101, 20, 120) == 20);
}

static void test_backoff_degenerate_ranges(void)
{
    CHECK(mesh_backoff_ms(12345, 50, 50) == 50);   /* min == max */
    CHECK(mesh_backoff_ms(12345, 80, 10) == 80);   /* min > max  */
    CHECK(mesh_backoff_ms(5, 0, 0xFFFFFFFFu) == 5); /* full range, no overflow */
}

/* ---------------- small multi-hop scenario ---------------- */

/* Four nodes in a line: 1 - 2 - 3 - 4. Each node hears only its neighbors.
 * Node 1 sends a unicast to node 4 with ttl 3. The message must reach
 * node 4 exactly once, and every middle node must forward it exactly once. */
static void test_line_of_four_nodes(void)
{
    enum { N = 4 };
    mesh_node_t nodes[N];
    for (int i = 0; i < N; i++) {
        mesh_node_init(&nodes[i], (uint16_t)(i + 1), 0);
    }

    /* queue of (receiver index, packet) to process, simple breadth-first */
    struct { int to; mesh_packet_t pkt; } queue[32];
    int head = 0, tail = 0;
    int delivered_at_4 = 0;
    int forwards[N] = { 0 };

    mesh_packet_t first;
    const uint8_t data[] = { 0x55 };
    CHECK(mesh_node_build(&nodes[0], &first, MESH_TYPE_DATA, 4, 3, data, 1) == MESH_OK);
    queue[tail].to = 1; queue[tail].pkt = first; tail++;   /* node 1 -> node 2 */

    while (head < tail) {
        int i = queue[head].to;
        mesh_packet_t pkt = queue[head].pkt;
        head++;

        mesh_packet_t fwd;
        uint8_t a = mesh_node_handle_rx(&nodes[i], &pkt, &fwd, NULL);
        if ((a & MESH_ACTION_DELIVER) && nodes[i].addr == 4) {
            delivered_at_4++;
        }
        if (a & MESH_ACTION_FORWARD) {
            forwards[i]++;
            /* a radio transmission reaches both neighbors */
            if (i > 0 && tail < 32)     { queue[tail].to = i - 1; queue[tail].pkt = fwd; tail++; }
            if (i < N - 1 && tail < 32) { queue[tail].to = i + 1; queue[tail].pkt = fwd; tail++; }
        }
    }

    CHECK(delivered_at_4 == 1);
    CHECK(forwards[1] == 1);   /* node 2 */
    CHECK(forwards[2] == 1);   /* node 3 */
    CHECK(forwards[3] == 0);   /* node 4 is the destination */
}

/* Same line, but ttl 2 is not enough to cover 3 hops. */
static void test_line_ttl_too_small(void)
{
    mesh_node_t n2, n3, n4;
    mesh_node_t n1;
    mesh_packet_t p, f2, f3;
    mesh_node_init(&n1, 1, 0);
    mesh_node_init(&n2, 2, 0);
    mesh_node_init(&n3, 3, 0);
    mesh_node_init(&n4, 4, 0);

    CHECK(mesh_node_build(&n1, &p, MESH_TYPE_DATA, 4, 2, NULL, 0) == MESH_OK);
    CHECK(mesh_node_handle_rx(&n2, &p, &f2, NULL) == MESH_ACTION_FORWARD);  /* ttl 2 -> 1 */
    CHECK(mesh_node_handle_rx(&n3, &f2, &f3, NULL) == MESH_ACTION_DROP);    /* ttl 1, not for 3 */
    (void)n4;
}

int main(void)
{
    test_dedup_new_then_duplicate();
    test_dedup_key_is_src_and_id();
    test_dedup_contains_does_not_add();
    test_dedup_evicts_oldest_when_full();

    test_unicast_for_me_delivered_not_forwarded();
    test_unicast_for_other_forwarded_with_ttl_minus_one();
    test_broadcast_delivered_and_forwarded();
    test_broadcast_last_hop_delivered_only();
    test_for_other_ttl_one_dropped();
    test_duplicate_dropped();
    test_own_packet_echo_dropped();
    test_invalid_input_dropped();

    test_build_fills_fields_and_increments_id();
    test_build_msg_id_wraps();
    test_build_rejects_bad_arguments();
    test_build_starts_from_first_msg_id();
    test_build_rejects_unknown_type();

    test_reboot_same_start_id_is_dropped();
    test_reboot_new_start_id_is_delivered();

    test_backoff_stays_in_range();
    test_backoff_covers_both_ends();
    test_backoff_degenerate_ranges();

    test_line_of_four_nodes();
    test_line_ttl_too_small();

    if (failures == 0) {
        printf("test_mesh: all tests passed\n");
        return 0;
    }
    printf("test_mesh: %d check(s) failed\n", failures);
    return 1;
}
