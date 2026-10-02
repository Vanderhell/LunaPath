#include "esp_rom_crc.h"

#include <stddef.h>
#include <stdint.h>

/* Adapt the ROM routine's complemented API to the portable raw CRC state. */
uint32_t lunu_embed_esp_crc32_update(uint32_t raw_crc, const uint8_t *bytes, size_t length) {
    while (length != 0u) {
        uint32_t chunk = length > UINT32_MAX ? UINT32_MAX : (uint32_t)length;
        raw_crc = ~esp_rom_crc32_le(~raw_crc, bytes, chunk);
        bytes += chunk;
        length -= chunk;
    }
    return raw_crc;
}
