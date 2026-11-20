# Non-functional requirements

| Area | Requirement |
|---|---|
| Memory | Firmware fits STM32F103C8 limits (64 KB Flash, 20 KB RAM) |
| Packet size | Max 255 bytes per LoRa frame; payload up to 235 bytes with encryption |
| Reliability | Duplicate messages are dropped; TTL limits the hop count |
| Security | Payload is encrypted and authenticated (AES-CCM); replayed packets are rejected |
| Timing | Non-blocking code: timers and interrupts instead of delays |
| Portability | `core/` has no dependency on the board or radio hardware |
| Regulatory | Transmit power and duty cycle follow local 868 MHz rules |
| Testability | Core logic runs in simulation on a PC |

Status: design stage. Requirements marked here are targets, not yet verified.