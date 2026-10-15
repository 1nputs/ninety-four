/* video94.asm: display helpers. Only the leaf utilities the engine needs are here so far. */
#include "game.h"

/* video94.asm:randomd0. d0 = range -> d0 = random 0..range-1.
 * Steps the 32-bit seed with a 16x16 multiply spread over three products (the 68000 has no
 * 32x32 multiply): new = (lo*E62D) + ((lo*BB40 + hi*E62D) << 16) + 1, then scales
 * (new >> 8).w by range and keeps the high word of the product. */
uint16_t randomd0(uint16_t range)
{
    uint32_t lo = g.seed & 0xFFFF, hi = g.seed >> 16;
    uint32_t prod = lo * 0xE62Du;
    uint16_t hiw = (uint16_t)(prod >> 16) + (uint16_t)(lo * 0xBB40u) + (uint16_t)(hi * 0xE62Du);
    g.seed = (((uint32_t)hiw << 16) | (prod & 0xFFFF)) + 1;
    uint32_t scaled = (uint32_t)((int32_t)g.seed >> 8);        /* asr.l #8 */
    return (uint16_t)(((scaled & 0xFFFF) * range) >> 16);      /* mulu.w, swap */
}
