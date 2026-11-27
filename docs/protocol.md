# Mesh protocol (draft v1)

Status: frame format, deduplication and forwarding decisions are implemented and unit-tested on a PC (`core/`). The SX1276 radio driver (`radio/`) can send and receive raw LoRa packets and has been tested on hardware; the mesh logic is not yet connected to it. HELLO handling and encryption are not implemented yet.

Radio settings: 868.1 MHz, SF7, BW 125 kHz, CR 4/5, 8-symbol preamble, explicit header, hardware payload CRC, private sync word 0x12.

## Frame format

All multi-byte fields are little-endian.

| Field | Size | Description |
|---|---|---|
| version | 1 byte | Protocol version (currently 1) |
| type | 1 byte | 0x01 DATA, 0x02 ACK, 0x03 HELLO |
| src | 2 bytes | Original sender address |
| dst | 2 bytes | Destination address; 0xFFFF = broadcast |
| msg_id | 2 bytes | Message ID, unique per sender; used for deduplication |
| ttl | 1 byte | Remaining hops |
| len | 1 byte | Payload length in bytes |
| payload | 0..243 bytes | Data (up to ~235 bytes once encryption is added) |
| crc16 | 2 bytes | CRC16-CCITT (poly 0x1021, init 0xFFFF) over header + payload |

Header is 10 bytes, CRC is 2 bytes, so the maximum payload without encryption is 255 - 12 = 243 bytes.

## Forwarding rules (flooding)

1. On receive, verify the CRC. Drop the frame if it is invalid.
2. If the pair (`src`, `msg_id`) is already in the seen-cache, drop the frame as a duplicate.
3. Otherwise add the pair to the seen-cache (fixed-size ring buffer).
4. If `dst` is this node or broadcast, deliver the payload to the application.
5. If `dst` is not this node (or is broadcast) and `ttl` > 1, decrement `ttl`, recompute the CRC, wait a random backoff delay, and retransmit.

The random backoff reduces collisions when several nodes receive the same frame and try to forward it at once.

### Implementation notes (`core/mesh.c`, `core/dedup.c`)

- A node also drops a frame whose `src` is its own address (its own message echoed back by a neighbor). Packets created with `mesh_node_build()` are added to the seen-cache at once.
- A frame with `ttl` = 0 is treated as invalid and dropped.
- A frame with an unknown `type` is rejected by the parser (`MESH_ERR_TYPE`) and cannot be serialized. Frame types are fixed per protocol version: adding a type requires a new `version`.
- The first `msg_id` after boot is passed to `mesh_node_init()`. On hardware it must be random: if every boot started from 0, neighbors that still remember the node's previous messages would drop its new ones as duplicates. A random 16-bit start makes such a collision unlikely (32 cached ids out of 65536) but not impossible; a persistent boot counter would remove it completely.
- A frame addressed to this node is delivered and never forwarded.
- The seen-cache holds the last 32 (`src`, `msg_id`) pairs (`MESH_DEDUP_SIZE`, 128 bytes of RAM). When full, the oldest entry is overwritten.
- Known limitation: the cache has no time-based expiry. In a busy network an entry can be evicted before the last copy of that message arrives, and the node then forwards it a second time. TTL still bounds this. The cache size has to be chosen for the expected traffic (to be measured on hardware).
- `mesh_node_handle_rx()` returns flags: `DELIVER`, `FORWARD`, both (broadcast), or none (drop, with a reason: invalid, own, duplicate, ttl expired). It does not touch the radio; the caller serializes the forwarded copy (which recomputes the CRC) and sends it after `mesh_backoff_ms()`.
- `mesh_backoff_ms()` takes the random number as an argument, so the same code runs on the STM32 (hardware randomness) and in PC tests.

## HELLO frames

Each node periodically broadcasts a HELLO frame with `ttl` = 1 (not forwarded). Receivers record the sender in a neighbor table together with RSSI and a last-seen timestamp.

## Planned: encryption

- AES-CCM with a pre-shared key for the first version
- Nonce derived from `src` and a per-sender frame counter
- The `ttl` field changes at every hop, so it must be excluded from the authenticated data
- Replay protection: reject frames whose counter is not newer than the last one seen from that sender

## Open questions

- Retransmission and ACK strategy for unicast messages
- Seen-cache size for the real traffic level
- Persistent boot counter instead of a random first `msg_id`
- Duty-cycle limits at 868 MHz