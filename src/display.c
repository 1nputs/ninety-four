/* display94.asm: game play display. Every frame the vertical blank code builds the sprite list,
 * sends it and other graphics to video memory and scrolls the rink. This file also draws the
 * sprites for the rink, the referee, faceoffs, the zamboni and the crowd. */
#include "game.h"
#include "hal.h"
#include "rom.h"
#include "symbols_gen.h"
#include "view.h"

/* display94.asm:updatesound. Move the crowd noise volume (psg noise channel, asv) toward
 * crowdlevel. */
void updatesound(void)
{
    uint16_t d0 = (uint16_t)((g.crowdlevel << 3) + 0x400);
    if (d0 > 0xEFF) d0 = 0xEFF;
    int16_t d2 = 0x28;
    int16_t diff = (int16_t)(d0 - g.asv);
    if (diff > d2 || diff < -d2) {
        if (!(diff > d2)) d2 = (int16_t)-d2;
        g.asv = (uint16_t)(g.asv + d2);
        if ((int16_t)g.asv < 0) g.asv = 0;
    }
    psg_write(0xC8);
    psg_write(1);
    psg_write((uint8_t)(((g.asv >> 8) ^ 0xF) | 0xF0));
}

/* display94.asm:KillCrowd. Silence the crowd noise (psg). */
void KillCrowd(void)
{
    psg_write(0xE7);
    psg_write(0xDF);
    psg_write(0xC8);
    psg_write(1);
    psg_write(0xFF);
}

#include "display_tables_gen.h"
#include "display_strings_gen.h"
#include <string.h>

/* The sprite builders share three registers: a5 = the DMA list being filled (long source, word
 * length in words, word VRAM address per entry), a6 = the sprite attribute table being filled,
 * d6 = the sprite link counter. */
#define SAT_MAX 0x40
static inline uint8_t *dmalist(void) { return g.DMAList; }
static inline uint8_t *satbase(void) { return g.Satt; }
/* one DMA list entry */
static inline void dma_entry(struct Vid *v, uint32_t src, uint16_t words, uint16_t vram)
{
    wr32(v->dma, src);
    wr16(v->dma + 4, words);
    wr16(v->dma + 6, vram);
    v->dma += 8;
}
/* one hardware sprite: y, size / link, tile word, x */
static inline void sat_entry(struct Vid *v, uint16_t y, uint8_t size, uint16_t tile, uint16_t x)
{
    wr16(v->sat, y);
    v->sat[2] = size;
    v->sat[3] = (uint8_t)v->link;
    wr16(v->sat + 4, tile);
    wr16(v->sat + 6, (uint16_t)(x + view_shift()));      /* (the wide view: to the middle) */
    v->sat += 8;
    v->link++;
}

/* display94.asm:VBlank. The main vblank code for game play: DumpSprites when the frame is
 * ready (dfok), the palette fade, then the game clock (a second every $AAA jiffies, the
 * shootout clock instead in a penalty shot / shootout, a beep at 60 seconds), vcount and the
 * music. */
void VBlank(void)
{
    if (!(g.disflags & HI(dfng))) {
        if (g.disflags & HI(dfok)) {
            g.disflags &= (uint16_t)~HI(dfok);
            DumpSprites();
        }
        cramfade();
    }
    if (!(g.sflags & HI(sfpz))) {
        int run = (g.gmode2 & HI(2)) || !(g.gmode & (1 << gmclock));
        if (run) {
            if (g.gmode2 & HI(1)) {                       /* penalty shot / shootout clock */
                if (g.shootoutclock != 0) {
                    uint16_t j = g.shootoutjiffy;
                    g.shootoutjiffy = (uint16_t)(j - 0xAAA);
                    if (j < 0xAAA) {
                        g.disflags |= HI(dfclock);
                        if (!(g.gmode2 & HI(7))) g.shootoutclock--;
                    }
                }
            } else if (g.gameclock != 0) {
                uint16_t j = g.gamejiffy;
                g.gamejiffy = (uint16_t)(j - 0xAAA);
                if (j < 0xAAA) {
                    g.disflags |= HI(dfclock);
                    g.gameclock--;
                    if ((int16_t)g.gameclock <= 0x3D && (int16_t)g.gameclock >= 0x3C)
                        sfx(SYM_SFXbeep2);                /* one minute left */
                }
            }
        }
    }
    g.vcount++;
    p_music_vblank();
}

/* display94.asm:vb2. The vblank used for palette fades only, no DMAs. */
void vb2(void)
{
    if (!(g.disflags & HI(dfng))) cramfade();
    g.vcount++;
    p_music_vblank();
}

/* display94.asm:DumpSprites. Transfer the scroll values, the sprite table and the DMA list. */
void DumpSprites(void)
{
    SetScroll2();
    DumpSprites2();
}

/* display94.asm:DumpSprites2. The sprite table, then the DMA list. */
void DumpSprites2(void)
{
    DoDMA(ram_addr(g.Satt), g.Sattsize, g.VSPRITES);
    DoDMAList();
}

/* display94.asm:DoDMAList. Transfer every entry of the DMA list, last first. */
void DoDMAList(void)
{
    const uint8_t *base = dmalist(), *a6 = base + (g.DMAlistend - ram_addr(base));   /* (DMAlistend: the 68000 address of its end) */
    while (a6 != base) {
        a6 -= 8;
        DoDMA(rd32(a6), rd16(a6 + 4), rd16(a6 + 6));
    }
}

/* display94.asm:SetScroll2. Write Hscroll / Vscroll to the VDP. */
void SetScroll2(void)
{
    Vmaddr((uint16_t)(g.VSCRLPM + 2));
    vdp_data(g.Hscroll);
    vdp_ctrl(VSRAM_W + (2u << 16));
    vdp_data(g.Vscroll);
}

/* display94.asm:setvideo. Not vblank code: set up RAM for the vblank transfers. Called once per
 * game frame. Builds the DMA list and the sprite table from every object on the ice, then
 * hands them to the vblank (dfok). */
void setvideo(void)
{
    while (g.disflags & HI(dfok)) hal_wait_vblank();      /* the last frame is still to go */
    struct Vid v = { dmalist(), satbase(), 1 };
    updatescroll(&v);
    checkfo(&v);
    checksso(&v);
    setsortcords(&v);
    setffo(&v);
    showclock(&v);
    showzam(&v);
    AddArenaAnimSprite(&v.sat, &v.link);
    showcrowd(&v);
    showref(&v);
    Crowd_Noise();
    if (v.sat == satbase()) {                             /* no sprites: one blank sprite */
        memset(v.sat, 0, 8);
        v.sat += 8;
    }
    v.sat[-5] = 0;                                        /* end the sprite list */
    g.Sattsize = (uint16_t)((v.sat - satbase()) >> 1);
    g.DMAlistend = ram_addr(v.dma);
    g.disflags |= HI(dfok);
}

/* display94.asm:updatescroll. Scroll from Hpos / Vpos; when the view moves to a new rink row
 * queue the map rows that scrolled in (from Rinktilelist, or RevRinkTilelist in a reverse angle
 * replay), up to $20 of them. */
void updatescroll(struct Vid *v)
{
    if (g.sflags & HI(sfhor)) return;
    g.Hscroll = (uint16_t)(-0x40 - (int16_t)g.Hpos + view_shift());
    int16_t d1 = (int16_t)g.Vpos;
    g.Vscroll = (uint16_t)-d1;
    int16_t d4 = (int16_t)g.Oldrow;
    d1 = (int16_t)(d1 >> 3);
    g.Oldrow = (uint16_t)d1;
    if (d1 == d4) return;                                 /* the same row as last frame */
    uint32_t map = (g.sflags4 & HI(4)) ? ART(RevRinkTilelist) : ART(Rinktilelist);
    map += rd32(rom_ptr(map) + 4);                        /* the map header: chars per row, then the rows */
    uint16_t perrow = rd16(rom_ptr(map));
    int up = d1 < d4;
    for (int d3 = 0x1F; d3 >= 0; d3--) {                  /* dbeq: stop at the old row */
        int16_t d0 = (int16_t)(((-d1 + (up ? 0x1D : 0x1E)) & 0x1F) << 7);
        d0 = (int16_t)(d0 + g.maps[1].vram);
        int16_t row = (int16_t)((up ? 0x3D : 0x1E) - d1);
        uint16_t off = (uint16_t)((uint16_t)row * perrow * 2);
        dma_entry(v, map + 4 + (int16_t)off, perrow, (uint16_t)d0);
        d1 = (int16_t)(d1 + (up ? 1 : -1));
        if (d1 == d4) break;
    }
}

/* display94.asm:showref. Queue the 8 rows of the ref window map (RefRamMap) when it changed. */
void showref(struct Vid *v)
{
    if (!(g.sflags2 & HI(sf2refref))) return;
    g.sflags2 &= (uint16_t)~HI(sf2refref);
    uint32_t a0 = ram_addr(g.RefRamMap);
    uint16_t d2 = g.maps[0].colshift;
    uint16_t d0 = (g.sflags & HI(sfhor)) ? 0xF : 2;
    d0 = (uint16_t)(d0 << d2);
    uint16_t d1 = (uint16_t)(2 << d2);
    d0 += 2;
    if (g.sflags & HI(sfhor)) d0 += 0xB;
    d0 = (uint16_t)((d0 << 1) + g.maps[0].vram);
    for (int i = 0; i < 8; i++) {
        dma_entry(v, a0, 7, d0);
        a0 += 0xE;
        d0 += d1;
    }
}

/* display94.asm:checkfo. The faceoff sprites: 3 entries of fofdata2 (word frame, word flags)
 * from FaceOffSprites at fodropx / fodropy. */
void checkfo(struct Vid *v)
{
    if (g.sflags & HI(sfpz)) return;
    if (!(g.disflags & HI(4))) return;
    const struct FoMan *a3 = g.fomen;
    for (int i = 0; i < 3; i++) {                         /* a3 only advances past a drawn entry */
        int16_t d4 = (int16_t)a3->frame;
        if (d4 <= 0) continue;
        const uint8_t *a2 = rom_ptr(ART(FaceOffSprites) + rd32(rom_ptr(ART(FaceOffSprites)) + 4));
        unsigned idx = (unsigned)d4 * 2;
        int16_t d5 = (int16_t)(((rd16(a2 + idx + 2) - rd16(a2 + idx)) >> 3) - 1);
        a2 += rd16(a2 + idx);
        uint16_t flags = a3->attribute;
        for (; d5 >= 0; d5--, a2 += 8) {
            uint16_t y = (uint16_t)(rd16(a2 + 2) + 0x80 + g.fodropy);
            int16_t x = (int16_t)rd16(a2);
            if (flags & 0x800) {                          /* x flip */
                x = (int16_t)(((a2[7] & 0xC) + 4) << 1);
                x = (int16_t)(-x - (int16_t)rd16(a2));
            }
            x = (int16_t)(x + 0x80 + g.fodropx);
            uint16_t tile = (uint16_t)((rd16(a2 + 6) & 0xF800) | rd16(a2 + 4));
            tile ^= (uint16_t)(flags & 0xF800);
            tile += g.faceoffvrcset;
            sat_entry(v, y, a2[7], tile, (uint16_t)x);
        }
        a3++;
    }
}

/* display94.asm:showzam. The zamboni: frame 1, frame 2-4 by x, then frame 5 (home not ahead)
 * or a frame by x. */
void showzam(struct Vid *v)
{
    int16_t d0 = (int16_t)g.zamx;
    if (d0 < 0) return;
    d0 = (int16_t)((uint16_t)d0 >> 2);
    int16_t d1 = 0x12A;
    uint16_t d3 = g.ExtraChars;
    SetSframe(ART(ZamFrameList), d0, d1, 1, d3, v);
    uint16_t d2 = (uint16_t)(m68k_divu((uint32_t)(uint16_t)(d0 >> 1), 3) >> 16);   /* (x/2) mod 3 */
    d2 += 2;
    SetSframe(ART(ZamFrameList), d0, d1, d2, d3, v);
    d2 = 5;
    if (!(g.teams[0].score > g.teams[1].score)) { SetSframe(ART(ZamFrameList), d0, d1, d2, d3, v); return; }
    int16_t t = (int16_t)(d0 - 0xDA);
    if (t < 0) t = 0;
    t = (int16_t)((uint16_t)t >> 2);
    t = (int16_t)(t + t);
    if (!(t < 0x1A)) t = 0x18;
    SetSframe(ART(ZamFrameList), d0, d1, zam_ftab[t / 2], d3, v);
}

/* display94.asm:SetSframe. Draw one sprite frame: a0 = frame list, d0 / d1 = x / y, d2 = the
 * frame, d3 = its start char in VRAM. */
void SetSframe(uint32_t list, int16_t x, int16_t y, uint16_t frame, uint16_t startchar, struct Vid *v)
{
    if ((int16_t)v->link >= SAT_MAX) return;
    const uint8_t *a0 = rom_ptr(list + rd32(rom_ptr(list) + 4));
    unsigned idx = frame * 2;
    int16_t d4 = (int16_t)(((rd16(a0 + idx + 2) - rd16(a0 + idx)) >> 3) - 1);
    a0 += rd16(a0 + idx);
    for (; d4 >= 0; d4--, a0 += 8) {
        uint16_t tile = (uint16_t)((rd16(a0 + 6) & 0xF800) + rd16(a0 + 4) + startchar);
        sat_entry(v, (uint16_t)(rd16(a0 + 2) + y), a0[7], tile, (uint16_t)(rd16(a0) + x));
        if (v->link == SAT_MAX) return;
    }
}

/* one crowd frame (0 = none) from CrowdFrameList, clipped to the view; horizontal frames are
 * 31 later */
static void crowd_frame(int16_t d0, const uint8_t *a1, int16_t d4, int16_t d5, struct Vid *v)
{
    d0 = (int8_t)d0;
    if (d0 == 0) return;
    if (g.sflags & HI(sfhor)) d0 = (int16_t)(d0 + 0x1F);
    if ((int16_t)v->link >= SAT_MAX) return;
    unsigned idx = (unsigned)d0 * 2;
    int16_t n = (int16_t)(((rd16(a1 + idx + 2) - rd16(a1 + idx)) >> 3) - 1);
    const uint8_t *a0 = a1 + rd16(a1 + idx);
    int16_t x0 = (int16_t)(d4 + 0xC0 - 0x90), x1 = (int16_t)(d4 + 0xC0 + 0x80);
    int16_t y0 = (int16_t)(0x170 - d5 - 0x80), y1 = (int16_t)(0x170 - d5 + 0x70);
    for (; n >= 0; n--, a0 += 8) {
        int16_t sx = (int16_t)rd16(a0), sy = (int16_t)rd16(a0 + 2);
        if (y0 > sy || y1 < sy || x0 - view_more() > sx || x1 + view_more() < sx) continue;
        uint16_t tile = (uint16_t)((rd16(a0 + 6) & 0xF800) + rd16(a0 + 4) + g.animchars);
        sat_entry(v, (uint16_t)(sy + 0x70 - y0), a0[7], tile, (uint16_t)(sx + 0x70 - x0));
        if (v->link == SAT_MAX) return;
    }
}

/* display94.asm:showcrowd. The crowd sprites: up to 3 frames per penalty box nibble, then the
 * two crowdframe frames. None in a reverse angle replay. */
void showcrowd(struct Vid *v)
{
    if (g.sflags4 & HI(4)) return;
    const uint8_t *a1 = rom_ptr(ART(CrowdFrameList) + rd32(rom_ptr(ART(CrowdFrameList)) + 4));
    int16_t d4 = (int16_t)g.Hpos, d5 = (int16_t)g.Vpos;
    if (g.sflags & HI(sfhor)) { d4 = -0x40; d5 = 0x100; }
    uint8_t pb = (uint8_t)(g.PBnum >> 8);
    for (int pass = 0; pass < 2; pass++) {
        int16_t count = pass ? (pb >> 4) : (pb & 0xF);
        int16_t first = pass ? 0x1D : 0x1A;
        if (count > 3) count = 3;
        for (int i = count - 1; i >= 0; i--) crowd_frame((int16_t)(first + i), a1, d4, d5, v);
    }
    crowd_frame((int8_t)(g.crowdframe >> 8), a1, d4, d5, v);
    crowd_frame((int8_t)g.crowdframe, a1, d4, d5, v);
}

/* the ROM address of the '0'..'9' Strings inside showclock (a local label, not in the symbol
 * table): the first "\0\4" "0" "\0" after the routine's entry */
static uint32_t clock_digits(void)
{
    static uint32_t addr;
    if (!addr) {
        const uint8_t pat[8] = { 0, 4, '0', 0, 0, 4, '1', 0 };
        for (uint32_t a = SYM_showclock; a < SYM_checksso; a += 2)
            if (memcmp(rom_ptr(a), pat, 8) == 0) { addr = a; break; }
    }
    return addr;
}
/* one clock digit of the horizontal rink */
static void clock_digit(uint16_t d)
{
    print(clock_digits() + d * 4);
}
/* one tile of the vertical clock, written backward from a0: the high word of the divu result */
static void clock_char(uint32_t *d0, const uint8_t *font, int k)
{
    uint16_t digit = (uint16_t)(*d0 >> 16);
    uint16_t tile = (uint16_t)(rd16(font + 0x64 + (int16_t)(digit << 1)) + g.smallfontchars);
    tile |= 0x8000;
    g.clocktiles[k] = tile;
    *d0 = (uint32_t)(int32_t)(int16_t)*d0;
}

/* display94.asm:showclock. The game clock on screen. Horizontal rink: printed with the big
 * digits (not in a shootout). Vertical rink: the 5 tiles go in the DMA list when dfclock is
 * set, the shootout clock instead in a penalty shot / shootout. */
void showclock(struct Vid *v)
{
    if (g.sflags & HI(sfhor)) {
        if (g.gmode2 & HI(0)) return;
        printz(str_showclock_1, sizeof str_showclock_1);
        uint32_t d0 = (uint32_t)(int32_t)(int16_t)g.gameclock;
        d0 = m68k_divu(d0, 0x258);                        /* tens of minutes */
        if ((uint16_t)d0 == 0) g.printx += 1;
        else clock_digit((uint16_t)d0);
        d0 = (uint32_t)(int32_t)(int16_t)(d0 >> 16);
        d0 = m68k_divu(d0, 0x3C);                         /* minutes */
        clock_digit((uint16_t)d0);
        d0 = (uint32_t)(int32_t)(int16_t)(d0 >> 16);
        g.printx += 2;
        d0 = m68k_divu(d0, 0xA);                          /* tens of seconds */
        clock_digit((uint16_t)d0);
        d0 = (uint32_t)(int32_t)(int16_t)(d0 >> 16);
        clock_digit((uint16_t)d0);
        printz(str_showclock_2, sizeof str_showclock_2);
        return;
    }
    if (!(g.disflags & HI(dfclock))) return;
    g.disflags &= (uint16_t)~HI(dfclock);
    if (g.sflags2 & HI(sf2replay)) return;
    if (g.gsp == 4) return;
    const uint8_t *font = rom_ptr(ART(SmallFontMap) + rd32(rom_ptr(ART(SmallFontMap)) + 4));   /* the 5 tiles, written backward */
    uint16_t colon = (uint16_t)((rd16(font + 0x78) + g.smallfontchars) | 0x8000);
    g.clocktiles[2] = colon;
    uint32_t d0 = (g.gmode2 & HI(1)) ? g.shootoutclock : g.gameclock;
    d0 = (uint32_t)(int32_t)(int16_t)d0;
    d0 = m68k_divu(d0, 0xA);
    clock_char(&d0, font, 4);                             /* seconds ones */
    d0 = m68k_divu(d0, 6);
    clock_char(&d0, font, 3);                             /* seconds tens */
    d0 = m68k_divu(d0, 0xA);                              /* (2: the colon) */
    clock_char(&d0, font, 1);                             /* minutes ones */
    d0 = (d0 << 16) | (d0 >> 16);
    if (d0 == 0) d0 = 0xFFF00000u;                        /* a blank leading zero */
    clock_char(&d0, font, 0);                             /* minutes tens */
    uint32_t src = ram_addr(g.clocktiles);
    uint16_t words = 5;
    const struct Plane *m = &g.maps[0];
    uint16_t y = 0x18, x = 3;
    if (g.sflags & HI(sfhor)) { m = &g.maps[1]; y = 5; x = 0xD; }
    uint16_t dest = (uint16_t)((((y << m->colshift) + x) << 1) + m->vram);
    dma_entry(v, src, words, dest);
}

/* the off-screen arrow of one controlled player: a0 = the object, a3 = the sso entry, d3 = its
 * first arrow frame */
static void sso_arrow(const struct RinkObj *a0, struct SpriteHead *a3, uint16_t d3, struct Vid *v)
{
    if (a0->Zpos < 0) return;                             /* not on the ice */
    ST_W(a3->frame);                                      /* a3 is an sso entry: x 0, y 2, attribute 4, frame 6, oldframe 8, VRoffs $A, VRchar $12 */
    int16_t d0 = a0->Xpos;
    if ((g.gmode2 & HI(0)) || (g.sflags2 & HI(sf2replay)) || (g.BA_PS_flags & HI(2))) {
        int16_t ax = d0 < 0 ? (int16_t)-d0 : d0;
        if (!(ax < 0xA6)) return;
    }
    int16_t d1 = a0->Ypos;
    if (g.sflags & HI(sfhor)) {
        int16_t t = d0; d0 = (int16_t)-d1; d1 = (int16_t)(t - 0xC5);
    } else {
        d0 = (int16_t)(d0 - (int16_t)g.Hpos);
        d1 = (int16_t)(d1 - (int16_t)g.Vpos);
    }
    unsigned d2 = 0;
    if (!(d0 < 0x74 + view_more())) d2 |= 8;
    if (!(d0 > -0x74 - view_more())) d2 |= 4;
    if (!(d1 < 0x64)) d2 |= 1;
    if (!(d1 > -0x64)) d2 |= 2;
    if (d2 == 0) return;
    d2 = rom_ptr(SYM_jdtab)[d2];
    const uint16_t *t = sso_tab + d2 * 4;
    if (t[0] != 0) d0 = (int16_t)((int16_t)t[0] + ((int16_t)t[0] < 0 ? -view_more() : view_more()));
    d0 = (int16_t)(d0 + 0x100);
    a3->Xpos = d0;
    if (t[1] != 0) d1 = (int16_t)t[1];
    d1 = (int16_t)(-d1 + 0xF0);
    a3->Xfrac = (uint16_t)d1;                             /* (an arrow's y) */
    a3->frame = (uint16_t)(d3 + t[2]);
    a3->attribute = t[3];
    addframe2(d0, d1, a3, v);
}

/* display94.asm:checksso. Arrows for the controlled players when they are off screen (pads 3
 * and 4 too on the four way play). Nothing on the horizontal rink. */
void checksso(struct Vid *v)
{
    if (g.sflags & HI(sfhor)) return;
    sso_arrow(&g.pads[1], &g.sso[0], SYM_SPFarrow, v);    /* (Joy1Struct, and the next) */
    sso_arrow(&g.pads[2], &g.sso[1], SYM_SPFarrow + 3, v);
    if (g.cont3team == 0) return;
    sso_arrow(&g.pads[4], &g.sso[2], 0x346, v);           /* (Joy3Struct: the replay camera is between) */
    if (g.cont4team == 0) return;
    sso_arrow(&g.pads[5], &g.sso[3], 0x349, v);
}

/* display94.asm:setsortcords. addframe for the 16 objects in OOlist order. */
void setsortcords(struct Vid *v)
{
    for (int i = 0; i < 16; i++)
        addframe(&sortobj(g.OOlist[i] / 2)->rink, v);
}

/* display94.asm:setffo. The 7 objects tied to the rink scrolling (the gloves and the pads),
 * each moved by its x offset. */
void setffo(struct Vid *v)
{
    uppads(v);
    for (int i = 0; i < 7; i++) {
        struct RinkObj *a3 = &g.pads[i];
        uint8_t *before = v->sat;
        addframe(a3, v);
        if (before == v->sat) continue;
        int16_t d1 = (int16_t)a3->Xfrac;                  /* (the label's x offset) */
        wr16(before + 6, (uint16_t)(rd16(before + 6) + d1));
        wr16(before + 0xE, (uint16_t)(rd16(before + 0xE) + d1));
    }
}

/* display94.asm:uppads. Update the gloves object and the 6 pad objects from the pad nibbles
 * of PadControlBits / PadControlBits34 ($E = no change, $F = none), queueing new labels. */
void uppads(struct Vid *v)
{
    struct RinkObj *gl = &g.pads[6];                      /* (glovestruct) */
    gl->Zpos = (int16_t)(gl->Zpos | 0xFF00);              /* st Zpos (its high byte): off */
    int16_t d0 = (int8_t)(g.glovecords >> 8);
    if (d0 != 0) {
        int16_t d1 = (int8_t)g.glovecords;
        gl->Xpos = (int16_t)(d0 << 2);
        gl->Ypos = (int16_t)(d1 << 2);
        gl->Zpos = 0;
    }
    uint16_t d3 = g.PadControlBits;
    for (int d4 = 5, k = 0; d4 >= 0; d4--, k++) {
        struct RinkObj *a0 = &g.pads[k];
        if (d4 == 1) d3 = (uint16_t)(g.PadControlBits34 >> 4);
        uint16_t n = d3 & 0xF;
        uint16_t d1 = 0xF;
        if (n != 0xE) {
            a0->Zpos = (int16_t)(a0->Zpos | 0xFF00);      /* (st: off) */
            if (n == 0xF) { d3 >>= 4; continue; }
            const struct SortObj *o = sortobj(n);
            a0->Xpos = o->Xpos;
            a0->Ypos = o->Ypos;
            a0->Zpos = 0;
            d1 = (uint16_t)(((o->position & 0xFF) << 8) | o->rostnum);
        }
        if (d1 != g.padcont[k]) {
            g.padcont[k] = d1;
            QueuePadLabel(d1, a0, v);
        }
        d3 >>= 4;
    }
}

/* display94.asm:QueuePadLabel. Queue the 3 character label of a pad object: two digits (bits
 * 7-4 and 3-0, $F = blank) and a letter by bits 10-8 (none while sflags7 bit 7 is set). */
void QueuePadLabel(uint16_t code, struct RinkObj *pad, struct Vid *v)
{
    static const char letters[] = " DDLCRX\xFF";
    pad->Xfrac = 0;                                       /* (the label's x offset) */
    int16_t d2 = (int16_t)((code >> 4) & 0xF);
    if (d2 == 0) {
        d2 = -0x10;                                       /* a blank leading zero */
        pad->Xfrac -= 4;
    }
    RenderSmallFontChar((uint16_t)(d2 + 0x30), 0, pad, v);
    d2 = (int16_t)(code & 0xF);
    if (d2 == 0xF) d2 = -0x10;
    RenderSmallFontChar((uint16_t)(d2 + 0x30), 1, pad, v);
    uint16_t d = (g.sflags7 & 0x80) ? 0 : code;
    d = (uint16_t)((d >> 8) & 7);
    if (d == 0) pad->Xfrac += 4;
    RenderSmallFontChar((uint8_t)letters[d], 2, pad, v);
}

/* display94.asm:RenderSmallFontChar. DMA one small font tile to the object's chars: d2 = the
 * character, d0 = the char slot. */
void RenderSmallFontChar(uint16_t ch, uint16_t slot, const struct RinkObj *pad, struct Vid *v)
{
    const uint8_t *map = rom_ptr(ART(SmallFontMap) + rd32(rom_ptr(ART(SmallFontMap)) + 4));
    uint16_t tile = (uint16_t)(rd16(map + 4 + (int16_t)(ch << 1)) & 0x7FF);
    uint32_t src = ART(SmallFontMap) + 0xA + (int16_t)(tile << 5);
    uint16_t dest = (uint16_t)((slot + pad->VRchar) << 5);
    dma_entry(v, src, 0x10, dest);
}

/* display94.asm:addframe. Project object a3 with find3d, then addframe2. */
void addframe(struct RinkObj *o, struct Vid *v)
{
    if (o->Zpos < 0) return;
    int16_t d0 = o->Xpos, d1 = o->Ypos;
    find3d(&d0, &d1, o->Zpos);
    if (d1 == 0x4E20) return;                             /* off screen */
    addframe2(d0, d1, &o->head, v);
}

/* display94.asm:addframe2. d0 / d1 = the object's screen position. Queue the sprite tiles of its
 * frame (once per frame change, sharing tiles already sent) and write its sprites. */
void addframe2(int16_t x, int16_t y, struct SpriteHead *o, struct Vid *v)
{
    uint16_t attr_saved = o->attribute;
    int16_t d4 = (int16_t)o->frame;
    if (d4 <= 0) return;
    o->attribute ^= (uint16_t)(d4 & 0xF800);
    uint16_t fr = (uint16_t)(o->frame & 0x7FF);
    const uint8_t *a2 = rom_ptr(ART(Sprites) + rd32(rom_ptr(ART(Sprites)) + 4));
    unsigned idx = fr * 2;
    if (!((int16_t)idx < (int16_t)rd16(a2 + 2))) { o->attribute = attr_saved; return; }
    int16_t d5 = (int16_t)(((rd16(a2 + idx + 2) - rd16(a2 + idx)) >> 3) - 1);
    a2 += rd16(a2 + idx);
    uint16_t d3 = 0, prev_tile = 0, prev_count = 0;
    for (; d5 >= 0; d5--, a2 += 8) {
        if (fr != o->oldframe) {
            if (d5 == 0) o->oldframe = fr;
            uint16_t tile = rd16(a2 + 4);
            uint16_t count = rom_ptr(SYM_sizetab)[a2[7]];
            int dup = 0;
            if (!((int16_t)count > (int16_t)prev_count) && !((int16_t)tile < (int16_t)prev_tile)) {
                int16_t d0 = (int16_t)(prev_tile + prev_count - tile - count);
                if (d0 >= 0) dup = 1;                     /* inside the tiles already sent */
            }
            if (!dup) {
                d3 = (uint16_t)(d3 + prev_count);
                uint32_t src = ART(Spritetiles) + ((int32_t)(int16_t)tile << 5);
                dma_entry(v, src, (uint16_t)(count << 4), (uint16_t)((d3 + o->VRchar) << 5));
            }
            prev_tile = tile;
            prev_count = count;
            o->VRoffs[d5] = (uint8_t)d3;
        }
        int16_t yy = (int16_t)rd16(a2 + 2);
        if (o->attribute & HI(4)) {                       /* y flip */
            yy = (int16_t)(((a2[7] & 3) + 1) << 3);
            yy = (int16_t)(-yy - (int16_t)rd16(a2 + 2));
        }
        int16_t xx = (int16_t)rd16(a2);
        if (o->attribute & HI(3)) {                       /* x flip */
            xx = (int16_t)(((a2[7] & 0xC) + 4) << 1);
            xx = (int16_t)(-xx - (int16_t)rd16(a2));
        }
        uint16_t t = (uint16_t)((rd16(a2 + 6) ^ o->attribute) & 0xF800);
        if ((o->attribute & 1) && (t & 0x4000)) t |= 0x2000;   /* team 2 colours */
        t |= o->VRoffs[d5];
        t += o->VRchar;
        sat_entry(v, (uint16_t)(y + yy), a2[7], t, (uint16_t)(x + xx));
    }
    o->attribute = attr_saved;
}

/* display94.asm:find3d. Input d0 / d1 = field x / y, d2 = height. Output the screen position,
 * or d1 = $4E20 when off screen. */
void find3d(int16_t *x, int16_t *y, int16_t z)
{
    int16_t d0 = *x, d1 = *y;
    if (g.sflags & HI(sfhor)) {
        int16_t t = d0; d0 = (int16_t)-d1; d1 = (int16_t)(t - 0xC5);
    } else {
        d0 = (int16_t)(d0 - (int16_t)g.Hpos);
        d1 = (int16_t)(d1 - (int16_t)g.Vpos);
    }
    if (d0 > 0x90 + view_more() || d0 < -0x90 - view_more()) { *x = d0; *y = 0x4E20; return; }
    d0 = (int16_t)(d0 + 0x100);
    d1 = (int16_t)(d1 + z + (z >> 1));
    if (d1 > 0x90 || d1 < -0x90) { *x = d0; *y = 0x4E20; return; }
    *x = d0;
    *y = (int16_t)(-d1 + 0xF0);
}
