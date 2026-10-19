#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "rom.h"
const uint8_t *rom_cart, *rom_art;
uint32_t rom_limit = ROM_SIZE;
int rom_load(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    uint8_t *buf = malloc(ROM_SIZE);
    size_t n = fread(buf, 1, ROM_SIZE, f);
    fclose(f);
    if (n != ROM_SIZE) { free(buf); return 0; }
    rom_cart = buf;
    rom_art = NULL;
    rom_limit = ROM_SIZE;
    return 1;
}

/* a ROM read past the cartridge (between its end and work RAM at $E00000): a bug in the port */
void rom_bad_address(uint32_t addr)
{
    fprintf(stderr, "rom_ptr: $%06X is past the ROM\n", addr);
    abort();
}

/* the ROM address of the first copy of bytes in [from, to), 0 if none: for data that sits
 * inline after a call and has no label (a remap table, a raw String) */
uint32_t rom_find(uint32_t from, uint32_t to, const void *bytes, unsigned n)
{
    for (uint32_t a = from; a + n <= to; a++)
        if (memcmp(rom_ptr(a), bytes, n) == 0) return a;
    return 0;
}
