#ifndef SX1276_REGS_H
#define SX1276_REGS_H

/* SX1276 register map, LoRa mode only.
 * Source: Semtech SX1276/77/78/79 datasheet, "Registers for the LoRa Mode".
 * Used by the RFM95W module (HopeRF). */

/* SPI access: the first byte of a transaction is the register address.
 * Bit 7 = 1 for a write, 0 for a read. */
#define SX1276_SPI_WRITE            0x80u
#define SX1276_SPI_ADDR_MASK        0x7Fu

/* ---- Registers ---- */
#define SX1276_REG_FIFO                 0x00u  /* FIFO read/write access */
#define SX1276_REG_OP_MODE              0x01u  /* LoRa/FSK and operating mode */
#define SX1276_REG_FRF_MSB              0x06u  /* carrier frequency, bits 23..16 */
#define SX1276_REG_FRF_MID              0x07u  /* carrier frequency, bits 15..8 */
#define SX1276_REG_FRF_LSB              0x08u  /* carrier frequency, bits 7..0 */
#define SX1276_REG_PA_CONFIG            0x09u  /* TX power and PA_BOOST select */
#define SX1276_REG_OCP                  0x0Bu  /* over-current protection */
#define SX1276_REG_LNA                  0x0Cu  /* receiver gain */
#define SX1276_REG_FIFO_ADDR_PTR        0x0Du  /* FIFO read/write pointer */
#define SX1276_REG_FIFO_TX_BASE_ADDR    0x0Eu  /* start of TX area in FIFO */
#define SX1276_REG_FIFO_RX_BASE_ADDR    0x0Fu  /* start of RX area in FIFO */
#define SX1276_REG_FIFO_RX_CURRENT_ADDR 0x10u  /* start of last received packet */
#define SX1276_REG_IRQ_FLAGS_MASK       0x11u  /* 1 = IRQ disabled */
#define SX1276_REG_IRQ_FLAGS            0x12u  /* IRQ flags, clear by writing 1 */
#define SX1276_REG_RX_NB_BYTES          0x13u  /* length of last received packet */
#define SX1276_REG_PKT_SNR_VALUE        0x19u  /* SNR of last packet, signed, x4 */
#define SX1276_REG_PKT_RSSI_VALUE       0x1Au  /* RSSI of last packet */
#define SX1276_REG_MODEM_CONFIG_1       0x1Du  /* bandwidth, coding rate, header mode */
#define SX1276_REG_MODEM_CONFIG_2       0x1Eu  /* spreading factor, payload CRC */
#define SX1276_REG_PREAMBLE_MSB         0x20u  /* preamble length, high byte */
#define SX1276_REG_PREAMBLE_LSB         0x21u  /* preamble length, low byte */
#define SX1276_REG_PAYLOAD_LENGTH       0x22u  /* TX payload length */
#define SX1276_REG_MODEM_CONFIG_3       0x26u  /* low data rate optimize, AGC */
#define SX1276_REG_SYNC_WORD            0x39u  /* network sync word */
#define SX1276_REG_DIO_MAPPING_1        0x40u  /* DIO0..DIO3 function */
#define SX1276_REG_VERSION              0x42u  /* silicon version */

/* ---- RegOpMode (0x01) ---- */
#define SX1276_MODE_LONG_RANGE      0x80u  /* bit 7: 1 = LoRa (change only in SLEEP) */
#define SX1276_MODE_MASK            0x07u  /* bits 2..0: operating mode */
#define SX1276_MODE_SLEEP           0x00u
#define SX1276_MODE_STDBY           0x01u
#define SX1276_MODE_TX              0x03u
#define SX1276_MODE_RX_CONTINUOUS   0x05u
#define SX1276_MODE_RX_SINGLE       0x06u
#define SX1276_MODE_CAD             0x07u

/* ---- RegIrqFlags (0x12) ---- */
#define SX1276_IRQ_RX_TIMEOUT           0x80u
#define SX1276_IRQ_RX_DONE              0x40u
#define SX1276_IRQ_PAYLOAD_CRC_ERROR    0x20u
#define SX1276_IRQ_VALID_HEADER         0x10u
#define SX1276_IRQ_TX_DONE              0x08u
#define SX1276_IRQ_CAD_DONE             0x04u
#define SX1276_IRQ_FHSS_CHANGE_CHANNEL  0x02u
#define SX1276_IRQ_CAD_DETECTED         0x01u
#define SX1276_IRQ_ALL                  0xFFu

/* ---- RegDioMapping1 (0x40), DIO0 = bits 7..6 ---- */
#define SX1276_DIO0_RX_DONE         0x00u
#define SX1276_DIO0_TX_DONE         0x40u
#define SX1276_DIO0_CAD_DONE        0x80u

/* ---- Constants ---- */
#define SX1276_VERSION_EXPECTED     0x12u   /* RegVersion of a working SX1276 */
#define SX1276_FIFO_SIZE            256u
#define SX1276_FXOSC_HZ             32000000u
/* Frequency step = FXOSC / 2^19 (about 61.035 Hz). FRF = f_carrier / step. */
#define SX1276_FRF_868MHZ           0xD90000u

#endif /* SX1276_REGS_H */
