#ifndef MESH_DEDUP_H
#define MESH_DEDUP_H

#include <stdbool.h>
#include <stdint.h>

/* How many recent messages a node remembers. Each entry is 4 bytes,
 * so 32 entries cost 128 bytes of RAM. */
#ifndef MESH_DEDUP_SIZE
#define MESH_DEDUP_SIZE 32
#endif

/* A message is identified by who created it (src) and its number (msg_id). */
typedef struct {
    uint16_t src;
    uint16_t msg_id;
} mesh_seen_entry_t;

/* Fixed-size ring buffer of recently seen messages.
 * When it is full, the oldest entry is overwritten. */
typedef struct {
    mesh_seen_entry_t entries[MESH_DEDUP_SIZE];
    uint8_t count;  /* number of valid entries, 0..MESH_DEDUP_SIZE */
    uint8_t next;   /* index where the next entry will be written */
} mesh_dedup_t;

void mesh_dedup_init(mesh_dedup_t *d);

/* Returns true if (src, msg_id) is already in the cache. Does not modify it. */
bool mesh_dedup_contains(const mesh_dedup_t *d, uint16_t src, uint16_t msg_id);

/* Adds (src, msg_id) to the cache, overwriting the oldest entry when full. */
void mesh_dedup_add(mesh_dedup_t *d, uint16_t src, uint16_t msg_id);

/* Returns true if the message was seen before (a duplicate).
 * Otherwise records it and returns false. */
bool mesh_dedup_check_and_add(mesh_dedup_t *d, uint16_t src, uint16_t msg_id);

#endif /* MESH_DEDUP_H */
