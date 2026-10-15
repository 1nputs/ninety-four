/* Draws the VDP's picture one line at a time: the background colour, planes B and A (A replaced by
 * the window in its area), and the sprites, each pixel taken from the highest layer that is not
 * transparent: B low, A low, sprites low, B high, A high, sprites high. Mode 5 without shadow /
 * highlight or interlace, which the game never turns on. */
#include <string.h>
#include "vdp.h"

#define LINES 224

/* a VRAM word (VDP byte order) */
static uint16_t vw(uint32_t a) { a &= 0xFFFE; return (uint16_t)(vdp.vram[a] << 8 | vdp.vram[a + 1]); }

/* a 9-bit colour to 0x00RRGGBB: each 3-bit level times 34 (0-238), as Genesis Plus GX maps the
 * normal (not shadowed or highlighted) colours */
static uint32_t rgb(uint16_t c)
{
    uint32_t r = (c >> 1) & 7, g = (c >> 5) & 7, b = (c >> 9) & 7;
    return (r * 34) << 16 | (g * 34) << 8 | (b * 34);
}

/* pixel x of row y (0-7) of the tile of name table entry e: a colour index 0-15 */
static int tile_pixel(uint16_t e, int x, int y)
{
    if (e & 0x0800) x = 7 - x;
    if (e & 0x1000) y = 7 - y;
    uint8_t b = vdp.vram[((e & 0x7FF) << 5) + (y << 2) + (x >> 1)];
    return (x & 1) ? (b & 0xF) : (b >> 4);
}

/* one line of a scroll plane: colour (palette * 16 + index, 0 when transparent) and priority */
static void plane_line(int line, uint32_t table, int hscroll, int plane, uint8_t *col, uint8_t *pri, int width)
{
    static const int sizes[4] = { 32, 64, 32, 128 };
    int w = sizes[vdp.reg[16] & 3], h = sizes[(vdp.reg[16] >> 4) & 3];
    for (int x = 0; x < width; x++) {
        int vs = (vdp.reg[11] & 4) ? vdp.vsram[((x >> 4) * 2 + plane) % 40] : vdp.vsram[plane];
        int py = (line + vs) & (h * 8 - 1);
        int px = (x - hscroll) & (w * 8 - 1);
        uint16_t e = vw(table + (uint32_t)(((py >> 3) * w + (px >> 3)) << 1));
        int c = tile_pixel(e, px & 7, py & 7);
        col[x] = (uint8_t)(c ? (((e >> 13) & 3) << 4) | c : 0);
        pri[x] = (uint8_t)(e >> 15);
    }
}

/* the window plane's area on this line: columns [x0, x1) */
static void window_span(int line, int width, int *x0, int *x1)
{
    int wy = (vdp.reg[18] & 0x1F) * 8, down = vdp.reg[18] & 0x80;
    if (down ? line >= wy : line < wy) { *x0 = 0; *x1 = width; return; }   /* the whole line */
    int wx = (vdp.reg[17] & 0x1F) * 16, right = vdp.reg[17] & 0x80;
    if (wx > width) wx = width;
    if (right) { *x0 = wx; *x1 = width; } else { *x0 = 0; *x1 = wx; }
}

/* one line of sprites, in link order; the first opaque pixel wins. Up to 20 sprites and 320
 * pixels a line in 40-cell mode (16 and 256 in 32-cell mode); a sprite at x 0 hides the
 * sprites after it on that line once another sprite was on it. */
static void sprite_line(int line, int width, uint8_t *col, uint8_t *pri)
{
    int h40 = width == 320;
    uint32_t sat = h40 ? (uint32_t)(vdp.reg[5] & 0x7E) << 9 : (uint32_t)(vdp.reg[5] & 0x7F) << 9;
    int max_sprites = h40 ? 80 : 64, per_line = h40 ? 20 : 16, dots = h40 ? 320 : 256;
    memset(col, 0, (size_t)width);
    int n = 0, link = 0, shown = 0, masked = 0, any = 0;
    do {
        uint32_t s = sat + (uint32_t)(link << 3);
        int y = (vw(s) & 0x3FF) - 128;
        int size = vdp.vram[s + 2];
        int sh = (size & 3) + 1, sw = ((size >> 2) & 3) + 1;
        link = vdp.vram[s + 3] & 0x7F;
        if (line >= y && line < y + sh * 8) {
            if (++shown > per_line) break;
            uint16_t e = vw(s + 4);
            int x = (vw(s + 6) & 0x1FF) - 128;
            if ((vw(s + 6) & 0x1FF) == 0 && any) masked = 1;
            any = 1;
            int row = line - y;
            if (e & 0x1000) row = sh * 8 - 1 - row;
            for (int cx = 0; cx < sw * 8; cx++) {
                if (dots-- <= 0) break;
                if (masked) continue;
                int sx = x + cx;
                if (sx < 0 || sx >= width || col[sx]) continue;
                int tx = (e & 0x0800) ? sw * 8 - 1 - cx : cx;
                uint16_t t = (uint16_t)((e & 0x7FF) + (tx >> 3) * sh + (row >> 3));
                int c = tile_pixel((uint16_t)(t & 0x7FF), tx & 7, row & 7);
                if (!c) continue;
                col[sx] = (uint8_t)((((e >> 13) & 3) << 4) | c);
                pri[sx] = (uint8_t)(e >> 15);
            }
            if (masked) break;
        }
    } while (link && link < max_sprites && ++n < max_sprites);
}

int vdp_render(uint32_t *pixels)
{
    int width = (vdp.reg[12] & 0x81) ? 320 : 256;
    uint32_t pal[64];
    for (int i = 0; i < 64; i++) pal[i] = rgb(vdp.cram[i]);
    uint32_t back = pal[vdp.reg[7] & 0x3F];
    uint32_t ta = (uint32_t)(vdp.reg[2] & 0x38) << 10, tb = (uint32_t)(vdp.reg[4] & 7) << 13;
    uint32_t twin = width == 320 ? (uint32_t)(vdp.reg[3] & 0x3C) << 10 : (uint32_t)(vdp.reg[3] & 0x3E) << 10;
    uint32_t hs = (uint32_t)(vdp.reg[13] & 0x3F) << 10;
    for (int line = 0; line < LINES; line++) {
        uint32_t *out = pixels + line * 320;
        if (!(vdp.reg[1] & 0x40)) {                       /* display off */
            for (int x = 0; x < width; x++) out[x] = back;
            continue;
        }
        int hline = 0;
        switch (vdp.reg[11] & 3) {
        case 2: hline = line & ~7; break;                 /* per cell */
        case 3: hline = line; break;                      /* per line */
        }
        int hsa = vw(hs + (uint32_t)(hline << 2)) & 0x3FF, hsb = vw(hs + (uint32_t)(hline << 2) + 2) & 0x3FF;
        uint8_t ca[320], pa[320], cb[320], pb[320], cs[320], ps[320];
        plane_line(line, tb, hsb, 1, cb, pb, width);
        plane_line(line, ta, hsa, 0, ca, pa, width);
        int w0, w1;
        window_span(line, width, &w0, &w1);
        int wcells = width == 320 ? 64 : 32;
        for (int x = w0; x < w1; x++) {
            uint16_t e = vw(twin + (uint32_t)((((line >> 3) * wcells) + (x >> 3)) << 1));
            int c = tile_pixel(e, x & 7, line & 7);
            ca[x] = (uint8_t)(c ? (((e >> 13) & 3) << 4) | c : 0);
            pa[x] = (uint8_t)(e >> 15);
        }
        sprite_line(line, width, cs, ps);
        for (int x = 0; x < width; x++) {
            uint8_t c = 0;
            if (cs[x] && ps[x]) c = cs[x];
            else if (ca[x] && pa[x]) c = ca[x];
            else if (cb[x] && pb[x]) c = cb[x];
            else if (cs[x]) c = cs[x];
            else if (ca[x]) c = ca[x];
            else if (cb[x]) c = cb[x];
            out[x] = c ? pal[c] : back;
        }
    }
    return width;
}
