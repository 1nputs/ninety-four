/* The ratings the League page shows (league.c): a player's overall rating as his card shows it,
 * reckoned from his ratings and the cartridge's weights without the game's RAM (the menu asks on
 * the window's thread while a game plays, and CalcAttrib keeps its sums in RAM). make rulestest
 * checks it against the game's own (CalcAttrib, AttribAdjust) for every player of the cartridge. */
#include "native.h"
#include "symbols_gen.h"

static uint16_t c16(const uint8_t *c, uint32_t a) { return (uint16_t)(c[a] << 8 | c[a + 1]); }

/* cards94.asm:CalcAttrib with an overall mask, his hot / cold bytes 0: each nibble the mask has
 * (bit 15 the high nibble of his number's byte, down to bit 0 the low one of his 7th rating byte)
 * times its weight / 2, but weight and fighting as they are (the sum to *sum, when not NULL: it
 * tells apart players the card shows as 99); then x 100 over 100 (a sum of 100 or more: over
 * itself, less one) and AttribAdjust (title94.asm: under $32, half of it and $19) */
int rating_overall(const uint8_t *cart, const uint8_t nib[14], int number, int goalie, int *sum)
{
    uint16_t bits = c16(cart, goalie ? SYM_GAttribOverallMask : SYM_PAttribOverallMask);
    const uint8_t *w = cart + (goalie ? SYM_OvrGoalWgtList : SYM_OvrPlayerWgtList);
    uint16_t d0 = 0, d1 = 100;
    for (int d2 = 15; d2 >= 0; d2--) {
        if (!(bits & 1u << d2)) continue;
        uint16_t v = (uint16_t)(d2 < 14 ? nib[13 - d2] : d2 == 14 ? number % 10 : number / 10 % 10);
        if (d2 != 0xD && d2 != 6 && (int8_t)w[d2] != 2) v = (uint16_t)((int16_t)(uint16_t)(v * (uint16_t)(int8_t)w[d2]) >> 1);
        d0 = (uint16_t)(d0 + v);
    }
    if (sum) *sum = (int16_t)d0;
    if ((int16_t)d0 >= (int16_t)d1) { d1 = d0; d0--; }
    uint16_t r = (uint16_t)((uint32_t)d0 * 100u / d1);
    return (int16_t)r >= 0x32 ? r : (uint16_t)(((int16_t)r >> 1) + 0x19);
}
