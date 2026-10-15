#include <stdio.h>
#include <stdlib.h>
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
