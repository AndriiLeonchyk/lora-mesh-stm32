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

static mesh_packet_t make_packet(void)
{
    mesh_packet_t p;
    memset(&p, 0, sizeof(p));
    p.version = MESH_PROTOCOL_VERSION;
    p.type    = MESH_TYPE_DATA;
    p.src     = 0x0102;
    p.dst     = MESH_ADDR_BROADCAST;
    p.msg_id  = 0x0304;
    p.ttl     = 5;
    p.len     = 5;
    memcpy(p.payload, "hello", 5);
    return p;
}

static void test_crc_known_vector(void)
{
    const uint8_t data[] = "123456789";
    CHECK(crc16_ccitt(data, 9) == 0x29B1);
}

static void test_roundtrip(void)
{
    mesh_packet_t in = make_packet();
    mesh_packet_t out;
    uint8_t buf[MESH_MAX_FRAME_SIZE];

    int n = mesh_packet_serialize(&in, buf, sizeof(buf));
    CHECK(n == MESH_HEADER_SIZE + 5 + MESH_CRC_SIZE);

    /* Wire format is little-endian: src = 0x0102 -> 02 01 */
    CHECK(buf[2] == 0x02);
    CHECK(buf[3] == 0x01);

    CHECK(mesh_packet_parse(buf, (size_t)n, &out) == MESH_OK);
    CHECK(out.version == in.version);
    CHECK(out.type == in.type);
    CHECK(out.src == in.src);
    CHECK(out.dst == in.dst);
    CHECK(out.msg_id == in.msg_id);
    CHECK(out.ttl == in.ttl);
    CHECK(out.len == in.len);
    CHECK(memcmp(out.payload, in.payload, in.len) == 0);
}

static void test_corrupted_byte(void)
{
    mesh_packet_t in = make_packet();
    mesh_packet_t out;
    uint8_t buf[MESH_MAX_FRAME_SIZE];

    int n = mesh_packet_serialize(&in, buf, sizeof(buf));
    buf[MESH_HEADER_SIZE] ^= 0x01; /* flip one payload bit */
    CHECK(mesh_packet_parse(buf, (size_t)n, &out) == MESH_ERR_CRC);
}

static void test_too_short(void)
{
    mesh_packet_t out;
    uint8_t buf[5] = {0};
    CHECK(mesh_packet_parse(buf, sizeof(buf), &out) == MESH_ERR_TOO_SHORT);
}

static void test_wrong_version(void)
{
    mesh_packet_t in = make_packet();
    mesh_packet_t out;
    uint8_t buf[MESH_MAX_FRAME_SIZE];

    in.version = 2;
    int n = mesh_packet_serialize(&in, buf, sizeof(buf));
    CHECK(mesh_packet_parse(buf, (size_t)n, &out) == MESH_ERR_VERSION);
}

static void test_length_field_mismatch(void)
{
    mesh_packet_t in = make_packet();
    mesh_packet_t out;
    uint8_t buf[MESH_MAX_FRAME_SIZE];

    int n = mesh_packet_serialize(&in, buf, sizeof(buf));
    buf[9] = 4; /* claims 4 bytes of payload, frame carries 5 */

    /* Recompute a valid CRC so only the length check can fail. */
    size_t body = (size_t)n - MESH_CRC_SIZE;
    uint16_t crc = crc16_ccitt(buf, body);
    buf[body]     = (uint8_t)(crc & 0xFF);
    buf[body + 1] = (uint8_t)(crc >> 8);

    CHECK(mesh_packet_parse(buf, (size_t)n, &out) == MESH_ERR_LENGTH);
}

static void test_max_payload(void)
{
    mesh_packet_t in = make_packet();
    mesh_packet_t out;
    uint8_t buf[MESH_MAX_FRAME_SIZE];

    in.len = MESH_MAX_PAYLOAD;
    for (int i = 0; i < MESH_MAX_PAYLOAD; i++) {
        in.payload[i] = (uint8_t)i;
    }

    int n = mesh_packet_serialize(&in, buf, sizeof(buf));
    CHECK(n == MESH_MAX_FRAME_SIZE);
    CHECK(mesh_packet_parse(buf, (size_t)n, &out) == MESH_OK);
    CHECK(memcmp(out.payload, in.payload, MESH_MAX_PAYLOAD) == 0);
}

static void test_payload_too_long(void)
{
    mesh_packet_t in = make_packet();
    uint8_t buf[300];

    in.len = MESH_MAX_PAYLOAD + 1;
    CHECK(mesh_packet_serialize(&in, buf, sizeof(buf)) == MESH_ERR_TOO_LONG);
}

static void test_buffer_too_small(void)
{
    mesh_packet_t in = make_packet();
    uint8_t buf[16]; /* frame needs 17 bytes */

    CHECK(mesh_packet_serialize(&in, buf, sizeof(buf)) == MESH_ERR_TOO_LONG);
}

static void test_null_arguments(void)
{
    mesh_packet_t p = make_packet();
    uint8_t buf[MESH_MAX_FRAME_SIZE];

    CHECK(mesh_packet_serialize(NULL, buf, sizeof(buf)) == MESH_ERR_ARGS);
    CHECK(mesh_packet_serialize(&p, NULL, sizeof(buf)) == MESH_ERR_ARGS);
    CHECK(mesh_packet_parse(NULL, 20, &p) == MESH_ERR_ARGS);
    CHECK(mesh_packet_parse(buf, 20, NULL) == MESH_ERR_ARGS);
}

int main(void)
{
    test_crc_known_vector();
    test_roundtrip();
    test_corrupted_byte();
    test_too_short();
    test_wrong_version();
    test_length_field_mismatch();
    test_max_payload();
    test_payload_too_long();
    test_buffer_too_small();
    test_null_arguments();

    if (failures == 0) {
        printf("All tests passed\n");
        return 0;
    }
    printf("%d check(s) failed\n", failures);
    return 1;
}