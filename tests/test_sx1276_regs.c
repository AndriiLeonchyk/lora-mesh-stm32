/* Sanity checks for the SX1276 register map constants. Runs on a PC. */
#include <stdint.h>
#include <stdio.h>

#include "sx1276_regs.h"

static int failures = 0;

#define CHECK(cond)                                                  \
    do {                                                             \
        if (!(cond)) {                                               \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);   \
            failures++;                                              \
        }                                                            \
    } while (0)

/* FRF = f / (FXOSC / 2^19) = f * 2^19 / FXOSC */
static uint32_t frf_for(uint64_t freq_hz)
{
    return (uint32_t)((freq_hz << 19) / SX1276_FXOSC_HZ);
}

static void test_frequency_constant(void)
{
    CHECK(frf_for(868000000u) == SX1276_FRF_868MHZ);
    CHECK(((SX1276_FRF_868MHZ >> 16) & 0xFFu) == 0xD9u); /* RegFrfMsb */
    CHECK(((SX1276_FRF_868MHZ >> 8) & 0xFFu) == 0x00u);  /* RegFrfMid */
    CHECK((SX1276_FRF_868MHZ & 0xFFu) == 0x00u);         /* RegFrfLsb */
}

static void test_spi_address_byte(void)
{
    /* every register address fits in 7 bits, so the R/W bit is free */
    CHECK((SX1276_REG_VERSION & SX1276_SPI_WRITE) == 0);
    CHECK((SX1276_REG_OP_MODE | SX1276_SPI_WRITE) == 0x81u);   /* write */
    CHECK((SX1276_REG_VERSION & SX1276_SPI_ADDR_MASK) == 0x42u); /* read */
}

static void test_op_mode_values(void)
{
    CHECK((SX1276_MODE_LONG_RANGE | SX1276_MODE_SLEEP) == 0x80u);
    CHECK((SX1276_MODE_LONG_RANGE | SX1276_MODE_STDBY) == 0x81u);
    CHECK((SX1276_MODE_LONG_RANGE | SX1276_MODE_TX) == 0x83u);
    CHECK((SX1276_MODE_LONG_RANGE | SX1276_MODE_RX_CONTINUOUS) == 0x85u);
    CHECK((SX1276_MODE_RX_CONTINUOUS & ~SX1276_MODE_MASK) == 0);
}

static void test_irq_flags_are_distinct_bits(void)
{
    const uint8_t flags[] = {
        SX1276_IRQ_RX_TIMEOUT, SX1276_IRQ_RX_DONE, SX1276_IRQ_PAYLOAD_CRC_ERROR,
        SX1276_IRQ_VALID_HEADER, SX1276_IRQ_TX_DONE, SX1276_IRQ_CAD_DONE,
        SX1276_IRQ_FHSS_CHANGE_CHANNEL, SX1276_IRQ_CAD_DETECTED,
    };
    uint8_t all = 0;
    for (unsigned i = 0; i < sizeof(flags); i++) {
        CHECK((all & flags[i]) == 0);  /* no overlap */
        all = (uint8_t)(all | flags[i]);
    }
    CHECK(all == SX1276_IRQ_ALL);
}

int main(void)
{
    test_frequency_constant();
    test_spi_address_byte();
    test_op_mode_values();
    test_irq_flags_are_distinct_bits();

    if (failures == 0) {
        printf("test_sx1276_regs: all tests passed\n");
        return 0;
    }
    printf("test_sx1276_regs: %d check(s) failed\n", failures);
    return 1;
}
