/* Team art as pictures (league.c): each team's logo, banners and centre ice, and the players'
 * photos, read out of a ROM (the cartridge, or a hack: romsrc.c) as palette indexes, written as
 * PNGs (pulseman's mdrt/png.h) with the colours they show in, and made again in the game's own
 * formats from PNGs a player may have changed:
 *
 *   logo      48 x 48: TeamLogoBitmaps (a picture: 6 x 6 map words, tiles), in the team's own
 *             16 colours (TeamLogoPalettes), which a logo's PNG brings along
 *   banners   88 x 32: on top the playoff tree's and the game's top bar's (Teamblocksmap: colours
 *             0-63, a cell's line x 16 + its colour), below the setup screen's (TeamBitmaps:
 *             64 + its colour, line 1, whose 1 to 4 are the team's own, from TeamPalettes)
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

/* the same for a length: 0 */
static size_t fail_size(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(err, sizeof err, fmt, ap);
    va_end(ap);
    return 0;
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
 * then the data) into out (NULL: only measured), room for max tiles: their count, -1 when they do
 * not read; *end the address after their data */
static int tiles_at(const struct romsrc *s, uint32_t a, uint8_t *out, int max, uint32_t *end)
{
    uint16_t n = rs16(s, a);
    int count = n & 0x7FFF;
    if (count > max) return -1;
    size_t want = (size_t)count * 32, got = 0;
    if (!(n & 0x8000)) {
        for (size_t i = 0; out && i < want; i++) out[i] = rs8(s, a + 2 + (uint32_t)i);
        *end = a + 2 + (uint32_t)want;
        return *end <= s->size ? count : -1;
    }
    uint8_t ring[256] = { 0 }, idx = 0;
    uint32_t src = a + 2;
#define PUT(v) do { ring[idx] = (v); if (++idx == 0) { for (int q = 0; q < 256 && got < want; q++, got++) if (out) out[got] = ring[q]; } } while (0)
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
                for (int q = 0; q < idx && got < want; q++, got++) if (out) out[got] = ring[q];
                *end = src;
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

int art_tiles(const struct romsrc *s, uint32_t a, uint8_t *out, int max)
{
    uint32_t end;
    return tiles_at(s, a, out, max, &end);
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
    if (art_logo_at(s, rs32(s, s->table[LG_TeamLogoBitmaps] + 4u * (uint32_t)t), px) < 0) return -1;
    for (int k = 0; k < 16; k++) pal[k] = rs16(s, s->table[LG_TeamLogoPalettes] + 32u * (uint32_t)t + 2u * (uint32_t)k);
    return 0;
}

int art_logo_at(const struct romsrc *s, uint32_t bmp, uint8_t px[ART_LOGO * ART_LOGO])
{
    static uint8_t tiles[64 * 32];
    uint32_t map = bmp + rs32(s, bmp + 4);
    int n = art_tiles(s, bmp + 8, tiles, 64);
    if (n < 0 || rs16(s, map) != 6 || rs16(s, map + 2) != 6) return -1;
    for (int y = 0; y < ART_LOGO; y++)
        for (int x = 0; x < ART_LOGO; x++)
            px[y * ART_LOGO + x] = (uint8_t)cell_pixel(tiles, n, 0, rs16(s, map + 4 + 2u * (uint32_t)(y / 8 * 6 + x / 8)), x % 8, y % 8);
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
    static const uint16_t greys[8] = { 0xEEE, 0x000, 0xCCC, 0xAAA, 0x888, 0x666, 0x444, 0x222 };
    uint32_t pp = s->photos + rs32(s, s->photos) + 32;
    for (int k = 0; k < 16; k++) pal[k] = pp + 32 <= s->size ? rs16(s, pp + 2u * (uint32_t)k) : k >= 5 && k <= 12 ? greys[k - 5] : 0;   /* (out of reach: the cartridge's shades) */
}

int art_photo(const struct romsrc *s, uint32_t pic, uint8_t px[ART_LOGO * ART_LOGO])
{
    uint32_t map = rs32(s, pic + 4) ? pic + rs32(s, pic + 4) : s->photos + rs32(s, s->photos + 4);
    int n = rs16(s, pic + 8), own = rs32(s, pic + 4) != 0; /* (a card with a map of its own may have fewer) */
    if (!own && (rs16(s, map) != 6 || rs16(s, map + 2) != 6)) map = 0;   /* (the photos' map out of reach: theirs, the 36 in order) */
    if (n < 1 || n > 36 || pic + 10 + (uint32_t)n * 24 > s->size || (map && (rs16(s, map) != 6 || rs16(s, map + 2) != 6))) return -1;
    for (int y = 0; y < ART_LOGO; y++)
        for (int x = 0; x < ART_LOGO; x++) {
            int ch = map ? rs16(s, map + 4 + 2u * (uint32_t)(y / 8 * 6 + x / 8)) & 0x7FF : y / 8 * 6 + x / 8;
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
 * each pixel its line x 16 + its colour (0-63: its cells may be of any line), the second's 64 + its
 * colour (line 1); their colours in pal */
int art_banners(const struct romsrc *s, int t, uint8_t px[ART_BANNER_H * ART_BANNER_W], uint16_t pal[ART_BANNER_COLOURS])
{
    static uint8_t tiles[2048 * 32];               /* (a league's: the cartridge's 501, 22 more a team) */
    for (int set = 0; set < 2; set++) {
        uint32_t base = set ? s->bitmaps : s->table[LG_Teamblocksmap], map = base + rs32(s, base + 4), pp = base + rs32(s, base);
        int n = art_tiles(s, base + 8, tiles, 2048);
        if (n < 0 || rs16(s, map) != 11 || map + 4 + 2u * 22 * (uint32_t)(t + 1) > s->size) return -1;   /* (the game reads a team's rows whatever height the map says) */
        for (int y = 0; y < 16; y++)
            for (int x = 0; x < ART_BANNER_W; x++) {
                uint16_t w = rs16(s, map + 4 + 2u * (uint32_t)((2 * t + y / 8) * 11 + x / 8));
                px[(set * 16 + y) * ART_BANNER_W + x] = (uint8_t)((set ? ART_BANNER_SETUP : (w >> 13 & 3) * 16) + cell_pixel(tiles, n, 0, w, x % 8, y % 8));
            }
        if (set) for (int k = 0; k < 16; k++) pal[ART_BANNER_SETUP + k] = rs16(s, pp + 32 + 2u * (uint32_t)k);   /* (line 1) */
        else for (int k = 0; k < 64; k++) pal[k] = rs16(s, pp + 2u * (uint32_t)k);
    }
    uint32_t tp = s->table[LG_TeamPalettes] + 64u * (uint32_t)t;   /* (the setup screen's colours 1 to 4: the team's, DrawTeamBitmap's) */
    pal[ART_BANNER_SETUP + 1] = rs16(s, tp + 2);
    pal[ART_BANNER_SETUP + 2] = rs16(s, tp + 4);
    pal[ART_BANNER_SETUP + 3] = rs16(s, tp + 0x26);       /* (as the visitors: 1 and 2 shown as 3 and 4) */
    pal[ART_BANNER_SETUP + 4] = rs16(s, tp + 0x28);
    pal[ART_BANNER_SETUP + 5] = 0xEEE;                    /* (DrawMatchupBitmaps' white) */
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

/* the PNG in b (n bytes; name for what is said of it), w x h (0: any), into p: 0, -1 (art_error) */
int art_load_bytes(const char *name, const uint8_t *b, size_t n, mdrt_png *p, int w, int h)
{
    char why[120];
    if (mdrt_png_read(b, n, p, why, sizeof why) < 0) return fail("%s: %s", name, why);
    if ((w && p->width != w) || (h && p->height != h)) {
        mdrt_png_free(p);
        return fail("%s: %d x %d pixels; it must be %d x %d", name, p->width, p->height, w, h);
    }
    return 0;
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
    int r = art_load_bytes(path, b, n, p, w, h);
    free(b);
    return r < 0 ? -1 : 1;
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

/* ---- the art every team shares (league.h's ART_ assets) ------------------------------------------ */

/* how each is made: a picture (palette offset or none, map offset, tiles: the title screens,
 * setup screens, fonts, the rinks, whose header is followed by their tiles), sprites (palette,
 * frames, tiles: shown as a sheet of their tiles in order), a stock photo. tiles: where the
 * tiles another address names are (Rinktiles, Spritetiles: of the asset before them); sub: a
 * picture inside it at that offset (title.c draws PlayoffSprite's at +$12E0) */
enum { PICTURE, SPRITES, PHOTO };
static const struct shared { int art; const char *name; int kind; int tiles_art, tiles_at; uint32_t sub; } shared[] = {
    { ART_TitleScreenImg, "TitleScreenImg", PICTURE, -1, 0, 0 }, { ART_TitleImg, "TitleImg", PICTURE, -1, 0, 0 },
    { ART_NHLShieldImg, "NHLShieldImg", PICTURE, -1, 0, 0 }, { ART_PAlogoImg, "PAlogoImg", PICTURE, -1, 0, 0 },
    { ART_EASportsMap, "EASportsMap", PICTURE, -1, 0, 0 }, { ART_HiScoreImg, "HiScoreImg", PICTURE, -1, 0, 0 },
    { ART_GameSetUpMap, "GameSetUpMap", PICTURE, -1, 0, 0 }, { ART_GameSetUpMap2, "GameSetUpMap2", PICTURE, -1, 0, 0 },
    { ART_SetupMenuMap, "SetupMenuMap", PICTURE, -1, 0, 0 }, { ART_SetupFramerMap, "SetupFramerMap", PICTURE, -1, 0, 0 },
    { ART_LogoBoxMap, "LogoBoxMap", PICTURE, -1, 0, 0 }, { ART_ScoutMap, "ScoutMap", PICTURE, -1, 0, 0 },
    { ART_RonBarrMap, "RonBarrMap", PICTURE, -1, 0, 0 }, { ART_PlayerCardsScreenPic, "PlayerCardsScreenPic", PICTURE, -1, 0, 0 },
    { ART_PicturePalette, "PicturePalette", PICTURE, -1, 0, 0 }, { ART_ReplayMap, "ReplayMap", PICTURE, -1, 0, 0 },
    { ART_Rinktilelist, "Rinktilelist", PICTURE, ART_Rinktiles, 8, 0 },
    { ART_RevRinkTilelist, "RevRinkTilelist", PICTURE, ART_RevRinkTiles, 8, 0 },
    { ART_HorRinkMap, "HorRinkMap", PICTURE, ART_HorRinkTiles, 8, 0 },
    { ART_ArenaGfxBank, "ArenaGfxBank", PICTURE, -1, 0, 0 }, { ART_CornerLogoMap, "CornerLogoMap", PICTURE, -1, 0, 0 },
    { ART_ScoresMap, "ScoresMap", PICTURE, -1, 0, 0 }, { ART_EASNmap, "EASNmap", PICTURE, -1, 0, 0 },
    { ART_EnergyBarMap, "EnergyBarMap", PICTURE, -1, 0, 0 }, { ART_FaceOffMap, "FaceOffMap", PICTURE, -1, 0, 0 },
    { ART_FramerMap, "FramerMap", PICTURE, -1, 0, 0 }, { ART_Arrowsmap, "Arrowsmap", PICTURE, -1, 0, 0 },
    { ART_RefsMap, "RefsMap", PICTURE, -1, 0, 0 }, { ART_RefMap2, "RefMap2", PICTURE, -1, 0, 0 },
    { ART_HotIconMap, "HotIconMap", PICTURE, -1, 0, 0 }, { ART_ColdIconMap, "ColdIconMap", PICTURE, -1, 0, 0 },
    { ART_BigFontMap, "BigFontMap", PICTURE, -1, 0, 0 }, { ART_SmallFontMap, "SmallFontMap", PICTURE, -1, 0, 0 },
    { ART_PrintFont2Map, "PrintFont2Map", PICTURE, -1, 0, 0 },
    { ART_Sprites, "Sprites", SPRITES, ART_Spritetiles, 10, 0 }, { ART_CrowdFrameList, "CrowdFrameList", SPRITES, -1, 0, 0 },
    { ART_ZamFrameList, "ZamFrameList", SPRITES, -1, 0, 0 }, { ART_FaceOffSprites, "FaceOffSprites", SPRITES, -1, 0, 0 },
    { ART_PlayoffSprite, "PlayoffSprite", SPRITES, -1, 0, 0x12E0 },
    { ART_NoPicSkater1, "NoPicSkater1", PHOTO, -1, 0, 0 }, { ART_NoPicSkater2, "NoPicSkater2", PHOTO, -1, 0, 0 },
    { ART_NoPicGoalie1, "NoPicGoalie1", PHOTO, -1, 0, 0 }, { ART_NoPicGoalie2, "NoPicGoalie2", PHOTO, -1, 0, 0 },
};
#define SHARED ((int)(sizeof shared / sizeof shared[0]))
#define SHARED_MAX 0x60000u     /* (the biggest, the players' sprites: 264 KB and their frames) */
#define TILES_MAX 9000

int art_shared(void) { return SHARED; }
const char *art_shared_name(int i) { return shared[i].name; }
int art_shared_asset(int i) { return shared[i].art; }
int art_shared_tiles(int i, int *at) { *at = shared[i].tiles_at; return shared[i].tiles_art; }

static uint32_t max_u32(uint32_t a, uint32_t b) { return a > b ? a : b; }

/* a picture at a: the length to the end of its tiles, palette (4 lines) and map; 0 when it does
 * not read */
static uint32_t picture_size(const struct romsrc *s, uint32_t a)
{
    uint32_t pal = rs32(s, a), map = rs32(s, a + 4), end;
    if (map >= s->size || (pal != 0xFFFFFFFFu && pal >= s->size) || tiles_at(s, a + 8, NULL, TILES_MAX, &end) < 0) return 0;
    uint32_t n = end - a, w = rs16(s, a + map), h = rs16(s, a + map + 2);
    if (!w || !h || w > 128 || h > 256) return 0;
    n = max_u32(n, map + 4 + 2 * w * h);
    if (pal != 0xFFFFFFFFu) n = max_u32(n, pal + 128);
    return a + n <= s->size ? n : 0;
}

/* sprites at a: to the end of their tiles, palette and frames (a table of offsets from itself,
 * a frame's pieces from its offset to the next, 8 bytes each: the first offset none, the second
 * the table's length, the last the end of the last frame) */
static uint32_t sprites_size(const struct romsrc *s, uint32_t a)
{
    uint32_t pal = rs32(s, a), frames = rs32(s, a + 4), end;
    if (frames >= s->size || (pal != 0xFFFFFFFFu && pal >= s->size) || tiles_at(s, a + 8, NULL, TILES_MAX, &end) < 0) return 0;
    uint32_t n = end - a, table = a + frames, first = rs16(s, table + 2);
    if (first < 4 || first & 1) return 0;
    n = max_u32(n, frames + rs16(s, table + first - 2));
    if (pal != 0xFFFFFFFFu) n = max_u32(n, pal + 128);
    return a + n <= s->size ? n : 0;
}

uint32_t art_shared_size(const struct romsrc *s, int i, uint32_t a)
{
    uint32_t n;
    switch (shared[i].kind) {
    case PICTURE: return picture_size(s, a);
    case SPRITES:
        n = sprites_size(s, a);
        if (n && shared[i].sub) {
            uint32_t sub = picture_size(s, a + shared[i].sub);
            n = sub ? max_u32(n, shared[i].sub + sub) : 0;
        }
        return n;
    default: {
        int t = rs16(s, a + 8);
        n = 10 + 24u * (uint32_t)t;
        return t >= 1 && t <= 36 && a + n <= s->size ? n : 0;
    }
    }
}

void art_view_free(struct art_view *v) { free(v->px); v->px = NULL; }

static void put_be32(uint8_t *p, uint32_t v) { p[0] = (uint8_t)(v >> 24); p[1] = (uint8_t)(v >> 16); p[2] = (uint8_t)(v >> 8); p[3] = (uint8_t)v; }

/* where a's parts are, wherever they are (a hack may move a palette or frames far from the rest) */
static uint32_t span(const struct romsrc *s, uint32_t a, uint32_t off, uint32_t len) { return off == 0xFFFFFFFFu ? 0 : (a + off + len <= s->size ? len : 0); }

/* asset i at a in s, its parts gathered into one (malloc'd) as the game reads them: the header,
 * the tiles as they are, the palette (4 lines) when it has one, then the map or the frames, the
 * offsets made to fit. PlayoffSprite, with a picture inside where the game knows it, as it is.
 * Its length, 0 when it does not read */
size_t art_shared_copy(const struct romsrc *s, int i, uint32_t a, uint8_t **out)
{
    uint32_t n = art_shared_size(s, i, a), end;
    *out = NULL;
    if (shared[i].kind == PHOTO || shared[i].sub || (n && n <= SHARED_MAX)) {
        if (!n || !(*out = malloc(n))) return 0;
        for (uint32_t k = 0; k < n; k++) (*out)[k] = rs8(s, a + k);
        return n;
    }
    /* (spread out: gathered) */
    if (tiles_at(s, a + 8, NULL, TILES_MAX, &end) < 0) return 0;
    uint32_t pal = rs32(s, a), rest = rs32(s, a + 4), tiles_len = end - a - 8, pal_len = span(s, a, pal, 128), rest_len;
    if (shared[i].kind == PICTURE) rest_len = 4 + 2u * rs16(s, a + rest) * rs16(s, a + rest + 2);
    else rest_len = rs16(s, a + rest + rs16(s, a + rest + 2) - 2);
    if ((pal != 0xFFFFFFFFu && !pal_len) || !span(s, a, rest, rest_len) || rest_len < 4) return 0;
    n = 8 + tiles_len + pal_len + rest_len;
    uint8_t *b = malloc(n);
    if (!b) return 0;
    put_be32(b, pal_len ? 8 + tiles_len : 0xFFFFFFFFu);
    put_be32(b + 4, 8 + tiles_len + pal_len);
    for (uint32_t k = 0; k < tiles_len; k++) b[8 + k] = rs8(s, a + 8 + k);
    for (uint32_t k = 0; k < pal_len; k++) b[8 + tiles_len + k] = rs8(s, a + pal + k);
    for (uint32_t k = 0; k < rest_len; k++) b[8 + tiles_len + pal_len + k] = rs8(s, a + rest + k);
    *out = b;
    return n;
}

/* asset i at a in s as a picture: a picture's map drawn (each pixel its line x 16 + its colour;
 * its own 4 lines of colours, else screen's: those of the rink, which shows the pictures that
 * have none), sprites' tiles in order 16 a row (their own first line of colours, else team: a
 * team's home jersey), a photo (art_photo). 0, -1 */
int art_shared_view(const struct romsrc *s, int i, uint32_t a, const uint16_t screen[64], const uint16_t team[16], struct art_view *v)
{
    static uint8_t tiles[TILES_MAX * 32];
    memset(v, 0, sizeof *v);
    if (!art_shared_size(s, i, a)) return -1;
    if (shared[i].kind == PHOTO) {
        v->w = v->h = ART_LOGO;
        v->n = 16;
        if (!(v->px = malloc(ART_LOGO * ART_LOGO))) return -1;
        art_photo_palette(s, v->pal);
        if (art_photo(s, a, v->px) < 0) { art_view_free(v); return -1; }
        return 0;
    }
    uint32_t pal = rs32(s, a), end;
    int n = tiles_at(s, a + 8, tiles, TILES_MAX, &end);
    if (n < 0) return -1;
    if (shared[i].kind == SPRITES) {
        v->w = 128;
        v->h = (n + 15) / 16 * 8;
        v->n = 16;
        for (int k = 0; k < 16; k++) v->pal[k] = (pal != 0xFFFFFFFFu ? rs16(s, a + pal + 2u * (uint32_t)k) : team[k]) & 0x0EEE;
        if (!(v->px = calloc((size_t)v->w * (size_t)(v->h ? v->h : 1), 1))) return -1;
        for (int t = 0; t < n; t++)                       /* (tile t as it is: no map word to say more) */
            for (int y = 0; y < 8; y++)
                for (int x = 0; x < 8; x++) v->px[(t / 16 * 8 + y) * 128 + t % 16 * 8 + x] = (uint8_t)(tiles[t * 32 + y * 4 + x / 2] >> (x & 1 ? 0 : 4) & 15);
        return 0;
    }
    uint32_t map = a + rs32(s, a + 4);
    int mw = rs16(s, map), mh = rs16(s, map + 2);
    v->w = mw * 8;
    v->h = mh * 8;
    v->n = 64;
    v->own = pal != 0xFFFFFFFFu;                          /* (the colours' bits the VDP has: a palette may be other bytes, unused) */
    for (int k = 0; k < 64; k++) v->pal[k] = (v->own ? rs16(s, a + pal + 2u * (uint32_t)k) : screen[k]) & 0x0EEE;
    if (!(v->px = malloc((size_t)v->w * (size_t)v->h))) return -1;
    for (int y = 0; y < v->h; y++)
        for (int x = 0; x < v->w; x++) {
            uint16_t w = rs16(s, map + 4 + 2u * (uint32_t)((y / 8) * mw + x / 8));
            v->px[y * v->w + x] = (uint8_t)((w >> 13 & 3) * 16 + cell_pixel(tiles, n, 0, w, x % 8, y % 8));
        }
    return 0;
}

/* the PNG p as a view like base (its size, kind and lines): an indexed PNG's indexes (a
 * picture's 0-63, sprites' and a photo's the low 4 bits), else each colour among those of the
 * line base has there; a picture with colours of its own takes an indexed PNG's 64 colours.
 * 0, -1 (art_error) */
int art_shared_read(const char *path, const mdrt_png *p, int i, const struct art_view *base, struct art_view *v)
{
    *v = *base;
    v->px = NULL;
    if (p->width != base->w || p->height != base->h) return fail("%s: %d x %d pixels; it must be %d x %d", path, p->width, p->height, base->w, base->h);
    if (!(v->px = malloc((size_t)v->w * (size_t)v->h))) return fail("out of memory");
    int kind = shared[i].kind;
    if (kind == PICTURE && base->own && p->index && p->colours >= 64)
        for (int k = 0; k < 64; k++) v->pal[k] = art_word(p->palette[k] & 0xFFFFFF);
    for (int at = 0; at < v->w * v->h; at++) {
        int line = kind == PICTURE ? base->px[at] / 16 : 0;
        if (kind == PHOTO) {                              /* (a photo's 8 shades, by index or brightness) */
            if (p->index && p->index[at] >= 5 && p->index[at] <= 12) { v->px[at] = p->index[at]; continue; }
            uint32_t c = p->rgba[at];
            long lum = (long)(c >> 16 & 0xFF) * 299 + (long)(c >> 8 & 0xFF) * 587 + (long)(c & 0xFF) * 114, best = -1;
            for (int k = 5; k <= 12; k++) {
                uint32_t q = art_rgb(base->pal[k]);
                long d = labs((long)(q >> 16 & 0xFF) * 299 + (long)(q >> 8 & 0xFF) * 587 + (long)(q & 0xFF) * 114 - lum);
                if (best < 0 || d < best) { best = d; v->px[at] = (uint8_t)k; }
            }
            continue;
        }
        if (p->index) {
            int k = p->index[at];
            if (kind == PICTURE ? k >= 64 : 0) return fail("%s: the pixel at %d, %d is colour %d (a picture has 64)", path, at % v->w, at / v->w, k);
            v->px[at] = (uint8_t)(kind == PICTURE ? k : k & 15);
            continue;
        }
        uint32_t c = p->rgba[at];
        int k = 0;
        if (c >> 24 >= 0x80) {
            uint16_t want = art_word(c & 0xFFFFFF);
            for (k = 1; k < 16 && v->pal[line * 16 + k] != want; k++) {}
            if (k == 16 && v->pal[line * 16] != want)
                return fail("%s: the colour #%06X at %d, %d is none of its line's (the PNG as written has them)", path, (unsigned)(c & 0xFFFFFF), at % v->w, at / v->w);
            if (k == 16) k = 0;
        }
        v->px[at] = (uint8_t)(line * 16 + k);
    }
    return 0;
}

int art_view_same(const struct art_view *a, const struct art_view *b)
{
    return a->w == b->w && a->h == b->h && a->n == b->n && !memcmp(a->pal, b->pal, sizeof a->pal) && !memcmp(a->px, b->px, (size_t)a->w * (size_t)a->h);
}


/* asset i at a in s again, with the pixels (and a picture's colours) of v, a view of it: each
 * tile from where the map (a picture's) or the sheet (sprites') has it, the same wherever the
 * map shows it twice; the rest as it is (its map, frames, the order of its tiles). Tiles kept as
 * they were (plain) keep everything where it was; packed ones become plain, the rest after them.
 * Into *out (malloc'd): its length, 0 (art_error) */
size_t art_shared_make(const struct romsrc *s, int i, uint32_t a, const struct art_view *v, uint8_t **out)
{
    static uint8_t tiles[TILES_MAX * 32];
    static uint8_t got[TILES_MAX];
    uint32_t size = art_shared_size(s, i, a), end;
    *out = NULL;
    if (!size) return fail_size("%s cannot be read", shared[i].name);
    if (shared[i].kind == PHOTO) {
        uint8_t *b = malloc(ART_PHOTO_BYTES);
        if (!b) return fail_size("out of memory");
        if (art_photo_make(v->px, b) < 0) { free(b); return fail_size("%s: a photo has 8 shades, colours 5 to 12 of its palette", shared[i].name); }
        *out = b;
        return ART_PHOTO_BYTES;
    }
    int n = tiles_at(s, a + 8, tiles, TILES_MAX, &end);
    if (n < 0) return fail_size("%s cannot be read", shared[i].name);
    memset(got, 0, (size_t)n);
    uint32_t pal = rs32(s, a), map = a + rs32(s, a + 4);
    if (shared[i].kind == SPRITES) {
        for (int t = 0; t < n; t++) {
            memset(tiles + 32 * t, 0, 32);
            for (int y = 0; y < 8; y++)
                for (int x = 0; x < 8; x++) tiles[32 * t + y * 4 + x / 2] |= (uint8_t)((v->px[(t / 16 * 8 + y) * 128 + t % 16 * 8 + x] & 15) << (x & 1 ? 0 : 4));
        }
    } else {
        int mw = rs16(s, map), mh = rs16(s, map + 2);
        for (int cy = 0; cy < mh; cy++)
            for (int cx = 0; cx < mw; cx++) {
                uint16_t w = rs16(s, map + 4 + 2u * (uint32_t)(cy * mw + cx));
                int k = w & 0x7FF, line = w >> 13 & 3;
                if (k >= n) continue;                     /* (a tile of another's) */
                uint8_t t[32] = { 0 };
                for (int y = 0; y < 8; y++)
                    for (int x = 0; x < 8; x++) {
                        int c = v->px[(cy * 8 + y) * v->w + cx * 8 + x] - line * 16, sx = w & 0x800 ? 7 - x : x, sy = w & 0x1000 ? 7 - y : y;
                        if (c < 0 || c > 15) return fail_size("%s: the pixel at %d, %d is not one of its line's colours (%d to %d)", shared[i].name, cx * 8 + x, cy * 8 + y, line * 16, line * 16 + 15);
                        t[sy * 4 + sx / 2] |= (uint8_t)(c << (sx & 1 ? 0 : 4));
                    }
                if (got[k] && memcmp(tiles + 32 * k, t, 32))
                    return fail_size("%s: the picture differs at %d, %d from another place the same tile shows", shared[i].name, cx * 8, cy * 8);
                memcpy(tiles + 32 * k, t, 32);
                got[k] = 1;
            }
    }
    int packed = rs16(s, a + 8) & 0x8000;
    if (packed && shared[i].sub) return fail_size("%s: its tiles are packed and a picture inside it is where the game knows it: it is taken as it is", shared[i].name);
    if (!packed) {                                        /* (in place: the same length, everything where it was) */
        uint8_t *b = malloc(size);
        if (!b) return fail_size("out of memory");
        for (uint32_t k = 0; k < size; k++) b[k] = rs8(s, a + k);
        memcpy(b + 10, tiles, 32u * (uint32_t)n);
        if (shared[i].kind == PICTURE && pal != 0xFFFFFFFFu)  /* (the colours changed, and only they: the rest as it was) */
            for (uint32_t k = 0; k < 64 && pal + 2 * k + 2 <= size; k++)
                if (v->pal[k] != ((b[pal + 2 * k] << 8 | b[pal + 2 * k + 1]) & 0x0EEE)) { b[pal + 2 * k] = (uint8_t)(v->pal[k] >> 8); b[pal + 2 * k + 1] = (uint8_t)v->pal[k]; }
        *out = b;
        return size;
    }
    /* packed: the header, the tiles plain, the palette, then the map (a picture's) or the frames */
    uint32_t tiles_len = 10 + 32u * (uint32_t)n, pal_len = pal != 0xFFFFFFFFu ? 128 : 0, rest_at, rest_len;
    if (shared[i].kind == PICTURE) { rest_at = map; rest_len = 4 + 2u * rs16(s, map) * rs16(s, map + 2); }
    else { rest_at = a + rs32(s, a + 4); rest_len = rs16(s, rest_at + rs16(s, rest_at + 2) - 2); }
    uint8_t *b = calloc(tiles_len + pal_len + rest_len, 1);
    if (!b) return fail_size("out of memory");
    put_be32(b, pal_len ? tiles_len : 0xFFFFFFFFu);
    put_be32(b + 4, tiles_len + pal_len);
    b[8] = (uint8_t)(n >> 8);
    b[9] = (uint8_t)n;
    memcpy(b + 10, tiles, 32u * (uint32_t)n);
    for (uint32_t k = 0; k < pal_len / 2; k++) {
        uint16_t c = rs16(s, a + pal + 2 * k);
        if (shared[i].kind == PICTURE && v->pal[k] != (c & 0x0EEE)) c = v->pal[k];
        b[tiles_len + 2 * k] = (uint8_t)(c >> 8);
        b[tiles_len + 2 * k + 1] = (uint8_t)c;
    }
    for (uint32_t k = 0; k < rest_len; k++) b[tiles_len + pal_len + k] = rs8(s, rest_at + k);
    *out = b;
    return tiles_len + pal_len + rest_len;
}

/* ---- the art map ------------------------------------------------------------------------------- */

#define PIECES_MAX 512
static struct art_piece pieces[PIECES_MAX];
static int npieces, overlaps;
static struct art_piece overlapped[8][2];               /* (the first: the piece left out, the one it overlapped) */

/* a logo bitmap at a: to the end of its tiles and its map (6 x 6; its colours are its team's) */
static uint32_t logo_size(const struct romsrc *s, uint32_t a)
{
    uint32_t end, map = a + rs32(s, a + 4);
    if (tiles_at(s, a + 8, NULL, 64, &end) < 0) return 0;
    return max_u32(end, map + 4 + 2 * 6 * 6) - a;
}

/* a team's banners (Teamblocksmap, TeamBitmaps, a league's own): to the end of their tiles and
 * map (their colours are elsewhere: a league's own point back at the cartridge's) */
static uint32_t banners_size(const struct romsrc *s, uint32_t a)
{
    uint32_t end, map = a + rs32(s, a + 4);
    if (tiles_at(s, a + 8, NULL, TILES_MAX, &end) < 0) return 0;
    return max_u32(end, map + 4 + 2u * rs16(s, map) * rs16(s, map + 2)) - a;
}

/* a photo at a (UnpackPicture's: 3 bits a pixel, 24 bytes a tile after its count at +8; a map of
 * its own at +4, or the photos' common one) */
static uint32_t photo_size(const struct romsrc *s, uint32_t a)
{
    uint32_t n = 10 + 24u * rs16(s, a + 8), map = rs32(s, a + 4);
    return map ? max_u32(n, map + 4 + 2 * 6 * 6) : n;
}

static void piece(uint32_t at, uint32_t size, int kind, int which)
{
    if (!at || !size || npieces == PIECES_MAX) return;
    for (int i = 0; i < npieces; i++) if (pieces[i].at == at) return;   /* (one piece many teams show) */
    pieces[npieces++] = (struct art_piece){ at, size, (uint8_t)kind, (uint8_t)which };
}

static int by_at(const void *a, const void *b)
{
    const struct art_piece *x = a, *y = b;
    return x->at < y->at ? -1 : x->at > y->at;
}

void art_map_make(void)
{
    struct romsrc s;
    romsrc_game(&s);
    npieces = overlaps = 0;
    for (int i = 0; i < SHARED; i++) {
        uint32_t a = league.asset[shared[i].art];
        piece(a, art_shared_size(&s, i, a), PIECE_SHARED, shared[i].art);
    }
    piece(league.asset[ART_TeamBitmaps], banners_size(&s, league.asset[ART_TeamBitmaps]), PIECE_BANNERS, ART_TeamBitmaps);
    piece(SYM_Teamblocksmap, banners_size(&s, SYM_Teamblocksmap), PIECE_BANNERS, 0);   /* (setupTeamBlocksMap: the cartridge's always) */
    piece(league.table[LG_Teamblocksmap], banners_size(&s, league.table[LG_Teamblocksmap]), PIECE_BANNERS, 0);
    for (int k = 0; k < 8; k++) {                         /* the arena animations' sprites (RunArenaAnim: the cartridge's ArenaAnims) */
        uint32_t a = rs32(&s, SYM_ArenaAnims + 8u * (uint32_t)k + 4);
        piece(a, sprites_size(&s, a), PIECE_ARENA, k);
    }
    uint32_t slots = league.table[LG_BannerSlotMap];
    if (slots) piece(slots, 4 + 2u * rs16(&s, slots) * rs16(&s, slots + 2), PIECE_BANNER_SLOTS, 0);
    for (int t = 0; t < league.teams && t < LEAGUE_MAX; t++) {
        const struct LgTeam *T = &lg_team[t];
        piece(T->logo, logo_size(&s, T->logo), PIECE_LOGO, t);
        uint32_t end;
        if (tiles_at(&s, T->ice + 8, NULL, 24, &end) == 24) piece(T->ice, end - T->ice, PIECE_ICE, t);
        for (int k = 0; k < T->pictures; k++) piece(T->picture[k].pic, photo_size(&s, T->picture[k].pic), PIECE_PHOTO, t);
        piece(league.banner_tiles[t], 22 * 32, PIECE_TEAM_BANNERS, t);
    }
    qsort(pieces, (size_t)npieces, sizeof pieces[0], by_at);
    int n = 0;                                            /* (a piece inside the one before: left out, counted; across its end:
                                                             the one before, its length reckoned, ends where it begins) */
    for (int i = 0; i < npieces; i++) {
        if (n && pieces[i].at < pieces[n - 1].at + pieces[n - 1].size) {
            if (pieces[i].at + pieces[i].size > pieces[n - 1].at + pieces[n - 1].size) pieces[n - 1].size = pieces[i].at - pieces[n - 1].at;
            else {
                if (overlaps < 8) { overlapped[overlaps][0] = pieces[i]; overlapped[overlaps][1] = pieces[n - 1]; }
                overlaps++;
                continue;
            }
        }
        pieces[n++] = pieces[i];
    }
    npieces = n;
}

const struct art_piece *art_map(int *n) { *n = npieces; return pieces; }
int art_map_overlaps(void) { return overlaps; }

void art_map_report(void)
{
    char a[80], b[80];
    for (int k = 0; k < overlaps && k < 8; k++)
        printf("  overlapping: %s at $%06X (%u bytes), inside %s at $%06X (%u bytes)\n", art_piece_name(&overlapped[k][0], a, sizeof a),
               overlapped[k][0].at, overlapped[k][0].size, art_piece_name(&overlapped[k][1], b, sizeof b), overlapped[k][1].at, overlapped[k][1].size);
}

const struct art_piece *art_piece_at(uint32_t number)
{
    static int last;                                      /* (DMA reads go a piece at a time) */
    if (last < npieces && number - pieces[last].at < pieces[last].size) return &pieces[last];
    int lo = 0, hi = npieces - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        if (number < pieces[mid].at) hi = mid - 1;
        else if (number - pieces[mid].at >= pieces[mid].size) lo = mid + 1;
        else return &pieces[last = mid];
    }
    return NULL;
}

const char *art_piece_name(const struct art_piece *p, char *out, size_t size)
{
    static const char *const kinds[PIECES] = { "", "logo", "centre ice", "photo", "banners", "banner slots", "banner tiles", "" };
    const char *shared_name = "?";
    for (int i = 0; i < SHARED; i++) if (shared[i].art == p->which) shared_name = shared[i].name;
    if (p->kind == PIECE_SHARED) snprintf(out, size, "%s", shared_name);
    else if (p->kind == PIECE_BANNERS) snprintf(out, size, "%s", p->which ? "TeamBitmaps" : "Teamblocksmap");
    else if (p->kind == PIECE_BANNER_SLOTS) snprintf(out, size, "BannerSlotMap");
    else if (p->kind == PIECE_ARENA) snprintf(out, size, "arena animation %d", p->which + 1);
    else snprintf(out, size, "team %d's %s", p->which, kinds[p->kind]);
    return out;
}
