#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "rom.h"
const uint8_t *rom_image;
int rom_load(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    uint8_t *buf = malloc(0x100000);
    size_t n = fread(buf, 1, 0x100000, f);
    fclose(f);
    if (n != 0x100000) { free(buf); return 0; }
    rom_image = buf;
    return 1;
}

/* the ROM address of the first copy of bytes in [from, to), 0 if none: for data that sits
 * inline after a call and has no label (a remap table, a raw String) */
uint32_t rom_find(uint32_t from, uint32_t to, const void *bytes, unsigned n)
{
    for (uint32_t a = from; a + n <= to; a++)
        if (memcmp(rom_image + a, bytes, n) == 0) return a;
    return 0;
}
