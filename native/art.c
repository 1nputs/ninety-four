/* Team art as pictures (league.c): each team's logo, banners and centre ice, and the players'
 * photos, read out of a ROM (the cartridge, or a hack: romsrc.c) as palette indexes, written as
 * PNGs (pulseman's mdrt/png.h) with the colours they show in, and made again in the game's own
 * formats from PNGs a player may have changed:
 *
 *   logo      48 x 48: TeamLogoBitmaps (a picture: 6 x 6 map words, tiles), in the team's own
 *             16 colours (TeamLogoPalettes), which a logo's PNG brings along
 *   banners   88 x 32: on top the playoff tree's and the game's top bar's (Teamblocksmap), below
 *             the setup screen's (TeamBitmaps), in their screens' colours (line 1; the setup
 *             screen's 1 to 4 the team's own, from TeamPalettes)
 *   ice       the rink's centre, the 24 tiles the home team's TeamGfxList puts in place of the
 *             rink's last 24, where the rink's map has them (the rest of the picture shows the
 *             rink round them, and is not read back), in the colours of their line in a game
 *             (line 2: the home team's jersey colours)
 *   photo     48 x 48: a player card (FeaturedPictures), 3 bits a pixel shown as colours 5 to
 *             12 of the photos' palette (PicturePalette: shades of grey); 36 tiles in order
 *             on the photos' map, or fewer on a map of the card's own
 *
 * A picture is read back by its colours, or by its palette indexes when it is an indexed PNG
 * whose palette is the one written (an editor may keep it as it is, or change it). Nothing of a
 * ROM is kept in the repository: the pictures are the user's, made from the user's ROMs. */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "native.h"
#include "mdrt/png.h"
#include "mdrt/vdp.h"
#include "rom.h"
#include "symbols_gen.h"

static char err[400];
const char *art_error(void) { return err; }
static int fail(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(err, sizeof err, fmt, ap);
    va_end(ap);
    return -1;
}

/* ---- colours ------------------------------------------------------------------------------------ */

/* a Genesis colour word (0BGR, 3 bits a channel) as the screen shows it, and back (the nearest level) */
uint32_t art_rgb(uint16_t w) { return mdrt_vdp_rgb(w, MDRT_RGB_CONSOLE); }

uint16_t art_word(uint32_t rgb)
{
    static uint8_t level[8];
    if (!level[7])
        for (int l = 0; l < 8; l++) level[l] = (uint8_t)(mdrt_vdp_rgb((uint16_t)(l << 1), MDRT_RGB_CONSOLE) >> 16);
    uint16_t w = 0;
    for (int c = 0; c < 3; c++) {                         /* (red, green, blue: 0x00RRGGBB from the top) */
        int v = (int)(rgb >> (16 - 8 * c) & 0xFF), best = 0;
        for (int l = 1; l < 8; l++)
            if (abs(level[l] - v) < abs(level[best] - v)) best = l;
        w |= (uint16_t)(best << (1 + 4 * c));
    }
    return w;
}

/* ---- the game's tiles out of a ROM --------------------------------------------------------------- */

/* the tiles at a (DecompressGraphics: a count word, bit 15 packed by DecompressBytecode's code,
 * then the data) into out, room for max tiles: their count, -1 when they do not read */
int art_tiles(const struct romsrc *s, uint32_t a, uint8_t *out, int max)
{
    uint16_t n = rs16(s, a);
    int count = n & 0x7FFF;
    if (count > max) return -1;
    size_t want = (size_t)count * 32, got = 0;
    if (!(n & 0x8000)) {
        for (size_t i = 0; i < want; i++) out[i] = rs8(s, a + 2 + (uint32_t)i);
        return a + 2 + want <= s->size ? count : -1;
    }
    uint8_t ring[256], idx = 0;
    uint32_t src = a + 2;
#define PUT(v) do { ring[idx] = (v); if (++idx == 0) { for (int q = 0; q < 256 && got < want; q++) out[got++] = ring[q]; } } while (0)
    for (int guard = 0; guard < 1 << 22; guard++) {
        if (src + 2 >= s->size) return -1;
        uint8_t op = rs8(s, src++);
        int c, dist, step = 1;
        switch (op >> 4) {
        case 0: case 1: for (int k = (op & 0x1F) + 1; k; k--) PUT(rs8(s, src++)); continue;
        case 2: for (int k = (op & 0xF) + 1; k; k--) PUT(0); continue;
        case 3: { uint8_t v = rs8(s, src++); for (int k = (op & 0xF) + 3; k; k--) PUT(v); continue; }
        case 4: case 5: case 6: case 7: c = (op & 7) + 1; dist = ((op >> 3) & 7) + 1; break;
        case 8: c = (op & 0xF) + 2; dist = rs8(s, src++); break;
        case 9: c = (((op << 1) | (rs8(s, src) >> 7)) & 0x1F) + 2; dist = (rs8(s, src++) & 0x7F) + 1; break;
        case 10: c = (((op << 8 | rs8(s, src)) >> 6) & 0x3F) + 2; dist = (rs8(s, src++) & 0x3F) + 1; break;
        case 11: c = (((op << 8 | rs8(s, src)) >> 5) & 0x7F) + 2; dist = (rs8(s, src++) & 0x1F) + 1; break;
        case 12: case 13: c = (op & 3) + 1; dist = ((op >> 2) & 7) + 1; step = -1; break;
        case 14:
            c = (op & 0xF) + 2;
            dist = rs8(s, src++);
            step = -1;
            if (!dist) {                                  /* the end code */
                for (int q = 0; q < idx && got < want; q++) out[got++] = ring[q];
                return got == want ? count : -1;
            }
            break;
        default: c = (((op << 1) | (rs8(s, src) >> 7)) & 0x1F) + 2; dist = (rs8(s, src++) & 0x7F) + 1; step = -1; break;
        }
        uint8_t from = (uint8_t)(idx - dist);
        for (int k = c + 1; k; k--) { uint8_t v = ring[from]; from = (uint8_t)(from + step); PUT(v); }
    }
#undef PUT
    return -1;
}

/* pixel (x, y) of a cell drawn with map word w over tiles (count of them; first: the char of tile
 * 0): its colour 0-15, the word's palette line in *line */
static int cell_pixel(const uint8_t *tiles, int count, int first, uint16_t w, int x, int y)
{
    int ch = (w & 0x7FF) - first, sx = w & 0x800 ? 7 - x : x, sy = w & 0x1000 ? 7 - y : y;
    if (ch < 0 || ch >= count) return 0;
    uint8_t b = tiles[ch * 32 + sy * 4 + sx / 2];
    return sx & 1 ? b & 15 : b >> 4;
}

/* a picture's tiles from pixels, w x h cells (no two the same: the map in order, 0x2000 | n) */
static void tiles_of(const uint8_t *px, int pw, int cx, int cy, uint8_t out[32])
{
    for (int y = 0; y < 8; y++)
        for (int x = 0; x < 8; x += 2)
            out[y * 4 + x / 2] = (uint8_t)(px[(cy * 8 + y) * pw + cx * 8 + x] << 4 | px[(cy * 8 + y) * pw + cx * 8 + x + 1]);
}

/* ---- logos --------------------------------------------------------------------------------------- */

int art_logo(const struct romsrc *s, int t, uint8_t px[ART_LOGO * ART_LOGO], uint16_t pal[16])
{
    static uint8_t tiles[64 * 32];
    uint32_t bmp = rs32(s, s->table[LG_TeamLogoBitmaps] + 4u * (uint32_t)t), map = bmp + rs32(s, bmp + 4);
    int n = art_tiles(s, bmp + 8, tiles, 64);
    if (n < 0 || rs16(s, map) != 6 || rs16(s, map + 2) != 6) return -1;
    for (int y = 0; y < ART_LOGO; y++)
        for (int x = 0; x < ART_LOGO; x++)
            px[y * ART_LOGO + x] = (uint8_t)cell_pixel(tiles, n, 0, rs16(s, map + 4 + 2u * (uint32_t)(y / 8 * 6 + x / 8)), x % 8, y % 8);
    for (int k = 0; k < 16; k++) pal[k] = rs16(s, s->table[LG_TeamLogoPalettes] + 32u * (uint32_t)t + 2u * (uint32_t)k);
    return 0;
}

/* a logo in the cartridge's format: its palette's offset (none: the map's, as the cartridge's
 * logos have), the map's, the tiles (plain, those the same once), the map (6 x 6, line 1) */
size_t art_logo_make(const uint8_t px[ART_LOGO * ART_LOGO], uint8_t *out)
{
    uint8_t tiles[36][32];
    int n = 0, slot[36];
    for (int c = 0; c < 36; c++) {
        uint8_t t[32];
        tiles_of(px, ART_LOGO, c % 6, c / 6, t);
        int k = 0;
        while (k < n && memcmp(tiles[k], t, 32)) k++;
        if (k == n) memcpy(tiles[n++], t, 32);
        slot[c] = k;
    }
    uint32_t map = 8 + 2 + 32u * (uint32_t)n;
    uint8_t *p = out;
    for (int i = 0; i < 2; i++) { p[0] = (uint8_t)(map >> 24); p[1] = (uint8_t)(map >> 16); p[2] = (uint8_t)(map >> 8); p[3] = (uint8_t)map; p += 4; }
    *p++ = 0; *p++ = (uint8_t)n;
    memcpy(p, tiles, 32u * (uint32_t)n);
    p += 32 * n;
    *p++ = 0; *p++ = 6; *p++ = 0; *p++ = 6;
    for (int c = 0; c < 36; c++) { *p++ = 0x20; *p++ = (uint8_t)slot[c]; }
    return (size_t)(p - out);
}

/* ---- photos ------------------------------------------------------------------------------------- */

/* the photos' colours: PicturePalette's line 1 (colours 5 to 12 the shades a photo has) */
void art_photo_palette(const struct romsrc *s, uint16_t pal[16])
{
    uint32_t pp = s->photos + rs32(s, s->photos) + 32;
    for (int k = 0; k < 16; k++) pal[k] = rs16(s, pp + 2u * (uint32_t)k);
}

int art_photo(const struct romsrc *s, uint32_t pic, uint8_t px[ART_LOGO * ART_LOGO])
{
    uint32_t map = rs32(s, pic + 4) ? pic + rs32(s, pic + 4) : s->photos + rs32(s, s->photos + 4);
    int n = rs16(s, pic + 8);                             /* (a card with a map of its own may have fewer) */
    if (n < 1 || n > 36 || pic + 10 + (uint32_t)n * 24 > s->size || rs16(s, map) != 6 || rs16(s, map + 2) != 6) return -1;
    for (int y = 0; y < ART_LOGO; y++)
        for (int x = 0; x < ART_LOGO; x++) {
            int ch = rs16(s, map + 4 + 2u * (uint32_t)(y / 8 * 6 + x / 8)) & 0x7FF;
            if (ch >= n) return -1;
            uint32_t row = pic + 10 + (uint32_t)(ch * 8 + y % 8) * 3, b = (uint32_t)rs8(s, row) << 16 | (uint32_t)rs8(s, row + 1) << 8 | rs8(s, row + 2);
            px[y * ART_LOGO + x] = (uint8_t)(5 + (b >> (21 - 3 * (x % 8)) & 7));   /* (UnpackPicture: 3 bits a pixel, + 5) */
        }
    return 0;
}

/* a photo in the cards' format: no palette or map of its own (the photos' palette and map: the
 * 36 tiles in order), 36 tiles of 3-bit rows. Its pixels must be colours 5 to 12: 0, -1 */
int art_photo_make(const uint8_t px[ART_LOGO * ART_LOGO], uint8_t out[ART_PHOTO_BYTES])
{
    memset(out, 0, 8);
    out[8] = 0;
    out[9] = 36;
    for (int ch = 0; ch < 36; ch++)
        for (int y = 0; y < 8; y++) {
            uint32_t b = 0;
            for (int x = 0; x < 8; x++) {
                int v = px[(ch / 6 * 8 + y) * ART_LOGO + ch % 6 * 8 + x] - 5;
                if (v < 0 || v > 7) return -1;
                b |= (uint32_t)v << (21 - 3 * x);
            }
            uint8_t *row = out + 10 + (ch * 8 + y) * 3;
            row[0] = (uint8_t)(b >> 16); row[1] = (uint8_t)(b >> 8); row[2] = (uint8_t)b;
        }
    return 0;
}

/* ---- centre ice --------------------------------------------------------------------------------- */

/* the rink's map and tiles (Rinktilelist, Rinktiles), and where its last 24 tiles are on it */
struct rink { int w, h, count, x0, y0, cw, ch, line; uint32_t map, pal; uint8_t tiles[600 * 32]; };

static int rink_of(const struct romsrc *s, struct rink *R)
{
    R->map = s->rink + rs32(s, s->rink + 4);
    R->pal = s->rink + rs32(s, s->rink);
    R->w = rs16(s, R->map);
    R->h = rs16(s, R->map + 2);
    R->count = art_tiles(s, s->rink_tiles, R->tiles, 600);
    if (R->count < 24 || R->w < 1 || R->w > 64 || R->h < 1 || R->h > 256) return -1;
    int x1 = -1, y1 = -1;
    R->x0 = R->y0 = 1 << 20;
    R->line = 0;
    for (int y = 0; y < R->h; y++)
        for (int x = 0; x < R->w; x++) {
            uint16_t w = rs16(s, R->map + 4 + 2u * (uint32_t)(y * R->w + x));
            if ((w & 0x7FF) < R->count - 24 || (w & 0x7FF) >= R->count) continue;
            if (x < R->x0) R->x0 = x;
            if (y < R->y0) R->y0 = y;
            if (x > x1) x1 = x;
            if (y > y1) y1 = y;
            R->line = w >> 13 & 3;
        }
    if (x1 < 0 || x1 - R->x0 > 15 || y1 - R->y0 > 15) return -1;
    R->cw = x1 - R->x0 + 1;
    R->ch = y1 - R->y0 + 1;
    return 0;
}

static struct rink rink;

/* team t's centre ice: the rink round it, its 24 tiles where the rink has its last 24 */
int art_ice(const struct romsrc *s, int t, uint8_t *px, int *w, int *h, uint16_t pal[16])
{
    static uint8_t ice[24 * 32];
    if (rink_of(s, &rink) < 0) return -1;
    uint32_t gfx = rs32(s, s->table[LG_TeamGfxList] + 4u * (uint32_t)t);
    if (art_tiles(s, gfx + 8, ice, 24) != 24) return -1;
    *w = rink.cw * 8;
    *h = rink.ch * 8;
    for (int y = 0; y < *h; y++)
        for (int x = 0; x < *w; x++) {
            uint16_t word = rs16(s, rink.map + 4 + 2u * (uint32_t)((rink.y0 + y / 8) * rink.w + rink.x0 + x / 8));
            int ch = word & 0x7FF;
            px[y * *w + x] = (uint8_t)(ch >= rink.count - 24 ? cell_pixel(ice, 24, rink.count - 24, word, x % 8, y % 8)
                                                            : cell_pixel(rink.tiles, rink.count, 0, word, x % 8, y % 8));
        }
    uint32_t b = rs32(s, s->table[LG_TeamList] + 4u * (uint32_t)t);   /* (its line in a game: the home team's colours) */
    for (int k = 0; k < 16; k++) pal[k] = rink.line == 2 ? rs16(s, b + rs16(s, b + 2) + 2u * (uint32_t)k) : rs16(s, rink.pal + 32u * (uint32_t)rink.line + 2u * (uint32_t)k);
    return 0;
}

/* the 24 tiles back out of a centre ice picture (w x h, as art_ice made it, on the same rink):
 * each where the rink has it (flipped back), the same wherever the rink has it twice. Into out
 * as TeamGfxList has them (no palette or map, 24 plain tiles): 0, -1 */
int art_ice_make(const struct romsrc *s, const uint8_t *px, int w, int h, uint8_t out[ART_ICE_BYTES])
{
    if (rink_of(s, &rink) < 0) return fail("the rink cannot be read");
    if (w != rink.cw * 8 || h != rink.ch * 8) return fail("a centre ice picture is %d x %d pixels", rink.cw * 8, rink.ch * 8);
    uint8_t got[24] = { 0 }, *tiles = out + 10;
    memset(out, 0, ART_ICE_BYTES);
    out[9] = 24;
    for (int cy = 0; cy < rink.ch; cy++)
        for (int cx = 0; cx < rink.cw; cx++) {
            uint16_t word = rs16(s, rink.map + 4 + 2u * (uint32_t)((rink.y0 + cy) * rink.w + rink.x0 + cx));
            int k = (word & 0x7FF) - (rink.count - 24);
            if (k < 0 || k >= 24) continue;
            uint8_t t[32] = { 0 };
            for (int y = 0; y < 8; y++)
                for (int x = 0; x < 8; x++) {
                    int sx = word & 0x800 ? 7 - x : x, sy = word & 0x1000 ? 7 - y : y;
                    t[sy * 4 + sx / 2] |= (uint8_t)(px[(cy * 8 + y) * w + cx * 8 + x] << (sx & 1 ? 0 : 4));
                }
            if (got[k] && memcmp(tiles + 32 * k, t, 32)) return fail("the centre ice picture differs at %d, %d from where the rink shows the same tile", cx * 8, cy * 8);
            memcpy(tiles + 32 * k, t, 32);
            got[k] = 1;
        }
    return 0;
}

/* ---- banners ------------------------------------------------------------------------------------ */

/* team t's two banners (s's Teamblocksmap and TeamBitmaps) as one picture, 88 x 32: the first's
 * colours 0-15, the second's 16-31; their lines' colours in pal */
int art_banners(const struct romsrc *s, int t, uint8_t px[ART_BANNER_H * ART_BANNER_W], uint16_t pal[32])
{
    static uint8_t tiles[2048 * 32];               /* (a league's: the cartridge's 501, 22 more a team) */
    for (int set = 0; set < 2; set++) {
        uint32_t base = set ? s->bitmaps : s->table[LG_Teamblocksmap], map = base + rs32(s, base + 4), pp = base + rs32(s, base);
        int n = art_tiles(s, base + 8, tiles, 2048);
        if (n < 0 || rs16(s, map) != 11 || map + 4 + 2u * 22 * (uint32_t)(t + 1) > s->size) return -1;   /* (the game reads a team's rows whatever height the map says) */
        for (int y = 0; y < 16; y++)
            for (int x = 0; x < ART_BANNER_W; x++)
                px[(set * 16 + y) * ART_BANNER_W + x] = (uint8_t)(16 * set + cell_pixel(tiles, n, 0, rs16(s, map + 4 + 2u * (uint32_t)((2 * t + y / 8) * 11 + x / 8)), x % 8, y % 8));
        for (int k = 0; k < 16; k++) pal[16 * set + k] = rs16(s, pp + 32 + 2u * (uint32_t)k);   /* (line 1) */
    }
    uint32_t tp = s->table[LG_TeamPalettes] + 64u * (uint32_t)t;   /* (the setup screen's colours 1 to 4: the team's, DrawTeamBitmap's) */
    pal[17] = rs16(s, tp + 2);
    pal[18] = rs16(s, tp + 4);
    pal[19] = rs16(s, tp + 0x26);                         /* (as the visitors: 1 and 2 shown as 3 and 4) */
    pal[20] = rs16(s, tp + 0x28);
    pal[21] = 0xEEE;                                      /* (DrawMatchupBitmaps' white) */
    return 0;
}

/* ---- PNGs --------------------------------------------------------------------------------------- */

/* px (w x h indexes of pal's n colours) as a PNG at path: 0, -1 */
int art_save(const char *path, const uint8_t *px, int w, int h, const uint16_t *pal, int n, int transparent0)
{
    uint32_t rgb[256];
    uint8_t *png;
    for (int k = 0; k < n; k++) rgb[k] = art_rgb(pal[k]);
    size_t size = mdrt_png_write_indexed(px, w, h, w, rgb, n, transparent0, &png);
    FILE *f = size ? fopen(path, "wb") : NULL;
    int ok = f && fwrite(png, 1, size, f) == size;
    if (f && fclose(f)) ok = 0;
    free(png);
    return ok ? 0 : fail("cannot write %s", path);
}

/* the PNG at path, w x h (0: any; *w and *h say), into p: 1, 0 when there is none, -1 when it will
 * not do (art_error) */
int art_load(const char *path, mdrt_png *p, int w, int h)
{
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    uint8_t *b = NULL;
    size_t n = 0, cap = 0;
    for (;;) {
        if (n == cap) {
            cap = cap ? cap * 2 : 1 << 16;
            if (cap > 64u << 20) { free(b); fclose(f); return fail("%s: too big for a picture", path); }
            uint8_t *q = realloc(b, cap);
            if (!q) { free(b); fclose(f); return fail("%s: out of memory", path); }
            b = q;
        }
        size_t got = fread(b + n, 1, cap - n, f);
        n += got;
        if (!got) break;
    }
    fclose(f);
    char why[120];
    int r = mdrt_png_read(b, n, p, why, sizeof why);
    free(b);
    if (r < 0) return fail("%s: %s", path, why);
    if ((w && p->width != w) || (h && p->height != h)) {
        mdrt_png_free(p);
        return fail("%s: %d x %d pixels; it must be %d x %d", path, p->width, p->height, w, h);
    }
    return 1;
}

/* the colours of p's pixels in the box (x0, y0, w, h) as indexes of pal (n colours) into out (w
 * a row), each the index + base: an indexed PNG's own indexes when they are base to base + n - 1
 * (its colours aside: these pictures show in the colours of the screens they are on); else each
 * colour's place in pal (transparent: 0; the first place with the colour, colour 0 last). 0, -1
 * (art_error, path in it) */
int art_indexes(const char *path, const mdrt_png *p, int x0, int y0, int w, int h, const uint16_t *pal, int n, int base, uint8_t *out)
{
    int by_index = p->index != NULL;
    for (int y = 0; by_index && y < h; y++)
        for (int x = 0; by_index && x < w; x++) {
            int i = p->index[(y0 + y) * p->width + x0 + x] - base;
            by_index = i >= 0 && i < n;
        }
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            size_t at = (size_t)(y0 + y) * (size_t)p->width + (size_t)(x0 + x);
            if (by_index) { out[y * w + x] = p->index[at]; continue; }
            uint32_t c = p->rgba[at];
            int k = 0;
            if (c >> 24 >= 0x80) {
                uint16_t want = art_word(c & 0xFFFFFF);
                for (k = 1; k < n && pal[k] != want; k++) {}
                if (k == n && pal[0] != want)
                    return fail("%s: the colour #%06X at %d, %d is none of its palette's (the PNG as written has them)", path, (unsigned)(c & 0xFFFFFF), x0 + x, y0 + y);
                if (k == n) k = 0;
            }
            out[y * w + x] = (uint8_t)(k + base);
        }
    return 0;
}

/* a logo's own palette and indexes from p (48 x 48): an indexed PNG's first 16 colours and its
 * indexes, when it uses no more; else its colours in the order they come (at most 15, and
 * transparency as colour 0). 0, -1 */
int art_own_palette(const char *path, const mdrt_png *p, uint16_t pal[16], uint8_t *out)
{
    int n = p->width * p->height, top = -1;
    if (p->index) {
        for (int i = 0; i < n; i++) if (p->index[i] > top) top = p->index[i];
        if (top < 16) {
            for (int k = 0; k < 16; k++) pal[k] = k < p->colours ? art_word(p->palette[k] & 0xFFFFFF) : 0;
            memcpy(out, p->index, (size_t)n);
            return 0;
        }
    }
    int used = 1;
    pal[0] = 0;
    for (int k = 1; k < 16; k++) pal[k] = 0;
    for (int i = 0; i < n; i++) {
        uint32_t c = p->rgba[i];
        if (c >> 24 < 0x80) { out[i] = 0; continue; }
        uint16_t want = art_word(c & 0xFFFFFF);
        int k = 1;
        while (k < used && pal[k] != want) k++;
        if (k == used) {
            if (used == 16) return fail("%s: more than 15 colours (a logo has 15 and the transparent one)", path);
            pal[used++] = want;
        }
        out[i] = (uint8_t)k;
    }
    return 0;
}
