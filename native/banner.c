/* Team name banners for leagues (league.c): the cartridge's two sets, 88 x 16 pixels a team
 * (Teamblocksmap: the playoff tree's and the game's top bar; TeamBitmaps: the setup screen's),
 * and new ones for a team whose name the cartridge has no banner for, drawn in the cartridge's
 * own letters: its art team's banner, the name cleared off its two colour bands, the new name in
 * their place. Both sets draw a name the same way: a letter's pixels in one colour, and a shadow
 * one pixel down and right in another (1 and 4 in Teamblocksmap, 5 and 6 in TeamBitmaps). The
 * letters are read at run time off the user's cartridge, from its 28 banners, whose names are
 * known; the few the cartridge never writes (X, Z, the digits, the hyphen) are drawn here in
 * its style. Nothing of the cartridge is kept in the repository. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "native.h"
#include "rom.h"
#include "symbols_gen.h"

#define BW 88
#define BH 16
#define CART_TEAMS 28
#define TOP 2                   /* the name's rows: 2 to 13, columns 2 to 85 (inside the frame) */
#define BOTTOM 13
#define LEFT 2
#define RIGHT 85
#define GAP 1                   /* between letters (the cartridge's: 1, now and then 2 or 0) */
#define SPACE 4                 /* more between words (5 in all) */

static const uint32_t set_addr[BANNER_SETS] = { SYM_Teamblocksmap, SYM_TeamBitmaps };
static const uint8_t ink_of[BANNER_SETS] = { 1, 5 }, shadow_of[BANNER_SETS] = { 4, 6 };
/* the names on the cartridge's banners, in team order */
static const char *const cart_text[CART_TEAMS] = {
    "ANAHEIM", "BOSTON", "BUFFALO", "CALGARY", "CHICAGO", "DALLAS", "DETROIT", "EDMONTON", "FLORIDA", "HARTFORD",
    "LOS ANGELES", "MONTREAL", "NEW JERSEY", "NEW YORK", "NEW YORK", "OTTAWA", "PHILADELPHIA", "PITTSBURGH", "QUEBEC",
    "SAN JOSE", "ST. LOUIS", "TAMPA BAY", "TORONTO", "VANCOUVER", "WASHINGTON", "WINNIPEG", "ALL STARS E.", "ALL STARS W.",
};
/* the letters the cartridge's banners never show, in their style (rows 3 to 11) */
static const struct { char c; const char *rows[9]; } drawn[] = {
    { 'X', { "##...##", "###.###", ".#####.", "..###..", "..###..", ".#####.", "###.###", "##...##", "##...##" } },
    { 'Z', { "######", "######", "...###", "..###.", ".###..", "###...", "##....", "######", "######" } },
    { '0', { ".####.", "######", "##..##", "##..##", "##..##", "##..##", "##..##", "######", ".####." } },
    { '1', { ".##", "###", "###", ".##", ".##", ".##", ".##", ".##", ".##" } },
    { '2', { ".####.", "######", "##..##", "...###", "..###.", ".###..", "###...", "######", "######" } },
    { '3', { "#####.", "######", "....##", ".####.", ".####.", "....##", "....##", "######", "#####." } },
    { '4', { "##..##", "##..##", "##..##", "##..##", "######", "######", "....##", "....##", "....##" } },
    { '5', { "######", "######", "##....", "#####.", "######", "....##", "....##", "######", "#####." } },
    { '6', { ".####.", "#####.", "##....", "#####.", "######", "##..##", "##..##", "######", ".####." } },
    { '7', { "######", "######", "...###", "..###.", "..##..", ".###..", ".##...", ".##...", ".##..." } },
    { '8', { ".####.", "######", "##..##", ".####.", ".####.", "##..##", "##..##", "######", ".####." } },
    { '9', { ".####.", "######", "##..##", "##..##", "######", ".#####", "....##", ".#####", ".####." } },
    { '-', { "....", "....", "....", "####", "####", "....", "....", "....", "...." } },
    { '\'', { "##", "##", "##", "..", "..", "..", "..", "..", ".." } },
};

struct set { uint8_t *tiles; int count; uint32_t map; uint16_t mw; };
static struct set sets[BANNER_SETS];
static const uint8_t *made_from;        /* the cartridge the sets and letters are from */
struct glyph { uint8_t w; uint16_t rows[BOTTOM - TOP + 1]; };   /* bit x: column x of the letter */
static struct glyph glyphs[128];

static uint16_t c16(const uint8_t *c, uint32_t a) { return (uint16_t)(c[a] << 8 | c[a + 1]); }
static uint32_t c32(const uint8_t *c, uint32_t a) { return (uint32_t)c16(c, a) << 16 | c16(c, a + 2); }

/* the cartridge's packed tiles at src (DecompressBytecode's code, into memory): the bytes out, -1
 * when they run past cap or the ROM */
static long unpack(const uint8_t *c, uint32_t src, uint8_t *out, size_t cap)
{
    uint8_t ring[256], idx = 0;
    size_t n = 0;
#define PUT(v) do { ring[idx] = (v); if (++idx == 0) { if (n + 256 > cap) return -1; memcpy(out + n, ring, 256); n += 256; } } while (0)
    for (;;) {
        if (src + 2 >= ROM_SIZE) return -1;
        uint8_t op = c[src++];
        int count, dist, step = 1;
        switch (op >> 4) {
        case 0: case 1: for (int k = (op & 0x1F) + 1; k; k--) PUT(c[src++]); continue;
        case 2: for (int k = (op & 0xF) + 1; k; k--) PUT(0); continue;
        case 3: { uint8_t v = c[src++]; for (int k = (op & 0xF) + 3; k; k--) PUT(v); continue; }
        case 4: case 5: case 6: case 7: count = (op & 7) + 1; dist = ((op >> 3) & 7) + 1; break;
        case 8: count = (op & 0xF) + 2; dist = c[src++]; break;
        case 9: count = (((op << 1) | (c[src] >> 7)) & 0x1F) + 2; dist = (c[src++] & 0x7F) + 1; break;
        case 10: count = (((op << 8 | c[src]) >> 6) & 0x3F) + 2; dist = (c[src++] & 0x3F) + 1; break;
        case 11: count = (((op << 8 | c[src]) >> 5) & 0x7F) + 2; dist = (c[src++] & 0x1F) + 1; break;
        case 12: case 13: count = (op & 3) + 1; dist = ((op >> 2) & 7) + 1; step = -1; break;
        case 14:
            count = (op & 0xF) + 2;
            dist = c[src++];
            step = -1;
            if (!dist) {                                  /* the end code */
                if (n + idx > cap) return -1;
                memcpy(out + n, ring, idx);
                return (long)(n + idx);
            }
            break;
        default: count = (((op << 1) | (c[src] >> 7)) & 0x1F) + 2; dist = (c[src++] & 0x7F) + 1; step = -1; break;
        }
        uint8_t from = (uint8_t)(idx - dist);
        for (int k = count + 1; k; k--) { uint8_t v = ring[from]; from = (uint8_t)(from + step); PUT(v); }
    }
#undef PUT
}

/* team t's banner in set s, as the cartridge has it */
static void cart_banner(int s, int t, uint8_t px[BH][BW])
{
    const struct set *S = &sets[s];
    for (int y = 0; y < BH; y++)
        for (int x = 0; x < BW; x++) {
            uint16_t w = c16(made_from, S->map + 4 + 2u * (uint32_t)((t * 2 + y / 8) * S->mw + x / 8));
            int ch = w & 0x7FF, sx = w & 0x800 ? 7 - x % 8 : x % 8, sy = w & 0x1000 ? 7 - y % 8 : y % 8;
            uint8_t b = ch < S->count ? S->tiles[ch * 32 + sy * 4 + sx / 2] : 0;
            px[y][x] = sx & 1 ? b & 15 : b >> 4;
        }
}

int banner_prepare(const uint8_t *cart)
{
    if (made_from == cart) return 0;
    for (int s = 0; s < BANNER_SETS; s++) {
        uint32_t base = set_addr[s];
        uint16_t n = c16(cart, base + 8);
        struct set *S = &sets[s];
        free(S->tiles);
        S->count = n & 0x7FFF;
        S->tiles = malloc((size_t)S->count * 32 + 256);
        if (!S->tiles) return -1;
        if (n & 0x8000) {
            if (unpack(cart, base + 10, S->tiles, (size_t)S->count * 32 + 256) < S->count * 32) return -1;
        } else {
            memcpy(S->tiles, cart + base + 10, (size_t)S->count * 32);
        }
        S->map = base + c32(cart, base + 4);
        S->mw = c16(cart, S->map);
        if (S->mw != 11 || c16(cart, S->map + 2) < 2 * CART_TEAMS) return -1;
    }
    made_from = cart;
    /* the letters, off the Teamblocksmap banners: a letter is a run of columns with its colour in
     * them (a banner whose runs are not its letters, two of them touching, gives none) */
    memset(glyphs, 0, sizeof glyphs);
    for (int t = 0; t < CART_TEAMS; t++) {
        uint8_t px[BH][BW];
        cart_banner(BANNER_BLOCKS, t, px);
        int runs[2][16], nr = 0;
        for (int x = LEFT, in = 0; x <= RIGHT + 1; x++) {
            int any = 0;
            for (int y = TOP; y <= BOTTOM && x <= RIGHT; y++) any |= px[y][x] == ink_of[BANNER_BLOCKS];
            if (any && !in && nr < 16) runs[0][nr] = x;
            if (!any && in) runs[1][nr++] = x;
            in = any;
        }
        const char *s = cart_text[t];
        int letters = 0;
        for (const char *p = s; *p; p++) letters += *p != ' ';
        if (letters != nr) continue;
        for (int k = 0; *s; s++) {
            if (*s == ' ') continue;
            struct glyph *g = &glyphs[(unsigned char)*s];
            int x0 = runs[0][k], w = runs[1][k] - x0;
            k++;
            if (g->w || w > 16) continue;
            g->w = (uint8_t)w;
            for (int y = TOP; y <= BOTTOM; y++)
                for (int x = 0; x < w; x++)
                    if (px[y][x0 + x] == ink_of[BANNER_BLOCKS]) g->rows[y - TOP] |= (uint16_t)(1 << x);
        }
    }
    for (size_t i = 0; i < sizeof drawn / sizeof drawn[0]; i++) {
        struct glyph *g = &glyphs[(unsigned char)drawn[i].c];
        if (g->w) continue;                               /* (the cartridge's own, when it has one) */
        g->w = (uint8_t)strlen(drawn[i].rows[0]);
        for (int y = 0; y < 9; y++)
            for (int x = 0; x < g->w; x++)
                if (drawn[i].rows[y][x] == '#') g->rows[y + 3 - TOP] |= (uint16_t)(1 << x);
    }
    return 0;
}

/* the name's width in pixels with gaps of gap between letters, or -1 with *bad its first letter
 * that has no glyph */
static int width(const char *text, int gap, char *bad)
{
    int w = 0, first = 1;
    for (const char *p = text; *p; p++) {
        if (*p == ' ') { w += SPACE; continue; }
        unsigned char c = (unsigned char)*p;
        if (c >= 128 || !glyphs[c].w) { *bad = *p; return -1; }
        w += glyphs[c].w + (first ? 0 : gap);
        first = 0;
    }
    return w;
}

int banner_check(const char *text, char *why, size_t n)
{
    char bad = 0;
    int w = width(text, GAP, &bad), room = RIGHT - LEFT - 1;   /* (the shadow takes the last column) */
    if (w < 0) { snprintf(why, n, "banner: the banners have no letter '%c' (A-Z, 0-9, . - ')", bad); return -1; }
    if (w > room && width(text, 0, &bad) > room) {
        snprintf(why, n, "banner: \"%s\" is %d pixels wide, a banner holds %d: give the team a shorter banner", text, width(text, 0, &bad), room);
        return -1;
    }
    return 0;
}

/* the name drawn on the bands of px (the rows and columns inside the frame), as both sets do */
static void draw_mask(int s, uint8_t px[BH][BW], const uint8_t mask[BH][BW])
{
    for (int y = TOP; y <= BOTTOM; y++) {
        const uint8_t band[2] = { px[y][LEFT], px[y][LEFT + 1] };   /* (a band can be dithered: its first two columns) */
        for (int x = LEFT; x <= RIGHT; x++)
            px[y][x] = mask[y][x] ? ink_of[s] : mask[y - 1][x - 1] ? shadow_of[s] : band[(x - LEFT) & 1];
    }
}

void banner_pixels(int s, int art, const char *text, uint8_t px[BH][BW])
{
    cart_banner(s, art, px);
    if (!text) return;
    static uint8_t mask[BH][BW];
    memset(mask, 0, sizeof mask);
    char bad;
    int gap = width(text, GAP, &bad) <= RIGHT - LEFT - 1 ? GAP : 0, w = width(text, gap, &bad);
    if (w < 0) return;
    int x = LEFT + 1 + (RIGHT - LEFT - 1 - w) / 2, first = 1;
    for (const char *p = text; *p; p++) {
        if (*p == ' ') { x += SPACE; continue; }
        const struct glyph *g = &glyphs[(unsigned char)*p];
        if (!first) x += gap;
        first = 0;
        for (int y = 0; y <= BOTTOM - TOP; y++)
            for (int k = 0; k < g->w; k++)
                if (g->rows[y] >> k & 1 && x + k <= RIGHT - 1) mask[TOP + y][x + k] = 1;
        x += g->w;
    }
    draw_mask(s, px, mask);
}

void banner_tiles(const uint8_t px[BH][BW], uint8_t out[22 * 32])
{
    for (int i = 0; i < 22; i++)
        for (int y = 0; y < 8; y++)
            for (int x = 0; x < 8; x += 2)
                out[i * 32 + y * 4 + x / 2] = (uint8_t)(px[i / 11 * 8 + y][i % 11 * 8 + x] << 4 | px[i / 11 * 8 + y][i % 11 * 8 + x + 1]);
}

const uint8_t *banner_set(int s, int *count, uint32_t *map)
{
    *count = sets[s].count;
    *map = sets[s].map;
    return sets[s].tiles;
}

int banner_selfcheck(int *letters, int *same)
{
    *letters = 0;
    for (int c = 0; c < 128; c++) *letters += glyphs[c].w != 0;
    /* each cartridge banner, its name cleared and drawn again from its own letters' pixels: as it was */
    *same = 0;
    for (int s = 0; s < BANNER_SETS; s++)
        for (int t = 0; t < CART_TEAMS; t++) {
            uint8_t px[BH][BW], again[BH][BW], mask[BH][BW] = { { 0 } };
            cart_banner(s, t, px);
            memcpy(again, px, sizeof px);
            for (int y = TOP; y <= BOTTOM; y++)
                for (int x = LEFT; x <= RIGHT; x++) mask[y][x] = px[y][x] == ink_of[s];
            draw_mask(s, again, mask);
            *same += !memcmp(again, px, sizeof px);
        }
    return BANNER_SETS * CART_TEAMS;
}
