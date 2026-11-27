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
- Default channel is **868.1 MHz**, fully inside the 868.0-868.6 MHz sub-band
  (1 % duty cycle). A 125 kHz signal centred on exactly 868.0 MHz would
  spill below the sub-band edge.
- Duty cycle: a short ping (about 10 bytes) takes about 40 ms on air at SF7,
  so it may be sent at most every 4 s; the test firmware uses 5 s. Check
  other payloads and spreading factors with `tools/lora_airtime.py`.
- `0xFF` / `0x00` from any register usually means a wiring problem, not a
  driver bug (see the troubleshooting section below).

## Bring-up log and troubleshooting

Problems met while bringing up the first nodes, how they were found and fixed.

### ST-Link cannot connect: "Unable to get core ID"

- **Symptom:** STM32CubeProgrammer sees the target voltage (3.26 V) but reports
  "Unable to get core ID / No STM32 target found".
- **Fix:** reseat the SWDIO, SWCLK and GND wires, lower the SWD frequency
  to 480 kHz and connect in **Under reset** mode while holding the RESET button.
- **Prevention:** keep **SYS → Debug = Serial Wire** in STM32CubeMX, so the
  firmware never disables the SWD pins.

### Radio not detected: RegVersion reads 0xFF

- **Symptom:** the firmware reported a start-up error (LED: 3 flashes and a
  pause = `RegVersion` read as `0xFF`). It happened with both RFM95W modules,
  including one that had worked before, after the boards were moved to a
  breadboard.
- **What was checked:** solder joints looked clean and every wire passed a
  continuity test end to end. The firmware and driver were identical to the
  version that had worked, so the code was ruled out.
- **Cause:** poor contact between the Blue Pill pins and the breadboard.
  `0xFF` means MISO is not driven by the chip at all: the SPI bus did not
  reach the module, although each wire was fine on its own.
- **Fix:** connect the module directly to the Blue Pill pins with
  female-to-female jumper wires, without the breadboard. The node then
  started normally and sent a `ping` packet every 2 s (LED toggling on every
  TxDone interrupt, which also confirms the DIO0 line).
- **Lessons:**
  - Test continuity from the **module pad to the MCU pin itself**, not to the
    end of a wire or a breadboard hole: the contact inside the breadboard is
    part of the circuit.
  - Cheap breadboards often do not grip square header pins well; SPI at
    4.5 Mbit/s needs reliable contacts. Prefer direct jumper wires or a
    soldered prototype board for radio nodes.
  - A diagnostic code is worth the effort: the LED error pattern pointed to
    the MISO path at once, with no UART adapter or debugger.
  - When a part that used to work fails, look for what the setups have in
    common (here the breadboard), not at the part itself.

### Start-up LED codes (PC13)

| LED | Meaning | Where to look |
|---|---|---|
| 3 quick flashes at start | radio found and configured | — |
| toggles every packet | TX: packet sent (TxDone); RX: packet received | — |
| 2 flashes, pause | `RegVersion` = `0x00` | 3.3 V / GND to the module, NSS (PA4), SCK (PA5) |
| 3 flashes, pause | `RegVersion` = `0xFF` | MISO (PA6), contacts, RESET (PB0) held low |
| 4 flashes, pause | other start-up error | unstable contact, SPI error, wrong chip |
