# lora-mesh-stm32

Experimental LoRa mesh network with AES encryption on STM32 (C).

## Status
Work in progress (started November 2026). 
Currently done: 
- Repository structure
- Project goals and packet format design (see `docs/protocol.md`)
Planned: 
- Hardware has been ordered; firmware development starts when it arrives
- Mesh logic developed and tested in simulation first (Python / C on PC)

## Features (planned)
- SX1276 (RFM95W) driver over SPI
- Custom packet format with CRC16
- Flooding with TTL and message deduplication
- Retransmission with random backoff
- Neighbor discovery with HELLO packets
- AES-CCM payload encryption with replay protection
- Python tools for testing and measurements

## Hardware
- 2 × STM32F103C8T6 ("Blue Pill") boards
- ST-Link V2 programmer (SWD)
- 2 × HopeRF RFM95W-868 (SX1276) LoRa modules, 868 MHz
- Antennas for 868 MHz

## Packet format
(table or description)

## Build & run
### Host unit tests (PC)
Requires `gcc` on PATH (for example MSYS2 UCRT64). From the repository root:
```
.\tests\run_tests.ps1
```

### STM32 firmware
Open `platform/stm32/blink` in VS Code with the STM32Cube extension and run **Build**.

## Test results
Host unit tests for the mesh core (`core/`), run on a PC:
- `tests/test_packet.c`: packet round trip, CRC error, version, length and argument checks
- `tests/test_crc16.c`: CRC-16/CCITT-FALSE check values, every single-bit error detected, residue property
- `tests/test_packet_edge.c`: empty payload, all frame types, wire layout, field extremes, any bit flip in a frame rejected, length and size boundaries

All three suites pass. The firmware has not been tested on hardware yet.

## Limitations
- Early-stage educational project, not production-ready
- Until the hardware is in place, mesh logic is verified only in simulation, not over real radio
- Real-world range and reliability have not been measured yet
- Encryption design is not security-audited

## License
MIT