# lora-mesh-stm32

Experimental LoRa mesh network with AES encryption on STM32 (C).

## Status
Work in progress (started November 2026). 
Currently done: 
- Repository structure
- Project goals and packet format design (see `docs/protocol.md`)
- Packet serialization with CRC16 (`core/packet.c`)
- Mesh core: message deduplication, flooding forward decisions with TTL, random backoff (`core/dedup.c`, `core/mesh.c`)
- SX1276 register map (`radio/sx1276_regs.h`)
- LoRa time-on-air calculator (`tools/lora_airtime.py`)
- Host unit tests for all of the above
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

## Project structure
```
core/              mesh logic in portable C, no hardware dependency
  packet.c/.h      frame serialization and parsing
  crc16.c/.h       CRC-16/CCITT-FALSE
  dedup.c/.h       cache of recently seen messages
  mesh.c/.h        deliver / forward / drop decisions, backoff
radio/             SX1276 (RFM95W) driver, register map so far
platform/stm32/    STM32CubeMX project for the Blue Pill
tests/             host unit tests (run on a PC)
tools/             Python helper tools
docs/              protocol, hardware and requirements
```

## Hardware
- 2 × STM32F103C8T6 ("Blue Pill") boards
- ST-Link V2 programmer (SWD)
- 2 × HopeRF RFM95W-868 (SX1276) LoRa modules, 868 MHz
- Antennas for 868 MHz

## Packet format
Every frame has a 10-byte header, a payload and a CRC16. Multi-byte fields are little-endian.

| Field | Size | Description |
|---|---|---|
| version | 1 B | Protocol version (1) |
| type | 1 B | DATA, ACK or HELLO |
| src | 2 B | Address of the node that created the message |
| dst | 2 B | Destination address, `0xFFFF` = broadcast |
| msg_id | 2 B | Message number, used to drop duplicates |
| ttl | 1 B | Hops left |
| len | 1 B | Payload length |
| payload | 0..243 B | Data |
| crc16 | 2 B | CRC-16/CCITT-FALSE over header and payload |

Full specification and forwarding rules: [`docs/protocol.md`](docs/protocol.md).

## How a node handles a received frame
1. Check the CRC; drop the frame if it is corrupted.
2. Drop it if it is the node's own message coming back, or a message already seen (`src` + `msg_id`).
3. Deliver it to the application if it is addressed to this node or to everyone.
4. If someone else still needs it and `ttl` > 1, wait a random delay and retransmit it with `ttl` - 1.

## Build & run
### Host unit tests (PC)
Requires `gcc` on PATH (for example MSYS2 UCRT64). From the repository root:
```
.\tests\run_tests.ps1
```

### STM32 firmware
Open `platform/stm32/blink` in VS Code with the STM32Cube extension and run **Build**.

### Tools
LoRa time-on-air calculator (SF7..SF12, bit rate and packets per hour under a duty-cycle limit):
```
python tools/lora_airtime.py
python tools/lora_airtime.py --payload 60 --bw 250
```

Example for a 20-byte frame, BW 125 kHz, CR 4/5, 1 % duty cycle:

| SF | Bit rate, bit/s | Time on air, ms | Packets per hour |
|---|---|---|---|
| 7 | 5469 | 56.6 | 636 |
| 9 | 1758 | 185.3 | 194 |
| 12 | 293 | 1318.9 | 27 |

## Test results
Host unit tests, run on a PC:
- `tests/test_packet.c`: packet round trip, CRC error, version, length and argument checks
- `tests/test_crc16.c`: CRC-16/CCITT-FALSE check values, every single-bit error detected, residue property
- `tests/test_packet_edge.c`: empty payload, all frame types, wire layout, field extremes, any bit flip in a frame rejected, length and size boundaries
- `tests/test_sx1276_regs.c`: SX1276 register map constants (868 MHz frequency word, SPI read/write bit, operating modes, IRQ flags)
- `tests/test_mesh.c`: deduplication cache (including eviction), deliver/forward/drop decisions, own-packet echo, packet building and `msg_id` wrap-around, backoff range, a 4-node multi-hop line scenario

All five suites pass. The firmware has not been tested on hardware yet.

## Roadmap
- [x] Packet format with CRC16
- [x] Mesh core: deduplication, TTL flooding, random backoff
- [x] SX1276 register map
- [ ] Radio abstraction and multi-node network simulator on a PC
- [ ] SX1276 SPI driver, first test: read `RegVersion` = `0x12`
- [ ] Two-node LoRa link on real hardware, range and RSSI measurements
- [ ] HELLO frames and neighbor table
- [ ] AES-CCM encryption with replay protection

## Limitations
- Early-stage educational project, not production-ready
- Until the hardware is in place, mesh logic is verified only in simulation, not over real radio
- Real-world range and reliability have not been measured yet
- Encryption design is not security-audited

## License
MIT