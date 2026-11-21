/* Unit tests for crc16_ccitt (CRC-16/CCITT-FALSE). Runs on a PC. */
#include <stdio.h>
#include <string.h>

#include "crc16.h"

static int failures = 0;

#define CHECK(cond)                                                  \
    do {                                                             \
        if (!(cond)) {                                               \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);   \
            failures++;                                              \
        }                                                            \
    } while (0)

/* Published check values for CRC-16/CCITT-FALSE. */
static void test_known_vectors(void)
{
    CHECK(crc16_ccitt((const uint8_t *)"123456789", 9) == 0x29B1);
    CHECK(crc16_ccitt((const uint8_t *)"A", 1) == 0xB915);
}

/* With no data the CRC stays at the initial value 0xFFFF. */
static void test_empty_input(void)
{
    const uint8_t dummy = 0;
    CHECK(crc16_ccitt(&dummy, 0) == 0xFFFF);
}

/* Same input always gives the same CRC. */
static void test_deterministic(void)
{
    const uint8_t data[] = { 0x10, 0x20, 0x30, 0x40, 0x50 };
    CHECK(crc16_ccitt(data, sizeof(data)) == crc16_ccitt(data, sizeof(data)));
}

/* Every single-bit error in a frame must change the CRC. */
static void test_detects_every_single_bit_flip(void)
{
    uint8_t data[64];
    for (size_t i = 0; i < sizeof(data); i++) {
        data[i] = (uint8_t)(i * 7 + 3);
    }
    const uint16_t good = crc16_ccitt(data, sizeof(data));

    int undetected = 0;
    for (size_t byte = 0; byte < sizeof(data); byte++) {
        for (int bit = 0; bit < 8; bit++) {
            data[byte] ^= (uint8_t)(1u << bit);
            if (crc16_ccitt(data, sizeof(data)) == good) {
                undetected++;
            }
            data[byte] ^= (uint8_t)(1u << bit); /* restore */
        }
    }
    CHECK(undetected == 0);
}

/* Swapping two different bytes must change the CRC (order matters). */
static void test_detects_byte_swap(void)
{
    uint8_t a[] = { 0x01, 0x02, 0x03, 0x04 };
    uint8_t b[] = { 0x02, 0x01, 0x03, 0x04 };
    CHECK(crc16_ccitt(a, sizeof(a)) != crc16_ccitt(b, sizeof(b)));
}

/* A trailing zero byte must change the CRC (length matters). */
static void test_detects_appended_zero(void)
{
    uint8_t data[] = { 0xAA, 0xBB, 0xCC, 0x00 };
    CHECK(crc16_ccitt(data, 3) != crc16_ccitt(data, 4));
}

/* Appending the CRC (big-endian) to the data gives a remainder of 0.
 * This is the standard self-check property of a non-reflected CRC
 * without a final XOR. */
static void test_residue_is_zero(void)
{
    uint8_t data[11] = { 'h', 'e', 'l', 'l', 'o', ' ', 'm', 'e', 's', 'h' };
    const uint16_t crc = crc16_ccitt(data, 10);
    uint8_t framed[12];
    memcpy(framed, data, 10);
    framed[10] = (uint8_t)(crc >> 8);
    framed[11] = (uint8_t)(crc & 0xFF);
    CHECK(crc16_ccitt(framed, 12) == 0x0000);
}

int main(void)
{
    test_known_vectors();
    test_empty_input();
    test_deterministic();
    test_detects_every_single_bit_flip();
    test_detects_byte_swap();
    test_detects_appended_zero();
    test_residue_is_zero();

    if (failures == 0) {
        printf("test_crc16: all tests passed\n");
        return 0;
    }
    printf("test_crc16: %d check(s) failed\n", failures);
    return 1;
}
