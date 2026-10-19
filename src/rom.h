/* ROM data access. The game's graphics, sound, text and tables are read from the user's ROM at
 * run time, by their 68k address (the asm rebuild's symbols); nothing is extracted into the
 * repository. */
#pragma once
#include <stdint.h>
#define ROM_SIZE 0x100000u                                /* the retail cartridge: 1 MB */
extern const uint8_t *rom_cart;          /* the cartridge */
/* a league's own art (native/league.c), in the cartridge's formats, at the numbers the game reads
 * it by: from the cartridge's end (ROM_SIZE) to rom_limit; NULL, and rom_limit ROM_SIZE, with
 * the cartridge's league or a league whose art is all the cartridge's */
extern const uint8_t *rom_art;
extern uint32_t rom_limit;
int rom_load(const char *path);
__attribute__((noreturn, cold)) void rom_bad_address(uint32_t addr);
/* the byte at 68k address addr: the cartridge's, or a league's art past it (below rom_limit:
 * anything else is a stray pointer) */
static inline const uint8_t *rom_ptr(uint32_t addr)
{
    if (addr < ROM_SIZE) return rom_cart + addr;
    if (addr >= rom_limit) rom_bad_address(addr);
    return rom_art + (addr - ROM_SIZE);
}
uint32_t rom_find(uint32_t from, uint32_t to, const void *bytes, unsigned n);
