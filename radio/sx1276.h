#ifndef SX1276_H
#define SX1276_H

/* SX1276 (RFM95W) LoRa radio driver.
 *
 * The driver does not depend on a microcontroller. It talks to the chip
 * through a small set of platform functions (sx1276_hal_t): SPI transfer,
 * the NSS and RESET pins, and a delay. On STM32 they call the HAL; in the
 * host tests they drive a simulated chip. */

#include <stdint.h>

#include "sx1276_regs.h"

typedef enum {
    SX1276_OK          =  0,
    SX1276_ERR_ARGS    = -1,  /* NULL pointer or value out of range */
    SX1276_ERR_SPI     = -2,  /* the platform SPI transfer failed */
    SX1276_ERR_VERSION = -3,  /* RegVersion is not 0x12: no chip or bad wiring */
    SX1276_ERR_MODE    = -4   /* the chip did not switch to LoRa mode */
} sx1276_status_t;

/* Platform functions. All of them are required.
 *
 * spi_transfer  sends len bytes and receives len bytes at the same time.
 *               tx == NULL means send zeros; rx == NULL means discard
 *               what was received. Returns 0 on success.
 * set_nss       drives the chip-select pin: 0 = selected, 1 = released.
 * set_reset     drives the RESET pin: 0 = in reset, 1 = running.
 * delay_ms      blocking delay in milliseconds.
 * ctx           passed back to every function (e.g. a SPI handle). */
typedef struct {
    void *ctx;
    int  (*spi_transfer)(void *ctx, const uint8_t *tx, uint8_t *rx, uint16_t len);
    void (*set_nss)(void *ctx, int level);
    void (*set_reset)(void *ctx, int level);
    void (*delay_ms)(void *ctx, uint32_t ms);
} sx1276_hal_t;

/* Radio settings. Bandwidth (125 kHz) and coding rate (4/5) are fixed
 * for now; payload CRC is always on, header mode is explicit. */
typedef struct {
    uint32_t frequency_hz;      /* carrier, 137 MHz .. 1020 MHz, e.g. 868000000 */
    uint8_t  spreading_factor;  /* 7 .. 12 */
    uint8_t  tx_power_dbm;      /* 2 .. 17 dBm on PA_BOOST */
    uint16_t preamble_len;      /* preamble length in symbols, usually 8 */
    uint8_t  sync_word;         /* SX1276_SYNC_WORD_PRIVATE for our network */
} sx1276_config_t;

/* One radio chip. */
typedef struct {
    const sx1276_hal_t *hal;
    uint8_t version;            /* RegVersion read by sx1276_probe() */
} sx1276_t;

/* Default settings: 868 MHz, SF7, 10 dBm (low power for bench tests),
 * 8-symbol preamble, private sync word. */
sx1276_config_t sx1276_default_config(void);

/* ---- Register access ---- */
int sx1276_read_reg(sx1276_t *dev, uint8_t addr, uint8_t *value);
int sx1276_write_reg(sx1276_t *dev, uint8_t addr, uint8_t value);
/* Burst access: len bytes from/to consecutive registers, or from/to the
 * FIFO when addr is SX1276_REG_FIFO. */
int sx1276_read_burst(sx1276_t *dev, uint8_t addr, uint8_t *buf, uint16_t len);
int sx1276_write_burst(sx1276_t *dev, uint8_t addr, const uint8_t *buf, uint16_t len);

/* ---- Chip control ---- */
/* Pulses RESET low for 1 ms and waits 10 ms for the chip to start. */
void sx1276_reset(sx1276_t *dev);
/* Binds the driver to hal, resets the chip and checks RegVersion == 0x12.
 * This is the first test on new hardware. */
int sx1276_probe(sx1276_t *dev, const sx1276_hal_t *hal);
/* Full start-up: probe, switch to LoRa mode, apply cfg, go to standby. */
int sx1276_init(sx1276_t *dev, const sx1276_hal_t *hal, const sx1276_config_t *cfg);

/* Sets the operating mode (SX1276_MODE_SLEEP, _STDBY, _TX, _RX_CONTINUOUS...),
 * keeping the chip in LoRa mode. */
int sx1276_set_mode(sx1276_t *dev, uint8_t mode);

/* ---- Settings (can be changed after init, in standby) ---- */
int sx1276_set_frequency(sx1276_t *dev, uint32_t frequency_hz);
int sx1276_set_tx_power(sx1276_t *dev, uint8_t dbm);
int sx1276_set_spreading_factor(sx1276_t *dev, uint8_t sf);

/* FRF register value for a carrier frequency: f * 2^19 / 32 MHz.
 * 868 MHz -> 0xD90000. Pure function, no chip access. */
uint32_t sx1276_frf_from_hz(uint32_t frequency_hz);

#endif /* SX1276_H */
