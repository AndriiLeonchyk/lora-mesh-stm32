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
(steps)

## Test results
- Implement packet serialization with CRC16 and unit tests. All tests passed.

## Limitations
- Early-stage educational project, not production-ready
- Until the hardware is in place, mesh logic is verified only in simulation, not over real radio
- Real-world range and reliability have not been measured yet
- Encryption design is not security-audited

## License
MIT