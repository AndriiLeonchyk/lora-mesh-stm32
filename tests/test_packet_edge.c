/* Edge-case tests for mesh packet serialization and parsing. Runs on a PC. */
#include <stdio.h>
#include <string.h>

#include "crc16.h"
#include "packet.h"

static int failures = 0;

#define CHECK(cond)                                                  \
    do {                                                             \
        if (!(cond)) {                                               \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);   \
            failures++;                                              \
        }                                                            \
    } while (0)

static mesh_packet_t make_packet(uint8_t type, uint8_t len)
{
    mesh_packet_t p;
    memset(&p, 0, sizeof(p));
    p.version = MESH_PROTOCOL_VERSION;
    p.type    = type;
    p.src     = 0xA1B2;
    p.dst     = 0xC3D4;
    p.msg_id  = 0xE5F6;
    p.ttl     = 3;
    p.len     = len;
    for (uint8_t i = 0; i < len; i++) {
        p.payload[i] = (uint8_t)(0x40 + i);
    }
    return p;
}

/* Recompute the CRC after a deliberate header edit, so that only the
 * check under test can fail. */
static void fix_crc(uint8_t *buf, int frame_len)
{
    size_t body = (size_t)frame_len - MESH_CRC_SIZE;
    uint16_t crc = crc16_ccitt(buf, body);
    buf[body]     = (uint8_t)(crc & 0xFF);
    buf[body + 1] = (uint8_t)(crc >> 8);
}

/* A frame with no payload (e.g. HELLO or ACK) is valid. */
static void test_empty_payload(void)
{
    mesh_packet_t in = make_packet(MESH_TYPE_HELLO, 0);
    mesh_packet_t out;
    uint8_t buf[MESH_MAX_FRAME_SIZE];

    int n = mesh_packet_serialize(&in, buf, sizeof(buf));
    CHECK(n == MESH_HEADER_SIZE + MESH_CRC_SIZE);
    CHECK(mesh_packet_parse(buf, (size_t)n, &out) == MESH_OK);
    CHECK(out.len == 0);
    CHECK(out.type == MESH_TYPE_HELLO);
}

/* Every defined frame type survives a round trip. */
static void test_all_types_roundtrip(void)
{
    const uint8_t types[] = { MESH_TYPE_DATA, MESH_TYPE_ACK, MESH_TYPE_HELLO };
    for (size_t i = 0; i < sizeof(types); i++) {
        mesh_packet_t in = make_packet(types[i], 4);
        mesh_packet_t out;
        uint8_t buf[MESH_MAX_FRAME_SIZE];
        int n = mesh_packet_serialize(&in, buf, sizeof(buf));
        CHECK(mesh_packet_parse(buf, (size_t)n, &out) == MESH_OK);
        CHECK(out.type == types[i]);
    }
}

/* All header fields are written little-endian at fixed offsets. */
static void test_wire_layout(void)
{
    mesh_packet_t in = make_packet(MESH_TYPE_DATA, 2);
    uint8_t buf[MESH_MAX_FRAME_SIZE];
    int n = mesh_packet_serialize(&in, buf, sizeof(buf));

    CHECK(n == MESH_HEADER_SIZE + 2 + MESH_CRC_SIZE);
    CHECK(buf[0] == MESH_PROTOCOL_VERSION);
    CHECK(buf[1] == MESH_TYPE_DATA);
    CHECK(buf[2] == 0xB2 && buf[3] == 0xA1); /* src    = 0xA1B2 */
    CHECK(buf[4] == 0xD4 && buf[5] == 0xC3); /* dst    = 0xC3D4 */
    CHECK(buf[6] == 0xF6 && buf[7] == 0xE5); /* msg_id = 0xE5F6 */
    CHECK(buf[8] == 3);                      /* ttl */
    CHECK(buf[9] == 2);                      /* len */
    CHECK(buf[10] == 0x40 && buf[11] == 0x41);

    uint16_t crc = crc16_ccitt(buf, MESH_HEADER_SIZE + 2);
    CHECK(buf[12] == (uint8_t)(crc & 0xFF));
    CHECK(buf[13] == (uint8_t)(crc >> 8));
}

/* Boundary values of the 16-bit and 8-bit fields are preserved. */
static void test_field_extremes(void)
{
    mesh_packet_t in = make_packet(MESH_TYPE_DATA, 1);
    mesh_packet_t out;
    uint8_t buf[MESH_MAX_FRAME_SIZE];

    in.src = 0x0000;
    in.dst = MESH_ADDR_BROADCAST;
    in.msg_id = 0xFFFF;
    in.ttl = 0;
    int n = mesh_packet_serialize(&in, buf, sizeof(buf));
    CHECK(mesh_packet_parse(buf, (size_t)n, &out) == MESH_OK);
    CHECK(out.src == 0x0000);
    CHECK(out.dst == MESH_ADDR_BROADCAST);
    CHECK(out.msg_id == 0xFFFF);
    CHECK(out.ttl == 0);

    in.ttl = 255;
    n = mesh_packet_serialize(&in, buf, sizeof(buf));
    CHECK(mesh_packet_parse(buf, (size_t)n, &out) == MESH_OK);
    CHECK(out.ttl == 255);
}

/* Flipping any single bit anywhere in the frame (header, payload or CRC)
 * must be rejected. */
static void test_any_bit_flip_rejected(void)
{
    mesh_packet_t in = make_packet(MESH_TYPE_DATA, 20);
    mesh_packet_t out;
    uint8_t buf[MESH_MAX_FRAME_SIZE];
    int n = mesh_packet_serialize(&in, buf, sizeof(buf));

    int accepted = 0;
    for (int byte = 0; byte < n; byte++) {
        for (int bit = 0; bit < 8; bit++) {
            buf[byte] ^= (uint8_t)(1u << bit);
            if (mesh_packet_parse(buf, (size_t)n, &out) == MESH_OK) {
                accepted++;
            }
            buf[byte] ^= (uint8_t)(1u << bit);
        }
    }
    CHECK(accepted == 0);
    CHECK(mesh_packet_parse(buf, (size_t)n, &out) == MESH_OK); /* restored */
}

/* Truncating a valid frame by one byte breaks the CRC check. */
static void test_truncated_frame(void)
{
    mesh_packet_t in = make_packet(MESH_TYPE_DATA, 8);
    mesh_packet_t out;
    uint8_t buf[MESH_MAX_FRAME_SIZE];
    int n = mesh_packet_serialize(&in, buf, sizeof(buf));
    CHECK(mesh_packet_parse(buf, (size_t)n - 1, &out) != MESH_OK);
}

/* The smallest possible frame length is accepted; one byte less is too short. */
static void test_minimum_length_boundary(void)
{
    mesh_packet_t out;
    uint8_t buf[MESH_HEADER_SIZE + MESH_CRC_SIZE] = { 0 };
    CHECK(mesh_packet_parse(buf, MESH_HEADER_SIZE + MESH_CRC_SIZE - 1, &out)
          == MESH_ERR_TOO_SHORT);
}

/* A received length above the LoRa frame limit is rejected. */
static void test_frame_over_limit(void)
{
    mesh_packet_t out;
    uint8_t buf[MESH_MAX_FRAME_SIZE + 1];
    memset(buf, 0, sizeof(buf));
    CHECK(mesh_packet_parse(buf, sizeof(buf), &out) == MESH_ERR_TOO_LONG);
}

/* The len field claiming more payload than the frame carries is rejected. */
static void test_length_field_too_big(void)
{
    mesh_packet_t in = make_packet(MESH_TYPE_DATA, 4);
    mesh_packet_t out;
    uint8_t buf[MESH_MAX_FRAME_SIZE];
    int n = mesh_packet_serialize(&in, buf, sizeof(buf));
    buf[9] = 5;
    fix_crc(buf, n);
    CHECK(mesh_packet_parse(buf, (size_t)n, &out) == MESH_ERR_LENGTH);
}

/* The len field claiming less payload than the frame carries is rejected. */
static void test_length_field_too_small(void)
{
    mesh_packet_t in = make_packet(MESH_TYPE_DATA, 4);
    mesh_packet_t out;
    uint8_t buf[MESH_MAX_FRAME_SIZE];
    int n = mesh_packet_serialize(&in, buf, sizeof(buf));
    buf[9] = 3;
    fix_crc(buf, n);
    CHECK(mesh_packet_parse(buf, (size_t)n, &out) == MESH_ERR_LENGTH);
}

/* Version 0 is not accepted either (only MESH_PROTOCOL_VERSION is). */
static void test_version_zero_rejected(void)
{
    mesh_packet_t in = make_packet(MESH_TYPE_DATA, 1);
    mesh_packet_t out;
    uint8_t buf[MESH_MAX_FRAME_SIZE];
    in.version = 0;
    int n = mesh_packet_serialize(&in, buf, sizeof(buf));
    CHECK(mesh_packet_parse(buf, (size_t)n, &out) == MESH_ERR_VERSION);
}

/* A buffer of exactly the needed size is enough. */
static void test_exact_buffer_size(void)
{
    mesh_packet_t in = make_packet(MESH_TYPE_DATA, 5);
    uint8_t buf[MESH_HEADER_SIZE + 5 + MESH_CRC_SIZE];
    CHECK(mesh_packet_serialize(&in, buf, sizeof(buf)) == (int)sizeof(buf));
}

/* Serializing the same packet twice gives identical bytes. */
static void test_serialize_deterministic(void)
{
    mesh_packet_t in = make_packet(MESH_TYPE_DATA, 16);
    uint8_t a[MESH_MAX_FRAME_SIZE];
    uint8_t b[MESH_MAX_FRAME_SIZE];
    memset(a, 0x11, sizeof(a));
    memset(b, 0x22, sizeof(b));
    int na = mesh_packet_serialize(&in, a, sizeof(a));
    int nb = mesh_packet_serialize(&in, b, sizeof(b));
    CHECK(na == nb);
    CHECK(memcmp(a, b, (size_t)na) == 0);
}

int main(void)
{
    test_empty_payload();
    test_all_types_roundtrip();
    test_wire_layout();
    test_field_extremes();
    test_any_bit_flip_rejected();
    test_truncated_frame();
    test_minimum_length_boundary();
    test_frame_over_limit();
    test_length_field_too_big();
    test_length_field_too_small();
    test_version_zero_rejected();
    test_exact_buffer_size();
    test_serialize_deterministic();

    if (failures == 0) {
        printf("test_packet_edge: all tests passed\n");
        return 0;
    }
    printf("test_packet_edge: %d check(s) failed\n", failures);
    return 1;
}
