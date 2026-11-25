/* Host tests for the SX1276 driver.
 *
 * The real chip is replaced by a simulated one (mock): 128 registers and
 * a 256-byte FIFO behind a fake SPI bus. The mock follows the datasheet
 * rules the driver depends on:
 *   - the first byte after NSS goes low is the address, bit 7 = write;
 *   - the address auto-increments, except for RegFifo (0x00), which
 *     reads/writes the FIFO at RegFifoAddrPtr and advances the pointer;
 *   - a RESET pulse restores the reset values (FSK mode, standby);
 *   - the LoRa bit in RegOpMode changes only while the chip sleeps. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "sx1276.h"

static int failures = 0;

#define CHECK(cond)                                                  \
    do {                                                             \
        if (!(cond)) {                                               \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);   \
            failures++;                                              \
        }                                                            \
    } while (0)

/* ---------------- Simulated chip ---------------- */

typedef struct {
    uint8_t  regs[128];
    uint8_t  fifo[256];
    uint8_t  version_after_reset;  /* 0x12 for a real chip */
    int      nss;                  /* current NSS level */
    int      reset;                /* current RESET level */
    int      have_addr;            /* address byte already received */
    uint8_t  addr;
    int      write;
    int      reset_pulses;         /* completed low -> high RESET pulses */
    uint32_t delay_total_ms;
    int      spi_fail;             /* make spi_transfer fail */
    int      spi_without_nss;      /* bytes sent while NSS was high (bug) */
    int      transactions;         /* number of NSS low -> high cycles */
    uint8_t  first_bytes[64];      /* address byte of each transaction */
    int      n_first;
} mock_chip_t;

static void mock_power_on(mock_chip_t *m)
{
    memset(m->regs, 0, sizeof(m->regs));
    m->regs[SX1276_REG_OP_MODE] = 0x09;           /* FSK, LF mode, standby */
    m->regs[SX1276_REG_FRF_MSB] = 0x6C;           /* 434 MHz reset value */
    m->regs[SX1276_REG_FRF_MID] = 0x80;
    m->regs[SX1276_REG_PA_CONFIG] = 0x4F;
    m->regs[SX1276_REG_VERSION] = m->version_after_reset;
}

static void mock_init(mock_chip_t *m, uint8_t version)
{
    memset(m, 0, sizeof(*m));
    m->version_after_reset = version;
    m->nss = 1;
    m->reset = 1;
    mock_power_on(m);
}

static void mock_write(mock_chip_t *m, uint8_t value)
{
    if (m->addr == SX1276_REG_FIFO) {
        m->fifo[m->regs[SX1276_REG_FIFO_ADDR_PTR]++] = value;
        return;
    }
    if (m->addr == SX1276_REG_OP_MODE) {
        uint8_t old = m->regs[SX1276_REG_OP_MODE];
        int sleeping = (old & SX1276_MODE_MASK) == SX1276_MODE_SLEEP;
        if (!sleeping) {
            /* LongRangeMode bit is kept unless the chip is asleep. */
            value = (uint8_t)((value & ~SX1276_MODE_LONG_RANGE) |
                              (old & SX1276_MODE_LONG_RANGE));
        }
    }
    if (m->addr != SX1276_REG_VERSION) {          /* read-only register */
        m->regs[m->addr] = value;
    }
    m->addr = (uint8_t)((m->addr + 1u) & SX1276_SPI_ADDR_MASK);
}

static uint8_t mock_read(mock_chip_t *m)
{
    if (m->addr == SX1276_REG_FIFO) {
        return m->fifo[m->regs[SX1276_REG_FIFO_ADDR_PTR]++];
    }
    uint8_t v = m->regs[m->addr];
    m->addr = (uint8_t)((m->addr + 1u) & SX1276_SPI_ADDR_MASK);
    return v;
}

static int mock_spi_transfer(void *ctx, const uint8_t *tx, uint8_t *rx, uint16_t len)
{
    mock_chip_t *m = ctx;
    if (m->spi_fail) {
        return -1;
    }
    for (uint16_t i = 0; i < len; i++) {
        uint8_t out = tx ? tx[i] : 0x00;
        uint8_t in = 0x00;
        if (m->nss != 0) {
            m->spi_without_nss++;
        } else if (!m->have_addr) {
            m->have_addr = 1;
            m->write = (out & SX1276_SPI_WRITE) != 0;
            m->addr = (uint8_t)(out & SX1276_SPI_ADDR_MASK);
            if (m->n_first < (int)sizeof(m->first_bytes)) {
                m->first_bytes[m->n_first++] = out;
            }
        } else if (m->write) {
            mock_write(m, out);
        } else {
            in = mock_read(m);
        }
        if (rx) {
            rx[i] = in;
        }
    }
    return 0;
}

static void mock_set_nss(void *ctx, int level)
{
    mock_chip_t *m = ctx;
    if (m->nss == 0 && level != 0) {
        m->transactions++;
    }
    m->nss = level;
    m->have_addr = 0;     /* every transaction starts with an address */
}

static void mock_set_reset(void *ctx, int level)
{
    mock_chip_t *m = ctx;
    if (m->reset == 0 && level != 0) {
        m->reset_pulses++;
        mock_power_on(m);
    }
    m->reset = level;
}

static void mock_delay_ms(void *ctx, uint32_t ms)
{
    mock_chip_t *m = ctx;
    m->delay_total_ms += ms;
}

static sx1276_hal_t mock_hal(mock_chip_t *m)
{
    sx1276_hal_t hal = { m, mock_spi_transfer, mock_set_nss, mock_set_reset, mock_delay_ms };
    return hal;
}

/* ---------------- Tests ---------------- */

static void test_frf_calculation(void)
{
    CHECK(sx1276_frf_from_hz(868000000u) == SX1276_FRF_868MHZ);  /* 0xD90000 */
    CHECK(sx1276_frf_from_hz(915000000u) == 0xE4C000u);
    CHECK(sx1276_frf_from_hz(433000000u) == 0x6C4000u);
    /* One step is about 61 Hz. */
    CHECK(sx1276_frf_from_hz(868000061u) == SX1276_FRF_868MHZ);
    CHECK(sx1276_frf_from_hz(868000062u) == SX1276_FRF_868MHZ + 1u);
}

static void test_read_reg_uses_read_bit(void)
{
    mock_chip_t m;
    mock_init(&m, 0x12);
    sx1276_hal_t hal = mock_hal(&m);
    sx1276_t dev = { &hal, 0 };

    m.regs[0x1D] = 0xA5;
    uint8_t v = 0;
    CHECK(sx1276_read_reg(&dev, 0x1D, &v) == SX1276_OK);
    CHECK(v == 0xA5);
    CHECK(m.n_first == 1 && m.first_bytes[0] == 0x1D);   /* bit 7 = 0 */
    CHECK(m.transactions == 1);
    CHECK(m.nss == 1);                                    /* released */
    CHECK(m.spi_without_nss == 0);
}

static void test_write_reg_uses_write_bit(void)
{
    mock_chip_t m;
    mock_init(&m, 0x12);
    sx1276_hal_t hal = mock_hal(&m);
    sx1276_t dev = { &hal, 0 };

    CHECK(sx1276_write_reg(&dev, 0x39, 0x34) == SX1276_OK);
    CHECK(m.regs[0x39] == 0x34);
    CHECK(m.n_first == 1 && m.first_bytes[0] == (0x39 | 0x80));
    CHECK(m.nss == 1);
}

static void test_burst_registers_and_fifo(void)
{
    mock_chip_t m;
    mock_init(&m, 0x12);
    sx1276_hal_t hal = mock_hal(&m);
    sx1276_t dev = { &hal, 0 };

    /* Consecutive registers: one transaction, address auto-increments. */
    const uint8_t frf[3] = { 0xD9, 0x00, 0x00 };
    CHECK(sx1276_write_burst(&dev, SX1276_REG_FRF_MSB, frf, 3) == SX1276_OK);
    CHECK(m.transactions == 1);
    CHECK(m.regs[0x06] == 0xD9 && m.regs[0x07] == 0x00 && m.regs[0x08] == 0x00);

    uint8_t back[3] = { 0 };
    CHECK(sx1276_read_burst(&dev, SX1276_REG_FRF_MSB, back, 3) == SX1276_OK);
    CHECK(memcmp(back, frf, 3) == 0);

    /* FIFO: the address stays 0x00, the FIFO pointer advances. */
    const uint8_t data[5] = { 1, 2, 3, 4, 5 };
    CHECK(sx1276_write_reg(&dev, SX1276_REG_FIFO_ADDR_PTR, 0x00) == SX1276_OK);
    CHECK(sx1276_write_burst(&dev, SX1276_REG_FIFO, data, 5) == SX1276_OK);
    CHECK(memcmp(m.fifo, data, 5) == 0);
    CHECK(m.regs[SX1276_REG_FIFO_ADDR_PTR] == 5);

    uint8_t got[5] = { 0 };
    CHECK(sx1276_write_reg(&dev, SX1276_REG_FIFO_ADDR_PTR, 0x00) == SX1276_OK);
    CHECK(sx1276_read_burst(&dev, SX1276_REG_FIFO, got, 5) == SX1276_OK);
    CHECK(memcmp(got, data, 5) == 0);
}

static void test_probe_ok(void)
{
    mock_chip_t m;
    mock_init(&m, 0x12);
    sx1276_hal_t hal = mock_hal(&m);
    sx1276_t dev;

    CHECK(sx1276_probe(&dev, &hal) == SX1276_OK);
    CHECK(dev.version == 0x12);
    CHECK(m.reset_pulses == 1);       /* chip was reset first */
    CHECK(m.reset == 1);              /* and left running */
    CHECK(m.delay_total_ms >= 6);     /* >= 100 us low + 5 ms start-up */
    CHECK(m.nss == 1);
}

static void test_probe_detects_missing_chip(void)
{
    /* No chip / MISO not connected: the bus reads 0x00 or 0xFF. */
    const uint8_t bad[] = { 0x00, 0xFF, 0x11 };
    for (size_t i = 0; i < sizeof(bad); i++) {
        mock_chip_t m;
        mock_init(&m, bad[i]);
        sx1276_hal_t hal = mock_hal(&m);
        sx1276_t dev;
        CHECK(sx1276_probe(&dev, &hal) == SX1276_ERR_VERSION);
        CHECK(dev.version == bad[i]);
    }
}

static void test_init_configures_lora(void)
{
    mock_chip_t m;
    mock_init(&m, 0x12);
    sx1276_hal_t hal = mock_hal(&m);
    sx1276_t dev;
    sx1276_config_t cfg = sx1276_default_config();

    CHECK(sx1276_init(&dev, &hal, &cfg) == SX1276_OK);
    /* LoRa mode, HF band (LowFrequencyModeOn = 0), standby. */
    CHECK(m.regs[SX1276_REG_OP_MODE] == (SX1276_MODE_LONG_RANGE | SX1276_MODE_STDBY));
    /* 868 MHz */
    CHECK(m.regs[SX1276_REG_FRF_MSB] == 0xD9);
    CHECK(m.regs[SX1276_REG_FRF_MID] == 0x00);
    CHECK(m.regs[SX1276_REG_FRF_LSB] == 0x00);
    /* PA_BOOST, 10 dBm = 2 + 8 */
    CHECK(m.regs[SX1276_REG_PA_CONFIG] == 0xF8);
    CHECK(m.regs[SX1276_REG_LNA] == 0x23);
    CHECK(m.regs[SX1276_REG_FIFO_TX_BASE_ADDR] == 0x00);
    CHECK(m.regs[SX1276_REG_FIFO_RX_BASE_ADDR] == 0x00);
    /* BW 125 kHz, CR 4/5, explicit header */
    CHECK(m.regs[SX1276_REG_MODEM_CONFIG_1] == 0x72);
    /* SF7, payload CRC on */
    CHECK(m.regs[SX1276_REG_MODEM_CONFIG_2] == 0x74);
    /* AGC on, no low data rate optimize at SF7 */
    CHECK(m.regs[SX1276_REG_MODEM_CONFIG_3] == 0x04);
    CHECK(m.regs[SX1276_REG_PREAMBLE_MSB] == 0x00);
    CHECK(m.regs[SX1276_REG_PREAMBLE_LSB] == 0x08);
    CHECK(m.regs[SX1276_REG_SYNC_WORD] == 0x12);
    CHECK(m.spi_without_nss == 0);
    CHECK(m.nss == 1);
}

static void test_init_sf12_enables_low_data_rate_optimize(void)
{
    mock_chip_t m;
    mock_init(&m, 0x12);
    sx1276_hal_t hal = mock_hal(&m);
    sx1276_t dev;
    sx1276_config_t cfg = sx1276_default_config();
    cfg.spreading_factor = 12;
    cfg.tx_power_dbm = 17;

    CHECK(sx1276_init(&dev, &hal, &cfg) == SX1276_OK);
    CHECK(m.regs[SX1276_REG_MODEM_CONFIG_2] == 0xC4);   /* SF12 + CRC */
    CHECK(m.regs[SX1276_REG_MODEM_CONFIG_3] == 0x0C);   /* AGC + LDRO */
    CHECK(m.regs[SX1276_REG_PA_CONFIG] == 0xFF);        /* 17 dBm */

    /* SF10 does not need it, SF11 does. */
    CHECK(sx1276_set_spreading_factor(&dev, 10) == SX1276_OK);
    CHECK(m.regs[SX1276_REG_MODEM_CONFIG_3] == 0x04);
    CHECK(sx1276_set_spreading_factor(&dev, 11) == SX1276_OK);
    CHECK(m.regs[SX1276_REG_MODEM_CONFIG_3] == 0x0C);
}

static void test_lora_bit_needs_sleep(void)
{
    /* Setting the LoRa bit directly from standby does nothing on a real
     * chip; the driver must go through sleep. Check the mock agrees. */
    mock_chip_t m;
    mock_init(&m, 0x12);
    sx1276_hal_t hal = mock_hal(&m);
    sx1276_t dev = { &hal, 0 };

    CHECK(sx1276_set_mode(&dev, SX1276_MODE_STDBY) == SX1276_OK);
    CHECK((m.regs[SX1276_REG_OP_MODE] & SX1276_MODE_LONG_RANGE) == 0);

    sx1276_t dev2;
    sx1276_config_t cfg = sx1276_default_config();
    CHECK(sx1276_init(&dev2, &hal, &cfg) == SX1276_OK);
    CHECK((m.regs[SX1276_REG_OP_MODE] & SX1276_MODE_LONG_RANGE) != 0);
}

static void test_set_mode_keeps_lora(void)
{
    mock_chip_t m;
    mock_init(&m, 0x12);
    sx1276_hal_t hal = mock_hal(&m);
    sx1276_t dev;
    sx1276_config_t cfg = sx1276_default_config();
    CHECK(sx1276_init(&dev, &hal, &cfg) == SX1276_OK);

    CHECK(sx1276_set_mode(&dev, SX1276_MODE_RX_CONTINUOUS) == SX1276_OK);
    CHECK(m.regs[SX1276_REG_OP_MODE] == 0x85);
    CHECK(sx1276_set_mode(&dev, SX1276_MODE_SLEEP) == SX1276_OK);
    CHECK(m.regs[SX1276_REG_OP_MODE] == 0x80);
}

static void test_argument_checks(void)
{
    mock_chip_t m;
    mock_init(&m, 0x12);
    sx1276_hal_t hal = mock_hal(&m);
    sx1276_t dev;
    sx1276_config_t cfg = sx1276_default_config();
    uint8_t v;

    CHECK(sx1276_probe(NULL, &hal) == SX1276_ERR_ARGS);
    CHECK(sx1276_probe(&dev, NULL) == SX1276_ERR_ARGS);
    sx1276_hal_t partial = hal;
    partial.delay_ms = NULL;
    CHECK(sx1276_probe(&dev, &partial) == SX1276_ERR_ARGS);
    CHECK(sx1276_init(&dev, &hal, NULL) == SX1276_ERR_ARGS);

    CHECK(sx1276_init(&dev, &hal, &cfg) == SX1276_OK);
    CHECK(sx1276_read_reg(&dev, 0x42, NULL) == SX1276_ERR_ARGS);
    CHECK(sx1276_read_reg(NULL, 0x42, &v) == SX1276_ERR_ARGS);
    CHECK(sx1276_write_burst(&dev, 0x00, NULL, 3) == SX1276_ERR_ARGS);

    CHECK(sx1276_set_tx_power(&dev, 1) == SX1276_ERR_ARGS);
    CHECK(sx1276_set_tx_power(&dev, 18) == SX1276_ERR_ARGS);
    CHECK(sx1276_set_tx_power(&dev, 2) == SX1276_OK);
    CHECK(m.regs[SX1276_REG_PA_CONFIG] == 0xF0);

    CHECK(sx1276_set_spreading_factor(&dev, 6) == SX1276_ERR_ARGS);
    CHECK(sx1276_set_spreading_factor(&dev, 13) == SX1276_ERR_ARGS);

    CHECK(sx1276_set_frequency(&dev, 100000000u) == SX1276_ERR_ARGS);
    CHECK(sx1276_set_frequency(&dev, 1100000000u) == SX1276_ERR_ARGS);

    sx1276_config_t bad = cfg;
    bad.spreading_factor = 5;
    CHECK(sx1276_init(&dev, &hal, &bad) == SX1276_ERR_ARGS);
    bad = cfg;
    bad.tx_power_dbm = 20;
    CHECK(sx1276_init(&dev, &hal, &bad) == SX1276_ERR_ARGS);
}

static void test_spi_failure_releases_nss(void)
{
    mock_chip_t m;
    mock_init(&m, 0x12);
    sx1276_hal_t hal = mock_hal(&m);
    sx1276_t dev = { &hal, 0 };
    uint8_t v;

    m.spi_fail = 1;
    CHECK(sx1276_read_reg(&dev, 0x42, &v) == SX1276_ERR_SPI);
    CHECK(m.nss == 1);              /* chip is not left selected */

    sx1276_t dev2;
    CHECK(sx1276_probe(&dev2, &hal) == SX1276_ERR_SPI);
    CHECK(m.nss == 1);
}

int main(void)
{
    test_frf_calculation();
    test_read_reg_uses_read_bit();
    test_write_reg_uses_write_bit();
    test_burst_registers_and_fifo();
    test_probe_ok();
    test_probe_detects_missing_chip();
    test_init_configures_lora();
    test_init_sf12_enables_low_data_rate_optimize();
    test_lora_bit_needs_sleep();
    test_set_mode_keeps_lora();
    test_argument_checks();
    test_spi_failure_releases_nss();

    if (failures == 0) {
        printf("test_sx1276: all tests passed\n");
        return 0;
    }
    printf("test_sx1276: %d failure(s)\n", failures);
    return 1;
}
