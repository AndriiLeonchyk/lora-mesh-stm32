# Hardware

## Components

- 2 × STM32F103C8T6 "Blue Pill" board (8 MHz crystal, runs at 72 MHz)
- ST-Link V2 programmer (SWD)
- 2 × HopeRF RFM95W-868 (SX1276) LoRa module, 868 MHz
- 2 × 868 MHz antenna (a quarter-wave wire of about 8.2 cm works for bench tests)
- Adapter boards for the RFM95W (2 mm pitch, SMD module)
- Planned for the receiver node: 1602 LCD with an I2C backpack (PCF8574T), RGB LED

## Programming (ST-Link V2, SWD)

| ST-Link V2 | Blue Pill | Note |
|---|---|---|
| 3.3V | 3.3 | Powers the board while programming |
| SWDIO | DIO (PA13) | |
| SWCLK | DCLK (PA14) | |
| GND | GND | |

In STM32CubeMX, **SYS → Debug** must stay set to **Serial Wire**, otherwise
the firmware turns off the SWD pins. If the programmer cannot connect
("Unable to get core ID"), lower the SWD frequency to 480 kHz and connect
in **Under reset** mode while holding the RESET button.

## Pinout

All pins below are configured in `platform/stm32/blink/blink.ioc`.

### Radio (RFM95W over SPI1)

| RFM95W | Blue Pill | Note |
|---|---|---|
| 3.3V | 3.3V | Power (3.3 V only) |
| GND | GND | |
| SCK | PA5 | SPI1 SCK |
| MISO | PA6 | SPI1 MISO |
| MOSI | PA7 | SPI1 MOSI |
| NSS | PA4 | Chip select, software controlled, high (inactive) at start |
| RESET | PB0 | Output, high at start (module not in reset) |
| DIO0 | PB1 | EXTI1, rising edge, pull-down: TX done / RX done interrupt |

SPI1: master, mode 0 (CPOL 0, CPHA 0), MSB first, 8-bit, prescaler 16 = 4.5 Mbit/s.

### Other pins

| Function | Pin | Note |
|---|---|---|
| On-board LED | PC13 | Active low (LED is on when the pin is 0) |
| UART TX (debug log) | PA9 | USART1, 115200 8N1 |
| UART RX | PA10 | USART1 |
| 8 MHz crystal | PD0 / PD1 | HSE, PLL ×9 = 72 MHz |
| LCD I2C SCL (planned) | PB6 | I2C1, 1602 LCD + PCF8574T, address 0x27, LCD powered from 5V |
| LCD I2C SDA (planned) | PB7 | I2C1 |
| RGB LED (planned) | to be chosen | Timer PWM pins, one resistor per color |

## Clock

HSE 8 MHz → PLL ×9 → SYSCLK 72 MHz. AHB 72 MHz, APB1 36 MHz (/2), APB2 72 MHz, Flash latency 2.

## Notes

- Never power the radio without an antenna connected.
- Use low TX power (about 10-13 dBm) on the bench.
- A 100 nF capacitor close to the module power pins is recommended.
- First hardware check: read `RegVersion` (0x42), the expected value is 0x12.
