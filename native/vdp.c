/* The VDP's ports and DMA, after Genesis Plus GX's vdp_ctrl.c (mode 5 only: the game never uses
 * the Master System mode). Timing is not modelled: DMA completes at once and the status never
 * reports it busy, because the C game waits for the vblank cooperatively. */
#include <string.h>
#include "vdp.h"

struct Vdp vdp;
uint16_t (*vdp_bus_read16)(uint32_t addr);

void vdp_reset(void)
{
    memset(&vdp, 0, sizeof vdp);
}

/* a word written through the data port (or by DMA) to VRAM, CRAM or VSRAM at addr */
static void bus_write(uint16_t data)
{
    vdp.fifo = data;
    switch (vdp.code & 0x0F) {
    case 0x01: {                                          /* VRAM */
        uint16_t i = vdp.addr & 0xFFFE;
        if (vdp.addr & 1) data = (uint16_t)(data >> 8 | data << 8);
        vdp.vram[i] = (uint8_t)(data >> 8);
        vdp.vram[i + 1] = (uint8_t)data;
        break;
    }
    case 0x03:                                            /* CRAM */
        vdp.cram[(vdp.addr >> 1) & 0x3F] = data & 0xEEE;
        break;
    case 0x05:                                            /* VSRAM: 40 words */
        if ((vdp.addr & 0x7E) < 0x50) vdp.vsram[(vdp.addr & 0x7E) >> 1] = data & 0x7FF;
        break;
    }
    vdp.addr = (uint16_t)(vdp.addr + vdp.reg[15]);
}

/* the DMA source and length registers after a DMA: source + length, length 0 */
static void dma_done(void)
{
    uint16_t end = (uint16_t)(vdp.reg[21] + (vdp.reg[22] << 8) + vdp.reg[19] + (vdp.reg[20] << 8));
    vdp.reg[21] = (uint8_t)end;
    vdp.reg[22] = (uint8_t)(end >> 8);
    vdp.reg[19] = vdp.reg[20] = 0;
}

static uint32_t dma_length(void)
{
    uint32_t n = (uint32_t)(vdp.reg[20] << 8 | vdp.reg[19]);
    return n ? n : 0x10000;
}

/* DMA from the 68k bus: words from (reg 23 << 17) + source, within a 128K window */
static void dma_68k(void)
{
    uint32_t n = dma_length();
    uint32_t hi = (uint32_t)(vdp.reg[23] & 0x7F) << 17;
    uint32_t src = (uint32_t)(vdp.reg[22] << 8 | vdp.reg[21]) << 1;
    do {
        uint16_t w = vdp_bus_read16(hi | src);
        src = (src + 2) & 0x1FFFF;
        bus_write(w);
    } while (--n);
    dma_done();
}

/* VRAM copy: bytes from the source address to addr, both through the odd / even swap */
static void dma_copy(void)
{
    if (!(vdp.code & 0x10)) return;
    uint32_t n = dma_length();
    uint16_t src = (uint16_t)(vdp.reg[22] << 8 | vdp.reg[21]);
    do {
        vdp.vram[vdp.addr ^ 1] = vdp.vram[src ^ 1];
        src++;
        vdp.addr = (uint16_t)(vdp.addr + vdp.reg[15]);
    } while (--n);
    dma_done();
}

/* DMA fill (VRAM): the high byte of the last data word through addr */
static void dma_fill(void)
{
    uint32_t n = dma_length();
    uint8_t b = (uint8_t)(vdp.fifo >> 8);
    if ((vdp.code & 0x0F) == 0x01) {
        do {
            vdp.vram[vdp.addr ^ 1] = b;
            vdp.addr = (uint16_t)(vdp.addr + vdp.reg[15]);
        } while (--n);
    }
    dma_done();
}

void vdp_ctrl_write(uint16_t v)
{
    if (!vdp.pending) {
        vdp.addr = (uint16_t)(vdp.addr_latch | (v & 0x3FFF));
        vdp.code = (uint8_t)((vdp.code & 0x3C) | (v >> 14));
        if ((v & 0xC000) == 0x8000) {                     /* a register */
            int r = (v >> 8) & 0x1F;
            if (r < 24) vdp.reg[r] = (uint8_t)v;
        } else {
            vdp.pending = vdp.reg[1] & 4;
        }
        return;
    }
    vdp.pending = 0;
    vdp.addr_latch = (uint16_t)((v & 3) << 14);
    vdp.addr = (uint16_t)(vdp.addr_latch | (vdp.addr & 0x3FFF));
    vdp.code = (uint8_t)((vdp.code & 0x03) | ((v >> 2) & 0x3C));
    if ((vdp.code & 0x20) && (vdp.reg[1] & 0x10)) {
        switch (vdp.reg[23] >> 6) {
        case 2: vdp.dmafill = 1; break;
        case 3: dma_copy(); break;
        default: dma_68k(); break;
        }
    }
}

void vdp_data_write(uint16_t v)
{
    vdp.pending = 0;
    bus_write(v);
    if (vdp.dmafill) {
        vdp.dmafill = 0;
        dma_fill();
    }
}

uint16_t vdp_data_read(void)
{
    uint16_t d = 0;
    vdp.pending = 0;
    switch (vdp.code & 0x1F) {
    case 0x00: {
        uint16_t i = vdp.addr & 0xFFFE;
        d = (uint16_t)(vdp.vram[i] << 8 | vdp.vram[i + 1]);
        break;
    }
    case 0x04: {
        int i = vdp.addr & 0x7E;
        if (i >= 0x50) i = 0;
        d = (uint16_t)((vdp.vsram[i >> 1] & 0x7FF) | (vdp.fifo & ~0x7FF));
        break;
    }
    case 0x08:
        d = (uint16_t)(vdp.cram[(vdp.addr >> 1) & 0x3F] | (vdp.fifo & ~0xEEE));
        break;
    }
    vdp.addr = (uint16_t)(vdp.addr + vdp.reg[15]);
    return d;
}

uint16_t vdp_status_read(void)
{
    vdp.pending = 0;
    return 0x3600;                                        /* FIFO empty, no DMA, NTSC */
}
