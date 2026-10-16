/* ROM data access. The game's graphics, sound, text and tables are read from the user's ROM at
 * run time, by their 68k address (the asm rebuild's symbols); nothing is extracted into the
 * repository. */
#pragma once
#include <stdint.h>
#define ROM_SIZE 0x100000u                                /* the retail cartridge: 1 MB */
extern const uint8_t *rom_image;
int rom_load(const char *path);
__attribute__((noreturn, cold)) void rom_bad_address(uint32_t addr);
/* the ROM byte at 68k address addr (below ROM_SIZE: anything else is a stray pointer) */
static inline const uint8_t *rom_ptr(uint32_t addr)
{
    if (addr >= ROM_SIZE) rom_bad_address(addr);
    return rom_image + addr;
}
uint32_t rom_find(uint32_t from, uint32_t to, const void *bytes, unsigned n);
