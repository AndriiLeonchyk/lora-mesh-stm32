#ifndef SX1276_PORT_H
#define SX1276_PORT_H

/* STM32 (Blue Pill) implementation of the SX1276 driver's platform
 * functions: SPI1, NSS = PA4, RESET = PB0, delays from HAL_Delay. */

#include "sx1276.h"

const sx1276_hal_t *sx1276_port_hal(void);

#endif /* SX1276_PORT_H */
