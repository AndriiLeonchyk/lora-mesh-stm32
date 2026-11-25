#include "sx1276_port.h"

#include <string.h>

#include "main.h"

/* Created by STM32CubeMX in main.c. */
extern SPI_HandleTypeDef hspi1;

#define SPI_TIMEOUT_MS 100u

static int port_spi_transfer(void *ctx, const uint8_t *tx, uint8_t *rx, uint16_t len)
{
    SPI_HandleTypeDef *spi = (SPI_HandleTypeDef *)ctx;
    HAL_StatusTypeDef st = HAL_OK;

    if (tx != NULL && rx != NULL) {
        st = HAL_SPI_TransmitReceive(spi, (uint8_t *)tx, rx, len, SPI_TIMEOUT_MS);
    } else if (tx != NULL) {
        st = HAL_SPI_Transmit(spi, (uint8_t *)tx, len, SPI_TIMEOUT_MS);
    } else if (rx != NULL) {
        /* In full-duplex master mode HAL_SPI_Receive sends the contents of
         * rx while it receives, so clear it to send zeros. */
        memset(rx, 0, len);
        st = HAL_SPI_Receive(spi, rx, len, SPI_TIMEOUT_MS);
    } else {
        uint8_t zero = 0;
        for (uint16_t i = 0; i < len && st == HAL_OK; i++) {
            st = HAL_SPI_Transmit(spi, &zero, 1, SPI_TIMEOUT_MS);
        }
    }
    return (st == HAL_OK) ? 0 : -1;
}

static void port_set_nss(void *ctx, int level)
{
    (void)ctx;
    HAL_GPIO_WritePin(RADIO_NSS_GPIO_Port, RADIO_NSS_Pin,
                      level ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

static void port_set_reset(void *ctx, int level)
{
    (void)ctx;
    HAL_GPIO_WritePin(RADIO_RESET_GPIO_Port, RADIO_RESET_Pin,
                      level ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

static void port_delay_ms(void *ctx, uint32_t ms)
{
    (void)ctx;
    HAL_Delay(ms);
}

static const sx1276_hal_t port_hal = {
    .ctx          = &hspi1,
    .spi_transfer = port_spi_transfer,
    .set_nss      = port_set_nss,
    .set_reset    = port_set_reset,
    .delay_ms     = port_delay_ms,
};

const sx1276_hal_t *sx1276_port_hal(void)
{
    return &port_hal;
}
