#include "dedup.h"

#include <stddef.h>
#include <string.h>

void mesh_dedup_init(mesh_dedup_t *d)
{
    if (d == NULL) {
        return;
    }
    memset(d, 0, sizeof(*d));
}

bool mesh_dedup_contains(const mesh_dedup_t *d, uint16_t src, uint16_t msg_id)
{
    if (d == NULL) {
        return false;
    }
    for (uint8_t i = 0; i < d->count; i++) {
        if (d->entries[i].src == src && d->entries[i].msg_id == msg_id) {
            return true;
        }
    }
    return false;
}

void mesh_dedup_add(mesh_dedup_t *d, uint16_t src, uint16_t msg_id)
{
    if (d == NULL) {
        return;
    }
    d->entries[d->next].src = src;
    d->entries[d->next].msg_id = msg_id;
    d->next = (uint8_t)((d->next + 1u) % MESH_DEDUP_SIZE);
    if (d->count < MESH_DEDUP_SIZE) {
        d->count++;
    }
}

bool mesh_dedup_check_and_add(mesh_dedup_t *d, uint16_t src, uint16_t msg_id)
{
    if (mesh_dedup_contains(d, src, msg_id)) {
        return true;
    }
    mesh_dedup_add(d, src, msg_id);
    return false;
}
