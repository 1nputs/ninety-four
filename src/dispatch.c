/* jsr (a0) to a routine address the game keeps in RAM or in a table (menu handlers, the menu
 * draw routine): the C function of that 68k routine. Grows as the targets are ported. */
#include <stdio.h>
#include <stdlib.h>
#include "game.h"
#include "symbols_gen.h"

static const struct { uint32_t addr; void (*fn)(void); } routines[] = {
    { SYM_ReloadRinkGraphics, ReloadRinkGraphics },
};

/* a target that is not ported yet: the tests replay its recorded call instead */
__attribute__((weak)) void call68k_missing(uint32_t addr)
{
    fprintf(stderr, "call68k: routine %06x is not ported\n", addr);
    abort();
}

void call68k(uint32_t addr)
{
    for (size_t i = 0; i < sizeof routines / sizeof *routines; i++)
        if (routines[i].addr == addr) { routines[i].fn(); return; }
    call68k_missing(addr);
}
