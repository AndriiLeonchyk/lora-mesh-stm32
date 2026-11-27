# Non-functional requirements

| Area | Requirement |
|---|---|
| Memory | Firmware fits STM32F103C8 limits (64 KB Flash, 20 KB RAM) |
| Packet size | Max 255 bytes per LoRa frame; payload up to 235 bytes with encryption |
| Reliability | Duplicate messages are dropped; TTL limits the hop count |
| Security | Payload is encrypted and authenticated (AES-CCM); replayed packets are rejected |
| Timing | Non-blocking code: timers and interrupts instead of delays |
| Portability | `core/` has no dependency on the board or radio hardware |
| Regulatory | Transmit power and duty cycle follow local 868 MHz rules (868.0-868.6 MHz sub-band: 1 % duty cycle, at most 14 dBm ERP) |
| Testability | Core logic runs in simulation on a PC |

Status (November 2026):

- Portability and testability: met. `core/` and `radio/` have no hardware dependency and run in host tests and CI.
- Timing: the radio is non-blocking (DIO0 interrupt sets a flag, the main loop does the work); the rest of the firmware still uses blocking delays.
- Regulatory: the test firmware uses 868.1 MHz, 10 dBm and one ping every 5 s (about 0.8 % duty cycle). The firmware does not enforce the limit yet.
- Memory, reliability, security: not verified yet.