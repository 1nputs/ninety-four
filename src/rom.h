/* ROM data access. Phase 1 reads tables straight out of the retail ROM image; later the build
 * extracts them to asset files. Addresses come from the asm rebuild's symbols. */
#pragma once
#include <stdint.h>
extern const uint8_t *rom_image;
int rom_load(const char *path);
static inline const uint8_t *rom_ptr(uint32_t addr) { return rom_image + addr; }
uint32_t rom_find(uint32_t from, uint32_t to, const void *bytes, unsigned n);
