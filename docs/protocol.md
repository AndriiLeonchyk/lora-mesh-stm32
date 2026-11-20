# Mesh protocol (draft v1)

Status: design draft, not yet implemented.

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

## HELLO frames

Each node periodically broadcasts a HELLO frame with `ttl` = 1 (not forwarded). Receivers record the sender in a neighbor table together with RSSI and a last-seen timestamp.

## Planned: encryption

- AES-CCM with a pre-shared key for the first version
- Nonce derived from `src` and a per-sender frame counter
- The `ttl` field changes at every hop, so it must be excluded from the authenticated data
- Replay protection: reject frames whose counter is not newer than the last one seen from that sender

## Open questions

- Retransmission and ACK strategy for unicast messages
- Seen-cache size and expiry
- Duty-cycle limits at 868 MHz