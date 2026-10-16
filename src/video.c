/* video94.asm: shared helpers used all over the game. They fade palettes, make random numbers
 * and square roots, start sounds, read up to four joysticks and wait for buttons, move and fill
 * video memory, unpack tile graphics, and print strings on the screen maps.
 *
 * Every VDP access goes through the HAL (hal.h), one call per 68k port access, in the order the
 * 68k made them. Source addresses (DMA sources, strings, graphics) are 68k addresses: ROM or
 * work RAM, read through mem_ptr(). */
#include "game.h"
#include "hal.h"
#include "rom.h"
#include "symbols_gen.h"

#define VSRAM_W 0x40000010u
#define CRAM_W  0xC0000000u
#define RAM_W(off) ram_ptr(RAM_BASE + (off))

/* the address word pair of a VDP write command for VRAM address a: high word 01aa aaaa
 * aaaa aaaa, low word 0000 0000 0000 00aa (asl.l #2 / lsr.w #2 / ori.w #$4000 / swap) */
static uint32_t vram_write_cmd(uint16_t a) { return (uint32_t)(0x4000 | (a & 0x3FFF)) << 16 | (a >> 14); }

/* video94.asm:UnpackNibbles. a0 = packed data, d0 = count: unpack count 4-bit values (high
 * nibble first) into words at nibblebuffer. */
void UnpackNibbles(uint32_t src, uint16_t count)
{
    const uint8_t *p = mem_ptr(src);
    uint8_t *out = RAM_W(RAM_OFF_nibblebuffer);
    int lo = 0;
    while (count--) {
        uint8_t b = *p;
        if (lo) p++; else b >>= 4;
        lo ^= 1;
        wr16(out, b & 0xF); out += 2;
    }
}

/* video94.asm:WeightedRandomSelect. d0 = number of word weights at nibblebuffer -> d0 = an
 * index picked with probability weight / total. The pick walks back from the last weight. */
uint16_t WeightedRandomSelect(uint16_t n)
{
    uint16_t off = RAM_OFF_nibblebuffer, sum = 0;
    while (n--) { sum += rd16(RAM_W(off)); off += 2; }
    int16_t r = (int16_t)randomd0(sum);
    do {
        off -= 2;
        r = (int16_t)(r - rd16(RAM_W(off)));
    } while (r >= 0);
    return (uint16_t)((uint16_t)(off - RAM_OFF_nibblebuffer) >> 1);
}

/* video94.asm:remap. a0 = tile words, a1 = 8-byte colour table, d0 = words, d1 = VRAM address:
 * write the tiles to VRAM with every pixel's colour index n replaced by nibble n of the table
 * (even n the high nibble of byte n/2, odd n the low one). Protected from the vblank. */
void remap(uint32_t src, uint32_t table, uint16_t words, uint16_t vram)
{
    uint16_t saved = g.disflags;
    g.disflags |= HI(dfng);
    const uint8_t *p = mem_ptr(src), *tab = mem_ptr(table);
    Vmaddr(vram);
    do {
        uint16_t in = rd16(p), out = 0;
        p += 2;
        for (int i = 0; i < 4; i++) {
            uint8_t c = tab[(in & 0xF) >> 1];
            if (!(in & 1)) c >>= 4;
            out = (uint16_t)(out | (c & 0xF));
            out = (uint16_t)(out >> 4 | out << 12);   /* ror.w #4 */
            in = (uint16_t)(in >> 4 | in << 12);
        }
        vdp_data(out);
    } while (--words != 0);
    g.disflags = saved;
}

/* video94.asm:forcefade. Run the fade to palfadenew whatever the interrupts and disflags are
 * doing: the fades-only vblank (vb2) steps it until palcount goes negative. */
void forcefade(void)
{
    uint16_t sr = cpu_get_sr();
    uint32_t vbint = g.vbint;
    uint16_t saved = g.disflags;
    g.disflags &= (uint16_t)~HI(dfng);
    g.vbint = SYM_vb2;
    cpu_set_sr(0x2500);
    while ((int16_t)g.palcount >= 0) hal_wait_vblank();
    g.disflags = saved;
    g.vbint = vbint;
    cpu_set_sr(sr);
}

/* forceblack / forceblack2: fade to black over palcount frames, keeping palfadenew */
static void fade_black(uint16_t count)
{
    uint16_t keep[0x40];
    for (int i = 0; i < 0x40; i++) { keep[i] = g.palfadenew[i]; g.palfadenew[i] = 0; }
    g.palcount = count;
    forcefade();
    for (int i = 0; i < 0x40; i++) g.palfadenew[i] = keep[i];
}

/* video94.asm:forceblack. Fade every colour to black (palcount $18), palfadenew untouched. */
void forceblack(void) { fade_black(0x18); }

/* video94.asm:forceblack2. The same with palcount $64: CopyPaletteToCRAM sets black at once. */
void forceblack2(void) { fade_black(0x64); }

/* video94.asm:cramfade. Vblank code: step colour RAM toward palfadenew. palcount $64 copies the
 * palette at once; otherwise each frame moves one component (palcount mod 3: red, green, blue)
 * of every colour one step closer, while palcount counts down from $18. */
void cramfade(void)
{
    if ((int16_t)g.palcount < 0) return;
    if (g.palcount == 0x64) { CopyPaletteToCRAM(); return; }
    if ((int16_t)--g.palcount < 0) return;
    if ((int16_t)g.palcount > 0x18) return;
    unsigned shift = (g.palcount % 3) * 4;
    uint16_t step = (uint16_t)(2 << shift), mask = (uint16_t)(0xE << shift);
    for (uint16_t a = 0; a < 0x80; a += 2) {
        vdp_ctrl((uint32_t)a << 16 | 0x20);              /* CRAM read */
        uint16_t col = vdp_read();
        int16_t cur = (int16_t)(col & mask), want = (int16_t)(g.palfadenew[a >> 1] & mask);
        if (cur == want) continue;
        cur = (int16_t)(cur + (cur < want ? step : -step));
        col = (uint16_t)((col & ~mask) | cur);
        vdp_ctrl((uint32_t)(0xC000 | a) << 16);          /* CRAM write */
        vdp_data(col);
    }
}

/* video94.asm:CopyPaletteToCRAM. Copy all 64 palfadenew colours to colour RAM, protected from
 * the vblank, and end the fade (palcount negative). */
void CopyPaletteToCRAM(void)
{
    uint16_t saved = g.disflags;
    g.disflags |= HI(dfng);
    vdp_ctrl(CRAM_W);
    for (int i = 0; i < 0x40; i += 2) vdp_data32((uint32_t)g.palfadenew[i] << 16 | g.palfadenew[i + 1]);
    ST_W(g.palcount);
    g.disflags = saved;
}

/* video94.asm:randomd0s. d0 = range -> d0 = random number with -range <= d0 < range. */
uint16_t randomd0s(uint16_t range)
{
    return (uint16_t)(randomd0((uint16_t)(range << 1)) - range);
}

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

/* video94.asm:sroot. d0.l -> d0 = its square root. Up to $640 by subtracting odd numbers, up to
 * $F00000 by Newton steps from v/256 (at most ten), above that by bisection. */
uint16_t sroot(uint32_t v)
{
    if (v == 0) return 0;
    if (v <= 0x640) {
        uint16_t d0 = (uint16_t)v, d1 = 0xFFFF;
        int borrow;
        do { d1 += 2; borrow = d1 > d0; d0 -= d1; } while (!borrow);
        return d1 >> 1;
    }
    if (v > 0xF00000) {
        uint16_t lo = 0, hi = 0xFFFF;
        for (;;) {
            uint16_t mid = (uint16_t)(((uint32_t)lo + hi) >> 1);     /* add.w then roxr.w: 17-bit sum */
            if (mid == lo) return lo;
            if (v >= (uint32_t)mid * mid) lo = mid; else hi = mid;
        }
    }
    uint16_t d1 = (uint16_t)((v >> 8) + 2);
    for (int reps = 9; reps >= 0; reps--) {
        uint16_t d2 = d1;
        d1 = (uint16_t)m68k_divu(v, d2);                 /* overflow leaves v: its low word */
        d1 = (uint16_t)((uint16_t)(d1 + d2) >> 1);
        if (d1 == d2) break;
    }
    return d1;
}

/* video94.asm:sfx. Word on the stack: start sound effect n (remembered in lastsfx), none if
 * negative. */
void sfx(int16_t n)
{
    if (n < 0) return;
    g.lastsfx = (uint16_t)n;
    play_sfx_or_music_track((uint16_t)n);
}

/* video94.asm:song. Word on the stack: start song n, none if negative. */
void song(int16_t n)
{
    if (n < 0) return;
    play_sfx_or_music_track((uint16_t)n);
}

/* the vblank interrupt advances vcount; the 68k spins on it (.0 cmp.w (vcount).w,d0 / beq.s .0) */
static void wait_vcount(void)
{
    uint16_t v = g.vcount;
    while (g.vcount == v) hal_wait_vblank();
}

/* video94.asm:waitx. d0 = frames: wait that many vblanks or until a button is pressed on any
 * pad (pads 3 and 4 in four way play). waitxpad = the buttons held. Returns the last ReadJoy
 * results (d0 = vcount when the time ran out). */
void waitx(uint16_t frames, uint16_t *d0, uint16_t *d1, uint16_t *d2, uint16_t *d3)
{
    g.waitxpad = 0;
    g.vcount = (uint16_t)-frames;
    for (;;) {
        ReadJoy1(d0, d1, d2, d3);
        g.waitxpad = *d3;
        if (*d1) return;
        ReadJoy2(d0, d1, d2, d3);
        g.waitxpad |= *d3;
        if (*d1) return;
        if (g.FourWayPlay) {
            ReadJoy3(d0, d1, d2, d3);
            g.waitxpad |= *d3;
            if (*d1) return;
            ReadJoy4(d0, d1, d2, d3);
            g.waitxpad |= *d3;
            if (*d1) return;
        }
        *d0 = g.vcount;
        wait_vcount();
        if ((int16_t)*d0 >= 0) return;
    }
}

/* video94.asm:waitxsr. waitx for the zamboni crossing: d0 = frames. Each frame moves the zamboni,
 * runs the crowd and arena animation and redraws; a button opens the menu (HandleMenuInput),
 * which either ends the wait (d1 bit 7 set) or resumes it with at least 120 frames left.
 * Returns d1. */
uint16_t waitxsr(uint16_t frames)
{
    int16_t keep_elapsed = elapsed;
    uint16_t d0, d1, d2, d3;
    g.vcount = (uint16_t)-frames;
    for (;;) {
        if (g.zamx <= 0x708) g.zamx++;
        elapsed = 1;
        updatecrowdf();
        RunArenaAnim();
        g.pausepad = 0;
        g.sflags &= (uint16_t)~HI(1);
        ReadJoy1(&d0, &d1, &d2, &d3);
        ProcessInputWithRepeat(&d1, d2, d3);
        int key = d1 != 0;
        if (!key) {
            g.pausepad = 0;
            g.sflags |= HI(1);
            ReadJoy2(&d0, &d1, &d2, &d3);
            ProcessInputWithRepeat(&d1, d2, d3);
            key = d1 != 0;
        }
        if (!key && g.FourWayPlay) {
            g.sflags &= (uint16_t)~HI(1);
            g.pausepad = 3;
            ReadJoy3(&d0, &d1, &d2, &d3);
            ProcessInputWithRepeat(&d1, d2, d3);
            key = d1 != 0;
            if (!key && g.FourWayPlay) {
                g.sflags |= HI(1);
                g.pausepad = 4;
                ReadJoy4(&d0, &d1, &d2, &d3);
                ProcessInputWithRepeat(&d1, d2, d3);
                key = d1 != 0;
            }
        }
        if (key) {
            int16_t left = (int16_t)g.vcount;
            if (left >= -0x78) left = -0x78;
            if (HandleMenuInput(&d1)) { d1 |= 0x80; break; }
            g.vcount = (uint16_t)left;
        }
        setvideo();
        d0 = g.vcount;
        wait_vcount();
        if ((int16_t)d0 >= 0) break;
    }
    elapsed = keep_elapsed;
    return d1;
}

/* video94.asm:waitjoy. Wait a frame at a time until a button is pressed on any pad -> d1 = the
 * new presses. */
uint16_t waitjoy(void)
{
    uint16_t d1;
    do {
        wait_vcount();
        d1 = orjoy();
    } while (d1 == 0);
    return d1;
}

/* video94.asm:orjoy. -> d1 = the new presses of pads 1 and 2 (all four in four way play). */
uint16_t orjoy(void)
{
    uint16_t d0, d1, d2, d3, any;
    if (!g.FourWayPlay) {
        ReadJoy1(&d0, &any, &d2, &d3);
        ReadJoy2(&d0, &d1, &d2, &d3);
        return d1 | any;
    }
    uint16_t p1, p2, p3;
    ReadJoy1(&d0, &p1, &d2, &d3);
    ReadJoy2(&d0, &p2, &d2, &d3);
    ReadJoy3(&d0, &p3, &d2, &d3);
    ReadJoy4(&d0, &d1, &d2, &d3);
    return d1 | p3 | p2 | p1;
}

/* video94.asm:nodiag. d1 = new presses, d3 = held: when a diagonal is held, drop the new
 * direction presses (d1 bits 0-3). */
void nodiag(uint16_t *d1, uint16_t d3)
{
    uint16_t dir = d3 & 0xF;
    if (dir == 0) return;
    for (int b = 3; b >= 0; b--) if (dir == (1u << b)) return;
    *d1 &= 0xFFF0;
}

/* video94.asm:ProcessInputWithRepeat. nodiag, then key repeat: holding the buttons d3 unchanged
 * (d2 = 0) for 16 frames, then every 5th frame, presses them again (d1 = d3). */
void ProcessInputWithRepeat(uint16_t *d1, uint16_t d2, uint16_t d3)
{
    nodiag(d1, d3);
    if (d3 == 0) return;
    if (d2 != 0) { g.repeatdelayframes = 0xF; return; }
    if ((int16_t)--g.repeatdelayframes >= 0) return;
    g.repeatdelayframes = 4;
    *d1 = d3;
}

/* video94.asm:ReadJoy. d0 = pad byte (active low) -> d0 = direction 0-8 (jdtab) in bits 0-3 and
 * the buttons in bits 4-7, d1 = the buttons down (all 8 bits). */
void ReadJoy(uint8_t pad, uint16_t *d0, uint16_t *d1)
{
    uint8_t down = (uint8_t)~pad;
    *d0 = (uint16_t)((down & 0xF0) | rom_ptr(SYM_jdtab)[down & 0xF]);
    *d1 = down;
}

/* ReadJoy1-4: read pad n -> d0 = direction and buttons, d1 = new presses, d2 = changed
 * buttons, d3 = held buttons; ljn keeps the held buttons for the next read */
static void read_pad(uint8_t pad, uint16_t *last, uint16_t *d0, uint16_t *d1, uint16_t *d2, uint16_t *d3)
{
    uint16_t held;
    ReadJoy(pad, d0, &held);
    *d2 = (uint16_t)(*last ^ held);
    *last = held;
    *d3 = held;
    *d1 = held & *d2;
}

/* video94.asm:ReadJoy1. Pad 1 (pad4way1). */
void ReadJoy1(uint16_t *d0, uint16_t *d1, uint16_t *d2, uint16_t *d3)
{
    uint16_t last = g.lj1; read_pad(g.pad4way1, &last, d0, d1, d2, d3); g.lj1 = last;
}
/* video94.asm:ReadJoy2. Pad 2 (pad4way2). */
void ReadJoy2(uint16_t *d0, uint16_t *d1, uint16_t *d2, uint16_t *d3)
{
    uint16_t last = g.lj2; read_pad(g.pad4way2, &last, d0, d1, d2, d3); g.lj2 = last;
}
/* video94.asm:ReadJoy3. Four way play pad 3 (pad4way3). */
void ReadJoy3(uint16_t *d0, uint16_t *d1, uint16_t *d2, uint16_t *d3)
{
    uint16_t last = g.lj3; read_pad(g.pad4way3, &last, d0, d1, d2, d3); g.lj3 = last;
}
/* video94.asm:ReadJoy4. Four way play pad 4 (pad4way4). */
void ReadJoy4(uint16_t *d0, uint16_t *d1, uint16_t *d2, uint16_t *d3)
{
    uint16_t last = g.lj4; read_pad(g.pad4way4, &last, d0, d1, d2, d3); g.lj4 = last;
}

/* video94.asm:DoDMApro. DoDMA protected from the vblank (dfng). */
void DoDMApro(uint32_t src, uint16_t words, uint16_t vram)
{
    uint16_t saved = g.disflags;
    g.disflags |= HI(dfng);
    DoDMA(src, words, vram);
    g.disflags = saved;
}

/* DoDMA's .nd: one DMA transfer from 68k memory, with the Z80 off the bus. A transfer from
 * RAM writes its first word again by hand afterwards (the DMA bug the comment mentions). */
static void dma_68k(uint32_t src, uint16_t words, uint16_t vram)
{
    vdp_ctrl16(SYM_VDPR_MODE2 + 0x54);
    vdp_ctrl16(SYM_VDPR_AUTOINC + 0x02);
    vdp_ctrl16((uint16_t)(0x9300 | (words & 0xFF)));
    vdp_ctrl16((uint16_t)(0x9400 | (words >> 8)));
    uint32_t s = src >> 1;
    vdp_ctrl16((uint16_t)(0x9500 | (s & 0xFF)));
    vdp_ctrl16((uint16_t)(0x9600 | ((s >> 8) & 0xFF)));
    vdp_ctrl16((uint16_t)(SYM_VDPR_DMASRCH | ((s >> 16) & 0x7F)));
    uint32_t split = (uint32_t)(vram >> 14) << 16 | (vram & 0x3FFF);
    g.dmaram = split | 0x804000;
    uint16_t sr = cpu_get_sr();
    cpu_set_sr(0x2700);
    z80_busreq(0x100);
    while (z80_busack() & 1) {}
    vdp_ctrl16((uint16_t)g.dmaram);
    vdp_ctrl16((uint16_t)(g.dmaram >> 16));
    WaitDMA();
    vdp_ctrl16(SYM_VDPR_MODE2 + 0x64);
    z80_busreq(0);
    if ((int32_t)src < 0) {
        vdp_ctrl(vram_write_cmd(vram));
        vdp_data(rd16(mem_ptr(src)));
    }
    cpu_set_sr(sr);
}

/* video94.asm:DoDMA. a0 = source, d0 = words, d1 = VRAM address: DMA to VRAM. A source that
 * crosses a 64K boundary goes as two transfers. */
void DoDMA(uint32_t src, uint16_t words, uint16_t vram)
{
    uint32_t end = (uint32_t)(uint16_t)(words * 2) + (uint16_t)src;
    if (end > 0xFFFF && (uint16_t)end != 0) {
        uint16_t over = (uint16_t)((uint16_t)end >> 1);
        uint16_t first = (uint16_t)(words - over);
        dma_68k(src, first, vram);
        uint16_t bytes = (uint16_t)(first * 2);
        vram = (uint16_t)(vram + bytes);
        src += (uint32_t)(int32_t)(int16_t)bytes;
        words = over;
    }
    dma_68k(src, words, vram);
}

/* video94.asm:DoDMA_nd2. d0 = words, d1 = VRAM destination, d2 = VRAM source: VRAM to VRAM
 * copy by DMA, protected from the vblank. */
void DoDMA_nd2(uint16_t words, uint16_t dst, uint16_t src)
{
    uint16_t saved = g.disflags;
    g.disflags |= HI(dfng);
    vdp_ctrl16(SYM_VDPR_MODE2 + 0x54);
    vdp_ctrl16(SYM_VDPR_AUTOINC + 0x01);
    vdp_ctrl16((uint16_t)(0x9300 | (words & 0xFF)));
    vdp_ctrl16((uint16_t)(0x9400 | (words >> 8)));
    vdp_ctrl16((uint16_t)(0x9500 | (src & 0xFF)));
    vdp_ctrl16((uint16_t)(0x9600 | (src >> 8)));
    vdp_ctrl16(SYM_VDPR_DMASRCH + 0xC0);
    g.dmaram = (uint32_t)(dst & 0x3FFF) << 16 | (dst >> 14) | 0xC0;
    z80_busreq(0x100);
    while (z80_busack() & 1) {}
    vdp_ctrl16((uint16_t)(g.dmaram >> 16));
    vdp_ctrl16((uint16_t)g.dmaram);
    WaitDMA();
    vdp_ctrl16(SYM_VDPR_MODE2 + 0x64);
    vdp_ctrl16(SYM_VDPR_AUTOINC + 0x02);
    z80_busreq(0);
    g.disflags = saved;
}

/* video94.asm:DoFill. d0 = words, d1 = VRAM address, d2 = word: fill VRAM through the data
 * port, two words a pass (through dmaram). */
void DoFill(uint16_t words, uint16_t vram, uint16_t tile)
{
    vdp_ctrl(vram_write_cmd(vram));
    uint32_t pair = (uint32_t)tile << 16 | tile;
    uint16_t n = (uint16_t)((words >> 1) - 1);
    do {
        g.dmaram = pair;
        vdp_data((uint16_t)(g.dmaram >> 16));
        vdp_data((uint16_t)g.dmaram);
    } while (n-- != 0);
}

/* video94.asm:WaitDMA. Spin while the VDP status says a DMA is running. */
void WaitDMA(void)
{
    while (vdp_status() & 2) {}
}

/* video94.asm:setVram. d0 = colour: fade every colour to it, then setVram_0. */
void setVram(uint16_t colour)
{
    for (int i = 0; i < 0x40; i++) g.palfadenew[i] = colour;
    g.palcount = 0x18;
    forcefade();
    setVram_0();
}

/* video94.asm:setVram_0. Clear VRAM and set the VDP registers from disflags (32 / 40 columns),
 * Map1col1 (64 or 128 wide), VmMap1-3, VSPRITES and VSCRLPM; no window, whole-screen scroll. */
void setVram_0(void)
{
    uint16_t saved = g.disflags;
    g.disflags |= HI(dfng);
    vdp_ctrl16(SYM_VDPR_AUTOINC + 0x02);
    Vmaddr(0);
    for (int i = 0; i < 0x4000; i++) vdp_data32(0);
    uint16_t mode4 = 0x8C00;                          /* 8 for shadow mode */
    if (!(g.disflags & HI(df32c))) mode4 |= 0x81;
    vdp_ctrl16(mode4);                                 /* 40 column mode, no interlace */
    vdp_ctrl16(SYM_VDPR_MODE1 + 0x04);
    vdp_ctrl16(SYM_VDPR_MODE2 + 0x64);
    vdp_ctrl16(g.Map1col1 == 6 ? 0x9001 : 0x9003);     /* playfield 64x32 or 128x32 */
    vdp_ctrl16((uint16_t)(0x8200 | ((g.VmMap1 >> 8 >> 2) & 0x38)));
    vdp_ctrl16((uint16_t)(0x8400 | (g.VmMap2 >> 8 >> 5)));
    vdp_ctrl16((uint16_t)(0x8300 | ((g.VmMap3 >> 8 >> 2) & 0x3E)));
    vdp_ctrl16((uint16_t)(0x8500 | (g.VSPRITES >> 8 >> 1)));
    vdp_ctrl16((uint16_t)(0x8D00 | (g.VSCRLPM >> 8 >> 2)));
    vdp_ctrl16(SYM_VDPR_WINX);                         /* no window plane */
    vdp_ctrl16(SYM_VDPR_WINY);
    vdp_ctrl16(SYM_VDPR_BGCOL);                        /* colour 0 is the border */
    vdp_ctrl16(SYM_VDPR_MODE3);                        /* whole-screen scroll */
    vdp_ctrl(VSRAM_W);
    vdp_data32(0);
    g.disflags = saved;
}

/* video94.asm:Vmaddr. d0 = VRAM address: set the VDP to write there (a0 = the data port). */
void Vmaddr(uint16_t vram)
{
    vdp_ctrl(vram_write_cmd(vram));
}

/* ---- tile graphics ---------------------------------------------------------------------- */

/* video94.asm:dobitmap. a0 = palettes, a1 = map, a2 = tiles, d0/d1 = start x/y in the map,
 * d2/d3 = width/height, d4 = first char, d5 = which of the four palettes to load (bits 0-3).
 * Copy the chosen palettes to palfadenew, the map rectangle to the screen at printx/printy
 * (char + d4, palette bits of printa), then the tiles unless sflags6 bit 0. -> d4 = next char. */
uint16_t dobitmap(uint32_t pal, uint32_t map, uint32_t tiles, uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t start, uint16_t palbits)
{
    uint16_t keep_y = g.printy, saved = g.disflags;
    g.disflags |= HI(dfng);
    uint8_t *dst = RAM_W(RAM_OFF_palfadenew);
    for (; palbits; palbits >>= 1, pal += 0x20, dst += 0x20)
        if (palbits & 1) for (int i = 0x1F; i >= 0; i--) dst[i] = mem_ptr(pal)[i];
    uint16_t attr = g.printa & 0xF800;
    const uint8_t *m = mem_ptr(map);
    uint16_t row = y, rows = h;
    do {
        xyVmMap();
        uint16_t off = (uint16_t)(((uint16_t)(row * rd16(m)) + x) << 1);
        uint16_t cols = w;
        do {
            vdp_data((uint16_t)((rd16(m + 4 + (int16_t)off) + start) ^ attr));
            off += 2;
        } while (--cols != 0);
        g.printy++;
        row++;
    } while (--rows != 0);
    if (!(g.sflags6 & HI(0))) start = DecompressGraphicsPlain(tiles, start);
    g.disflags = saved;
    g.printy = keep_y;
    return start;
}

/* video94.asm:DecompressGraphicsWithCallback. DecompressGraphics with the 8-byte colour table
 * that follows the call (cb, its 68k address) as the remap table. */
uint16_t DecompressGraphicsWithCallback(uint32_t tiles, uint16_t start, uint32_t cb)
{
    g.callbackPtr = cb;
    return DecompressGraphics(tiles, start);
}

/* video94.asm:DecompressGraphicsPlain. DecompressGraphics without a remap table. */
uint16_t DecompressGraphicsPlain(uint32_t tiles, uint16_t start)
{
    g.callbackPtr = 0;
    return DecompressGraphics(tiles, start);
}

/* video94.asm:DecompressGraphics. a2 = graphics (a word count of tiles, bit 15 = packed, then
 * the data), d4 = first char: load them at char d4 (DMA, remap through callbackPtr, or
 * DecompressBytecode) -> d4 = next char. */
uint16_t DecompressGraphics(uint32_t tiles, uint16_t start)
{
    uint16_t vram = (uint16_t)(start << 5);
    uint16_t n = rd16(mem_ptr(tiles));
    uint32_t data = tiles + 2;
    if (n == 0) return start;
    if (n & 0x8000) {
        n &= 0x7FFF;
        start += n;
        DecompressBytecode(data, vram);
        return start;
    }
    start += n;
    uint16_t words = (uint16_t)(n << 4);
    if (g.callbackPtr == 0) DoDMApro(data, words, vram);
    else remap(data, g.callbackPtr, words, vram);
    return start;
}

/* the 256-byte ring buffer at ThreeStars that DecompressBytecode unpacks into */
#define RING (RAM_OFF_ThreeStars)
static uint8_t *ring(uint8_t i) { return RAM_W(RING + i); }

/* video94.asm:DecompressBytecode. a0 = packed tiles, d1 = VRAM address: run the byte code
 * (one handler per high nibble, jump_table) into the ring buffer, which goes to VRAM each time
 * it fills and at the end code. */
void DecompressBytecode(uint32_t src, uint16_t vram)
{
    static int (*const jump_table[16])(struct Unpack *) = {
        Opcode_CopyLiteral, Opcode_CopyLiteral, Opcode_ClearBytes, Opcode_Fillbytes,
        Opcode_CopyBackwardShort, Opcode_CopyBackwardShort, Opcode_CopyBackwardShort, Opcode_CopyBackwardShort,
        Opcode_CopyBackwardMedium, Opcode_CopyBackwardLong, Opcode_CopyBackwardExtended1, Opcode_CopyBackwardExtended2,
        Opcode_CopyBackwardReverseShort, Opcode_CopyBackwardReverseShort, Opcode_CopyBackwardReverseMedium, Opcode_CopyBackwardReverseLong,
    };
    struct Unpack u = { src, 0, vram };
    for (;;) {
        uint8_t op = *mem_ptr(u.src++);
        if (jump_table[op >> 4](&u)) return;
    }
}

/* put one byte in the ring buffer; a full buffer goes to VRAM */
static void ring_put(struct Unpack *u, uint8_t b)
{
    *ring(u->idx) = b;
    if (++u->idx == 0) FlushOutputBuffer(u);
}
static uint8_t op_byte(const struct Unpack *u) { return mem_ptr(u->src)[-1]; }   /* -1(a0) */
static uint8_t next_byte(struct Unpack *u) { return *mem_ptr(u->src++); }        /* (a0)+ */

/* video94.asm:Opcode_CopyLiteral. Opcodes 0-1: copy (low 5 bits)+1 bytes from the data. */
int Opcode_CopyLiteral(struct Unpack *u)
{
    for (int n = (op_byte(u) & 0x1F) + 1; n; n--) ring_put(u, next_byte(u));
    return 0;
}

/* video94.asm:Opcode_ClearBytes. Opcode 2: (low 4 bits)+1 zero bytes. */
int Opcode_ClearBytes(struct Unpack *u)
{
    for (int n = (op_byte(u) & 0xF) + 1; n; n--) ring_put(u, 0);
    return 0;
}

/* video94.asm:Opcode_Fillbytes. Opcode 3: the next byte (low 4 bits)+3 times. */
int Opcode_Fillbytes(struct Unpack *u)
{
    int n = (op_byte(u) & 0xF) + 3;
    uint8_t b = next_byte(u);
    for (; n; n--) ring_put(u, b);
    return 0;
}

/* video94.asm:CopyBackwardRun. Copy count+1 bytes from dist back in the ring buffer, forward. */
int CopyBackwardRun(struct Unpack *u, uint16_t count, uint8_t dist)
{
    uint8_t from = (uint8_t)(u->idx - dist);
    do ring_put(u, *ring(from++)); while (count-- != 0);
    return 0;
}

/* video94.asm:CopyBackwardReverseRun. The same, walking the source backwards. */
int CopyBackwardReverseRun(struct Unpack *u, uint16_t count, uint8_t dist)
{
    uint8_t from = (uint8_t)(u->idx - dist);
    do ring_put(u, *ring(from--)); while (count-- != 0);
    return 0;
}

/* video94.asm:Opcode_CopyBackwardShort. Opcodes 4-7: count bits 0-2, distance bits 3-5. */
int Opcode_CopyBackwardShort(struct Unpack *u)
{
    uint8_t op = op_byte(u);
    return CopyBackwardRun(u, (uint16_t)((op & 7) + 1), (uint8_t)(((op >> 3) & 7) + 1));
}

/* video94.asm:Opcode_CopyBackwardMedium. Opcode 8: count low 4 bits, distance the next byte. */
int Opcode_CopyBackwardMedium(struct Unpack *u)
{
    uint16_t count = (uint16_t)((op_byte(u) & 0xF) + 2);
    return CopyBackwardRun(u, count, next_byte(u));
}

/* opcodes 9 and F: count = low 4 bits and the top bit of the next byte (asl.b / roxl.b),
 * distance = the next byte's low 7 bits + 1 */
static uint16_t long_count(const struct Unpack *u)
{
    uint8_t next = *mem_ptr(u->src);
    return (uint16_t)((((op_byte(u) << 1) | (next >> 7)) & 0x1F) + 2);
}

/* video94.asm:Opcode_CopyBackwardLong. Opcode 9. */
int Opcode_CopyBackwardLong(struct Unpack *u)
{
    uint16_t count = long_count(u);
    return CopyBackwardRun(u, count, (uint8_t)((next_byte(u) & 0x7F) + 1));
}

/* video94.asm:Opcode_CopyBackwardExtended1. Opcode A: 6-bit count, 6-bit distance. */
int Opcode_CopyBackwardExtended1(struct Unpack *u)
{
    uint16_t w = (uint16_t)(op_byte(u) << 8 | *mem_ptr(u->src));
    uint16_t count = (uint16_t)(((w >> 6) & 0x3F) + 2);
    return CopyBackwardRun(u, count, (uint8_t)((next_byte(u) & 0x3F) + 1));
}

/* video94.asm:Opcode_CopyBackwardExtended2. Opcode B: 7-bit count, 5-bit distance. */
int Opcode_CopyBackwardExtended2(struct Unpack *u)
{
    uint16_t w = (uint16_t)(op_byte(u) << 8 | *mem_ptr(u->src));
    uint16_t count = (uint16_t)(((w >> 5) & 0x7F) + 2);
    return CopyBackwardRun(u, count, (uint8_t)((next_byte(u) & 0x1F) + 1));
}

/* video94.asm:Opcode_CopyBackwardReverseShort. Opcodes C-D: count bits 0-1, distance bits 2-4. */
int Opcode_CopyBackwardReverseShort(struct Unpack *u)
{
    uint8_t op = op_byte(u);
    return CopyBackwardReverseRun(u, (uint16_t)((op & 3) + 1), (uint8_t)(((op >> 2) & 7) + 1));
}

/* video94.asm:Opcode_CopyBackwardReverseMedium. Opcode E: count low 4 bits, distance the next
 * byte. Distance 0 is the end code: send what is left in the buffer and stop (1). */
int Opcode_CopyBackwardReverseMedium(struct Unpack *u)
{
    uint16_t count = (uint16_t)((op_byte(u) & 0xF) + 2);
    uint8_t dist = next_byte(u);
    if (dist) return CopyBackwardReverseRun(u, count, dist);
    if (u->idx) FlushOutputBuffer(u);
    return 1;
}

/* video94.asm:Opcode_CopyBackwardReverseLong. Opcode F. */
int Opcode_CopyBackwardReverseLong(struct Unpack *u)
{
    uint16_t count = long_count(u);
    return CopyBackwardReverseRun(u, count, (uint8_t)((next_byte(u) & 0x7F) + 1));
}

/* video94.asm:FlushOutputBuffer. Send the ring buffer (all 256 bytes when it wrapped, else the
 * idx bytes in it) to the VRAM address d3, through remap when callbackPtr is set, and advance d3. */
void FlushOutputBuffer(struct Unpack *u)
{
    uint16_t words = (uint16_t)((u->idx ? u->idx : 0x100) >> 1);
    uint16_t vram = u->vram;
    u->vram = (uint16_t)(u->vram + 2 * words);
    if (g.callbackPtr) remap(RAM_BASE + RING, g.callbackPtr, words, vram);
    else DoDMApro(RAM_BASE + RING, words, vram);
}

/* ---- printing --------------------------------------------------------------------------- */

/* the map selected by printm: VmMap1/2/3 (VRAM base) and the shift for a row */
static const uint8_t *print_map(void) { return RAM_W(RAM_OFF_VmMap1 + (int16_t)g.printm); }
/* asl.w Dn: the count is taken mod 64 and 16 or more clears the word */
static uint16_t asl_w(uint16_t v, uint16_t n) { n &= 63; return n >= 16 ? 0 : (uint16_t)(v << n); }

/* video94.asm:xyVmMap. Set the VDP to write at printx/printy on the map printm. */
void xyVmMap(void)
{
    const uint8_t *m = print_map();
    uint16_t row = asl_w(g.printy, rd16(m + 2));
    Vmaddr((uint16_t)(((uint16_t)(g.printx + row) << 1) + rd16(m)));
}

/* video94.asm:eraser. d0/d1 = width/height, d2 = map word: fill the rectangle at
 * printx/printy (printy advances, wrapping at 32 rows). */
void eraser(uint16_t w, uint16_t h, uint16_t attr)
{
    uint16_t saved = g.disflags;
    g.disflags |= HI(dfng);
    do {
        xyVmMap();
        uint16_t n = w;
        do vdp_data(attr); while (--n != 0);
        g.printy = (uint16_t)((g.printy + 1) & 0x1F);
    } while (--h != 0);
    g.disflags = saved;
}

/* Framer's .tbline: one row of the frame from the three-entry map row at framer word index
 * col: left, w-2 middles, right */
static void frame_row(const uint8_t *map, uint16_t *col, uint16_t base, uint16_t w)
{
    xyVmMap();
    g.printy++;
    vdp_data((uint16_t)(rd16(map + *col) + base));
    *col += 2;
    uint16_t n = (uint16_t)(w - 3);
    do vdp_data((uint16_t)(rd16(map + *col) + base)); while (n-- != 0);
    *col += 2;
    vdp_data((uint16_t)(rd16(map + *col) + base));
    *col += 2;
}

/* video94.asm:Framer. d0/d1 = width/height: draw a frame filled in (framer.map, its tiles at
 * framercset) with the top left corner at printx/printy. */
void Framer(uint16_t w, uint16_t h)
{
    uint16_t saved = g.disflags;
    g.disflags |= HI(dfng);
    uint16_t base = (uint16_t)(g.printa + g.framercset);
    const uint8_t *map = rom_ptr(SYM_FramerMap);
    map += rd32(map + 4) + 4;
    uint16_t col = 0;
    frame_row(map, &col, base, w);
    int16_t rows = (int16_t)(h - 3);
    do {
        frame_row(map, &col, base, w);
        col -= 6;
    } while (--rows >= 0);
    col += 6;
    frame_row(map, &col, base, w);
    g.disflags = saved;
}

/* the font map a printed character comes from: setup font when sflags6 bit 3 */
static uint16_t font_char(uint8_t c)
{
    const uint8_t *map = rom_ptr(g.sflags6 & HI(3) ? SYM_PrintFont2Map : SYM_SmallFontMap);
    map += rd32(map + 4);
    return rd16(map + 4 + 2 * c);
}

/* video94.asm:printsmall. Print a String with control codes (negative bytes,
 * ControlCodeJumpTable) at printx/printy/printm with printa; the char set is printfontset
 * (smallfontchars, or setupfontchars when sflags6 bit 3). -> a1 = past the string. */
static const uint8_t *printsmall_p(const uint8_t *s)
{
    static void (*const codes[9])(struct PrintState *) = {
        NULL, ControlCode_SetMap, ControlCode_SetAttribute, ControlCode_SetX, ControlCode_SetY,
        ControlCode_AddX, ControlCode_AddY, ControlCode_SetFont, ControlCode_SetMapAndPosition,
    };
    uint16_t saved = g.disflags;
    g.disflags |= HI(dfng);
    uint16_t chars = g.sflags6 & HI(3) ? RAM_OFF_setupfontchars : RAM_OFF_smallfontchars;
    xyVmMap();
    struct PrintState st = { s + 2, g.printa, (uint16_t)(rd16(s) - 2) };
    while (st.count-- != 0) {
        int8_t c = (int8_t)*st.p++;
        if (c <= 0) {
            if (codes[-c]) codes[-c](&st);
            continue;
        }
        uint16_t tile;
        if (c == 0x40) tile = 0x7FF;
        else if (c == 0x5E) { g.printx++; xyVmMap(); continue; }
        else tile = (uint16_t)(font_char((uint8_t)c) + rd16(RAM_W(chars + g.printfontset)));
        vdp_data((uint16_t)(tile + st.attr));
        g.printx++;
    }
    g.disflags = saved;
    return st.p;
}

uint32_t printsmall(uint32_t str) { return mem_addr(printsmall_p(mem_ptr(str))); }

/* video94.asm:printsmallz. printsmall of the String that follows the call. */
void printsmallz(const uint8_t *s, unsigned n) { (void)n; printsmall_p(s); }

/* video94.asm:ControlCode_SetMapAndPosition. Code -8: attribute, map, x, y. */
void ControlCode_SetMapAndPosition(struct PrintState *st)
{
    ControlCode_SetAttribute(st);
    ControlCode_SetMap(st);
    ControlCode_SetX(st);
    ControlCode_SetY(st);
}

/* video94.asm:ControlCode_SetMap. Code -1: the next byte is the map (1-3) -> printm. */
void ControlCode_SetMap(struct PrintState *st)
{
    uint8_t m = *st->p++;
    st->count--;
    g.printm = (uint16_t)(((m & 3) << 2) - 4);
    xyVmMap();
}

/* video94.asm:ControlCode_SetAttribute. Code -2: the next byte is palette / priority -> printa. */
void ControlCode_SetAttribute(struct PrintState *st)
{
    st->attr = (uint16_t)((*st->p++ & 7) << 13);
    st->count--;
    g.printa = st->attr;
}

/* video94.asm:ControlCode_SetX. Code -3: printx = the next byte. */
void ControlCode_SetX(struct PrintState *st) { g.printx = *st->p++; st->count--; xyVmMap(); }
/* video94.asm:ControlCode_SetY. Code -4: printy = the next byte. */
void ControlCode_SetY(struct PrintState *st) { g.printy = *st->p++; st->count--; xyVmMap(); }
/* video94.asm:ControlCode_AddX. Code -5: printx += the next (signed) byte. */
void ControlCode_AddX(struct PrintState *st) { g.printx = (uint16_t)(g.printx + (int8_t)*st->p++); st->count--; xyVmMap(); }
/* video94.asm:ControlCode_AddY. Code -6: printy += the next (signed) byte. */
void ControlCode_AddY(struct PrintState *st) { g.printy = (uint16_t)(g.printy + (int8_t)*st->p++); st->count--; xyVmMap(); }
/* video94.asm:ControlCode_SetFont. Code -7: printfontset = the next byte (char set index). */
void ControlCode_SetFont(struct PrintState *st) { g.printfontset = (uint16_t)(*st->p++ << 1); st->count--; }

/* video94.asm:print. Print a String at printx/printy/printm with printa. A negative byte -$ab
 * starts a new position: map a, palette / priority b, then x and y bytes; $40 is a blank tile,
 * $5E skips a column, 0 is padding. -> a1 = past the string. */
static const uint8_t *print_p(const uint8_t *s)
{
    uint16_t saved = g.disflags;
    g.disflags |= HI(dfng);
    xyVmMap();
    uint16_t attr = g.printa;
    uint16_t n = (uint16_t)(rd16(s) - 2);
    s += 2;
    while (n-- != 0) {
        int8_t c = (int8_t)*s++;
        if (c == 0) continue;
        if (c < 0) {
            uint16_t code = (uint16_t)-c;
            attr = (uint16_t)((code << 9) & 0xF800);
            g.printa = attr;
            g.printm = (uint16_t)(((code & 3) << 2) - 4);
            g.printx = (uint16_t)(int8_t)*s++;
            g.printy = (uint16_t)(int8_t)*s++;
            xyVmMap();
            n -= 2;
            continue;
        }
        uint16_t tile;
        if (c == 0x40) tile = 0x7FF;
        else if (c == 0x5E) { g.printx++; xyVmMap(); continue; }
        else tile = (uint16_t)(font_char((uint8_t)c) + (g.sflags6 & HI(3) ? g.setupfontchars : g.smallfontchars));
        vdp_data((uint16_t)(tile + attr));
        g.printx++;
    }
    g.disflags = saved;
    return s;
}

uint32_t print(uint32_t str) { return mem_addr(print_p(mem_ptr(str))); }

/* video94.asm:printz. print the String that follows the call. */
void printz(const uint8_t *s, unsigned n) { (void)n; print_p(s); }

/* video94.asm:FormatAndPrintTime. d0 = period (bits 14-15) and seconds (bits 0-13): print the
 * period label (" 1", " 2", " 3", "OT"), two columns on, the time as m:ss. */
void FormatAndPrintTime(uint16_t v)
{
    PrintStringFromList(SYM_PeriodLabelTable, (uint16_t)(v >> 14));
    g.printx += 2;
    print(PushTime(v & 0x3FFF));
}

/* the String length word and padding that end PushTime / PushNumber: the text runs from p to
 * end; a pad byte keeps the String even. -> its address */
static uint32_t push_string(uint8_t *p, const uint8_t *end)
{
    uint16_t len = (uint16_t)(end - p + 2);
    if (len & 1) { *--p = 0; len++; }
    p -= 2;
    wr16(p, len);
    return ram_addr(p);
}

/* video94.asm:PushTime. d0 = seconds -> a1 = a String " m:ss" (or "mm:ss") in mesarea. */
uint32_t PushTime(uint16_t t)
{
    uint8_t *end = RAM_W(RAM_OFF_mesarea + 30), *p = end;
    uint32_t d0 = m68k_divu((uint32_t)(int32_t)(int16_t)t, 10);
    *--p = (uint8_t)((d0 >> 16) + '0');
    d0 = m68k_divu((uint32_t)(int32_t)(int16_t)d0, 6);
    *--p = (uint8_t)((d0 >> 16) + '0');
    *--p = ':';
    d0 = m68k_divu((uint32_t)(int32_t)(int16_t)d0, 10);
    *--p = (uint8_t)((d0 >> 16) + '0');
    *--p = ' ';
    if ((uint16_t)d0) *p = (uint8_t)(d0 + '0');
    return push_string(p, end);
}

/* video94.asm:PushNumber. d0 -> a1 = a String of it in decimal, no leading zeros. */
uint32_t PushNumber(uint16_t n)
{
    uint8_t *end = RAM_W(RAM_OFF_PushNumberBuf), *p = end;
    uint32_t d0 = n;
    do {
        d0 = m68k_divu((uint32_t)(int32_t)(int16_t)d0, 10);
        *--p = (uint8_t)((d0 >> 16) + '0');
    } while ((uint16_t)d0);
    return push_string(p, end);
}

/* video94.asm:PushNumberWidth. d0 = number, d1 = width -> a1 = a String of it right-justified
 * in width columns (spaces before the first digit) at PushWidthBuf. */
uint32_t PushNumberWidth(uint16_t n, uint16_t width)
{
    uint8_t *buf = RAM_W(RAM_OFF_PushWidthBuf), *p = buf + 2;
    uint32_t div = 1;
    for (uint16_t i = (uint16_t)(width - 1); i; i--) div = (uint16_t)div * 10u;
    uint8_t fill = ' ';
    uint32_t d0 = n;
    do {
        d0 = m68k_divu((uint32_t)(int32_t)(int16_t)d0, (uint16_t)div);
        if ((uint16_t)d0 == 0 && (uint16_t)div != 1) d0 = (d0 & 0xFFFF0000) | fill;
        else { fill = '0'; d0 = (d0 & 0xFFFF0000) | (uint16_t)(d0 + '0'); }
        *p++ = (uint8_t)d0;
        d0 = d0 >> 16 | d0 << 16;                    /* swap: the remainder goes on */
        div = m68k_divu(div, 10);                    /* divu.w #10 of the whole long */
    } while ((uint16_t)div);
    uint16_t len = (uint16_t)(p - buf);
    if (len & 1) { *p++ = 0; len++; }
    wr16(buf, len);
    return ram_addr(buf);
}

/* video94.asm:appstring. a1 = String, a3 = String in RAM: append the first to the second (the
 * text of a3 ends at its first zero byte after the first character). */
static void appstring_p(const uint8_t *src, uint32_t buf)
{
    uint8_t *dst = ram_ptr(buf), *a0 = dst + 2;
    int16_t n = (int16_t)(rd16(dst) - 3);
    if (n >= 0) {
        do a0++; while (*a0 != 0 && n-- != 0);
    }
    n = (int16_t)(rd16(src) - 3);
    src += 2;
    if (n < 0) return;
    do {
        uint8_t b = *src++;
        *a0++ = b;
        if (b == 0) a0--;
    } while (n-- != 0);
    if ((a0 - (uint8_t *)&g) & 1) *a0++ = 0;
    wr16(dst, (uint16_t)(a0 - dst));
}

void appstring(uint32_t src, uint32_t buf) { appstring_p(mem_ptr(src), buf); }

/* video94.asm:appendz. appstring of the String that follows the call onto a3. */
void appendz(const uint8_t *s, unsigned n, uint32_t buf) { (void)n; appstring_p(s, buf); }

/* video94.asm:printbig. Print a String in the big font (two rows high, chars one or two
 * columns wide) at printx/printy; a negative byte starts a new position as in print. printx /
 * printy end past the text. */
static const uint8_t *printbig_p(const uint8_t *s)
{
    uint16_t saved = g.disflags;
    g.disflags |= HI(dfng);
    struct BigPen pen = { g.printx, g.printy, (uint16_t)(g.printa + g.BigFontChars) };
    uint16_t n = (uint16_t)(rd16(s) - 2);
    s += 2;
    while (n-- != 0) {
        int8_t c = (int8_t)*s++;
        if (c == 0) continue;
        if (c < 0) {
            uint16_t code = (uint16_t)-c;
            g.printa = (uint16_t)((code << 9) & 0xF800);
            pen.attr = (uint16_t)(g.printa + g.BigFontChars);
            g.printm = (uint16_t)(((code & 3) << 2) - 4);
            g.printx = pen.x = (uint16_t)(int8_t)*s++;
            g.printy = pen.y = (uint16_t)(int8_t)*s++;
            n -= 2;
            continue;
        }
        if (c >= 'a' && c <= 'z') c -= 0x20;
        PrintBigChar((uint8_t)c, &pen);
    }
    g.printx = pen.x;
    g.printy = pen.y;
    g.disflags = saved;
    return s;
}

void printbig(uint32_t str) { printbig_p(mem_ptr(str)); }

/* video94.asm:printbigz. printbig of the String that follows the call. */
void printbigz(const uint8_t *s, unsigned n) { (void)n; printbig_p(s); }

/* video94.asm:PrintBigChar. d0 = ASCII: put its big font tiles (bfasciicon: map column, negative
 * for a one-column char) at d4/d5, top and bottom, and advance d4. */
void PrintBigChar(uint8_t c, struct BigPen *pen)
{
    int8_t col = (int8_t)rom_ptr(SYM_bfasciicon)[(int16_t)(c - 0x20)];
    int cols = 2;
    if (col < 0) { col = (int8_t)-col; cols = 1; }
    uint16_t off = (uint16_t)(col << 1);
    const uint8_t *map = rom_ptr(SYM_BigFontMap);
    map += rd32(map + 4);
    while (cols--) {
        PutBigTile(rd16(map + 4 + off), pen);
        uint16_t row = (uint16_t)(rd16(map) << 1);
        pen->y++;
        PutBigTile(rd16(map + 4 + (uint16_t)(off + row)), pen);
        pen->y--;
        pen->x++;
        off += 2;
    }
}

/* video94.asm:PutBigTile. d3 = tile: write it plus the pen attribute at d4/d5 on printm. */
void PutBigTile(uint16_t tile, const struct BigPen *pen)
{
    const uint8_t *m = print_map();
    uint16_t row = asl_w(pen->y, rd16(m + 2));
    Vmaddr((uint16_t)(((uint16_t)(row + pen->x) << 1) + rd16(m)));
    vdp_data((uint16_t)(tile + pen->attr));
}

/* video94.asm:AddSmallFont. d4 = first char: load the small font tiles there -> d4 = next. */
uint16_t AddSmallFont(uint16_t start)
{
    g.smallfontchars = start;
    return DecompressGraphicsPlain(SYM_SmallFontMap + 8, start);
}

/* video94.asm:AddFramer. d4 = first char: load the framer tiles there -> d4 = next. */
uint16_t AddFramer(uint16_t start)
{
    g.framercset = start;
    return DecompressGraphicsPlain(SYM_FramerMap + 8, start);
}

/* video94.asm:AddTeamBlock. Load the team block tiles at char 2 -> d4 = next char. */
uint16_t AddTeamBlock(void)
{
    return DecompressGraphicsPlain(SYM_Teamblocksmap + 8, 2);
}
