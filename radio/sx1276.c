#include "sx1276.h"

#include <stddef.h>

#define SX1276_FREQ_MIN_HZ   137000000u
#define SX1276_FREQ_MAX_HZ  1020000000u

sx1276_config_t sx1276_default_config(void)
{
    sx1276_config_t cfg;
    cfg.frequency_hz     = 868000000u;
    cfg.spreading_factor = 7;
    cfg.tx_power_dbm     = 10;
    cfg.preamble_len     = 8;
    cfg.sync_word        = SX1276_SYNC_WORD_PRIVATE;
    return cfg;
}

/* ---- Register access ---- */

/* One SPI transaction: NSS low, address byte, data bytes, NSS high.
 * Exactly one of tx / rx is used for the data phase. NSS is always
 * released, even when the transfer fails. */
static int transaction(sx1276_t *dev, uint8_t addr_byte,
                       const uint8_t *tx, uint8_t *rx, uint16_t len)
{
    if (dev == NULL || dev->hal == NULL) {
        return SX1276_ERR_ARGS;
    }
    const sx1276_hal_t *hal = dev->hal;
    int rc = 0;

    hal->set_nss(hal->ctx, 0);
    rc = hal->spi_transfer(hal->ctx, &addr_byte, NULL, 1);
    if (rc == 0 && len > 0) {
        rc = hal->spi_transfer(hal->ctx, tx, rx, len);
    }
    hal->set_nss(hal->ctx, 1);

    return (rc == 0) ? SX1276_OK : SX1276_ERR_SPI;
}

int sx1276_read_burst(sx1276_t *dev, uint8_t addr, uint8_t *buf, uint16_t len)
{
    if (buf == NULL && len > 0) {
        return SX1276_ERR_ARGS;
    }
    /* Bit 7 = 0: read. */
    return transaction(dev, (uint8_t)(addr & SX1276_SPI_ADDR_MASK), NULL, buf, len);
}

int sx1276_write_burst(sx1276_t *dev, uint8_t addr, const uint8_t *buf, uint16_t len)
{
    if (buf == NULL && len > 0) {
        return SX1276_ERR_ARGS;
    }
    /* Bit 7 = 1: write. */
    return transaction(dev, (uint8_t)(addr | SX1276_SPI_WRITE), buf, NULL, len);
}

int sx1276_read_reg(sx1276_t *dev, uint8_t addr, uint8_t *value)
{
    if (value == NULL) {
        return SX1276_ERR_ARGS;
    }
    return sx1276_read_burst(dev, addr, value, 1);
}

int sx1276_write_reg(sx1276_t *dev, uint8_t addr, uint8_t value)
{
    return sx1276_write_burst(dev, addr, &value, 1);
}

/* ---- Chip control ---- */

void sx1276_reset(sx1276_t *dev)
{
    if (dev == NULL || dev->hal == NULL) {
        return;
    }
    const sx1276_hal_t *hal = dev->hal;
    hal->set_nss(hal->ctx, 1);     /* chip not selected during reset */
    hal->set_reset(hal->ctx, 0);   /* datasheet: hold low for at least 100 us */
    hal->delay_ms(hal->ctx, 1);
    hal->set_reset(hal->ctx, 1);
    hal->delay_ms(hal->ctx, 10);   /* datasheet: chip is ready after 5 ms */
}

static int hal_is_complete(const sx1276_hal_t *hal)
{
    return hal != NULL && hal->spi_transfer != NULL && hal->set_nss != NULL &&
           hal->set_reset != NULL && hal->delay_ms != NULL;
}

int sx1276_probe(sx1276_t *dev, const sx1276_hal_t *hal)
{
    if (dev == NULL || !hal_is_complete(hal)) {
        return SX1276_ERR_ARGS;
    }
    dev->hal = hal;
    dev->version = 0;

    sx1276_reset(dev);

    int rc = sx1276_read_reg(dev, SX1276_REG_VERSION, &dev->version);
    if (rc != SX1276_OK) {
        return rc;
    }
    /* 0x00 or 0xFF usually means MISO is not connected or NSS is wrong. */
    if (dev->version != SX1276_VERSION_EXPECTED) {
        return SX1276_ERR_VERSION;
    }
    return SX1276_OK;
}

int sx1276_set_mode(sx1276_t *dev, uint8_t mode)
{
    return sx1276_write_reg(dev, SX1276_REG_OP_MODE,
                            (uint8_t)(SX1276_MODE_LONG_RANGE | (mode & SX1276_MODE_MASK)));
}

/* Switches the chip from FSK (the state after reset) to LoRa.
 * The LongRangeMode bit can only be changed in SLEEP, so the chip is
 * first put to sleep, then the bit is set, then it is read back. */
static int enter_lora_mode(sx1276_t *dev)
{
    int rc = sx1276_write_reg(dev, SX1276_REG_OP_MODE, SX1276_MODE_SLEEP);
    if (rc != SX1276_OK) {
        return rc;
    }
    rc = sx1276_set_mode(dev, SX1276_MODE_SLEEP);
    if (rc != SX1276_OK) {
        return rc;
    }
    uint8_t op = 0;
    rc = sx1276_read_reg(dev, SX1276_REG_OP_MODE, &op);
    if (rc != SX1276_OK) {
        return rc;
    }
    if ((op & SX1276_MODE_LONG_RANGE) == 0) {
        return SX1276_ERR_MODE;
    }
    return SX1276_OK;
}

uint32_t sx1276_frf_from_hz(uint32_t frequency_hz)
{
    /* 64-bit math: 1020 MHz * 2^19 does not fit in 32 bits. */
    return (uint32_t)(((uint64_t)frequency_hz << 19) / SX1276_FXOSC_HZ);
}

int sx1276_set_frequency(sx1276_t *dev, uint32_t frequency_hz)
{
    if (frequency_hz < SX1276_FREQ_MIN_HZ || frequency_hz > SX1276_FREQ_MAX_HZ) {
        return SX1276_ERR_ARGS;
    }
    uint32_t frf = sx1276_frf_from_hz(frequency_hz);
    uint8_t bytes[3] = {
        (uint8_t)(frf >> 16),   /* RegFrfMsb */
        (uint8_t)(frf >> 8),    /* RegFrfMid */
        (uint8_t)(frf)          /* RegFrfLsb */
    };
    /* The three registers are consecutive: one burst write. */
    return sx1276_write_burst(dev, SX1276_REG_FRF_MSB, bytes, 3);
}

int sx1276_set_tx_power(sx1276_t *dev, uint8_t dbm)
{
    /* PA_BOOST output: Pout = 2 + OutputPower, so 2..17 dBm.
     * (20 dBm needs RegPaDac and is not used on the bench.) */
    if (dbm < 2 || dbm > 17) {
        return SX1276_ERR_ARGS;
    }
    uint8_t value = (uint8_t)(SX1276_PA_BOOST | SX1276_PA_MAX_POWER |
                              ((dbm - 2u) & SX1276_PA_OUTPUT_MASK));
    return sx1276_write_reg(dev, SX1276_REG_PA_CONFIG, value);
}

int sx1276_set_spreading_factor(sx1276_t *dev, uint8_t sf)
{
    /* SF6 needs implicit header mode, so it is not supported here. */
    if (sf < 7 || sf > 12) {
        return SX1276_ERR_ARGS;
    }
    int rc = sx1276_write_reg(dev, SX1276_REG_MODEM_CONFIG_2,
                              (uint8_t)((sf << SX1276_SF_SHIFT) | SX1276_RX_PAYLOAD_CRC_ON));
    if (rc != SX1276_OK) {
        return rc;
    }
    /* At BW 125 kHz a symbol lasts 2^SF / 125 kHz. For SF11 and SF12 it
     * is longer than 16 ms, and the datasheet requires LowDataRateOptimize. */
    uint8_t cfg3 = SX1276_AGC_AUTO_ON;
    if (sf >= 11) {
        cfg3 |= SX1276_LOW_DATA_RATE_OPT;
    }
    return sx1276_write_reg(dev, SX1276_REG_MODEM_CONFIG_3, cfg3);
}

int sx1276_init(sx1276_t *dev, const sx1276_hal_t *hal, const sx1276_config_t *cfg)
{
    if (cfg == NULL) {
        return SX1276_ERR_ARGS;
    }
    if (cfg->spreading_factor < 7 || cfg->spreading_factor > 12 ||
        cfg->tx_power_dbm < 2 || cfg->tx_power_dbm > 17 ||
        cfg->frequency_hz < SX1276_FREQ_MIN_HZ || cfg->frequency_hz > SX1276_FREQ_MAX_HZ) {
        return SX1276_ERR_ARGS;
    }

    int rc = sx1276_probe(dev, hal);
    if (rc != SX1276_OK) return rc;

    rc = enter_lora_mode(dev);
    if (rc != SX1276_OK) return rc;

    rc = sx1276_set_frequency(dev, cfg->frequency_hz);
    if (rc != SX1276_OK) return rc;

    rc = sx1276_set_tx_power(dev, cfg->tx_power_dbm);
    if (rc != SX1276_OK) return rc;

    rc = sx1276_write_reg(dev, SX1276_REG_LNA, SX1276_LNA_MAX_GAIN_BOOST);
    if (rc != SX1276_OK) return rc;

    /* Use the whole 256-byte FIFO for both TX and RX (half-duplex radio). */
    rc = sx1276_write_reg(dev, SX1276_REG_FIFO_TX_BASE_ADDR, 0x00);
    if (rc != SX1276_OK) return rc;
    rc = sx1276_write_reg(dev, SX1276_REG_FIFO_RX_BASE_ADDR, 0x00);
    if (rc != SX1276_OK) return rc;

    /* BW 125 kHz, CR 4/5, explicit header. */
    rc = sx1276_write_reg(dev, SX1276_REG_MODEM_CONFIG_1,
                          (uint8_t)(SX1276_BW_125KHZ | SX1276_CR_4_5));
    if (rc != SX1276_OK) return rc;

    rc = sx1276_set_spreading_factor(dev, cfg->spreading_factor);
    if (rc != SX1276_OK) return rc;

    uint8_t preamble[2] = { (uint8_t)(cfg->preamble_len >> 8), (uint8_t)cfg->preamble_len };
    rc = sx1276_write_burst(dev, SX1276_REG_PREAMBLE_MSB, preamble, 2);
    if (rc != SX1276_OK) return rc;

    rc = sx1276_write_reg(dev, SX1276_REG_SYNC_WORD, cfg->sync_word);
    if (rc != SX1276_OK) return rc;

    return sx1276_set_mode(dev, SX1276_MODE_STDBY);
}
