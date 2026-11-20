# Hardware

## Components

- 2 × STM32F103C8T6 "Blue Pill" board
- ST-Link V2 programmer (SWD)
- 2 × HopeRF RFM95W-868 (SX1276) LoRa module, 868 MHz
- 2 × 868 MHz antenna (a quarter-wave wire of about 8.2 cm works for bench tests)
- Adapter boards for the RFM95W (2 mm pitch, SMD module)

## Wiring (SPI1)

| RFM95W | Blue Pill | Note |
|---|---|---|
| 3.3V | 3.3V | Power (3.3 V only) |
| GND | GND | |
| SCK | PA5 | SPI1 SCK |
| MISO | PA6 | SPI1 MISO |
| MOSI | PA7 | SPI1 MOSI |
| NSS | PA4 | Chip select |
| RESET | PB0 | Can be changed |
| DIO0 | PB1 | RX/TX done interrupt |

Pin assignments are a plan and will be confirmed when the hardware is tested.

## Notes

- Never power the radio without an antenna connected.
- Use low TX power (about 10-13 dBm) on the bench.
- A 100 nF capacitor close to the module power pins is recommended.
- First hardware check: read `RegVersion` (0x42), the expected value is 0x12.