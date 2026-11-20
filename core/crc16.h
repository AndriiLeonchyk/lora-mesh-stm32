#ifndef MESH_CRC16_H
#define MESH_CRC16_H

#include <stddef.h>
#include <stdint.h>

/* CRC16-CCITT: polynomial 0x1021, initial value 0xFFFF. */
uint16_t crc16_ccitt(const uint8_t *data, size_t len);

#endif /* MESH_CRC16_H */