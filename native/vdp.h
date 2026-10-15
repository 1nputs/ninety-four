/* The Mega Drive VDP as the game uses it: the ports, VRAM / CRAM / VSRAM, the registers and DMA.
 * Memory layouts follow the hardware: VRAM in VDP byte order, CRAM and VSRAM as bus words. */
#pragma once
#include <stdint.h>

struct Vdp {
    uint8_t reg[32];
    uint8_t vram[0x10000];
    uint16_t cram[64];        /* 0BBB0GGG0RRR0 */
    uint16_t vsram[40];       /* 11 bits */
    uint16_t addr;            /* the access address */
    uint16_t addr_latch;      /* A15-A14 from the second command word */
    uint8_t code;             /* CD5-CD0 */
    uint8_t pending;          /* the first command word was written */
    uint8_t dmafill;          /* a DMA fill waits for the next data write */
    uint16_t fifo;            /* the last word written (unused bits of CRAM / VSRAM reads) */
};

extern struct Vdp vdp;
/* reads a word of the 68k bus for DMA (ROM or work RAM) */
extern uint16_t (*vdp_bus_read16)(uint32_t addr);

void vdp_reset(void);
void vdp_ctrl_write(uint16_t v);
void vdp_data_write(uint16_t v);
uint16_t vdp_data_read(void);
uint16_t vdp_status_read(void);

/* render one frame of the current VDP state into pixels (0x00RRGGBB), pitch 320; returns the
 * width (256 in 32-cell mode, 320 in 40-cell mode); 224 lines */
int vdp_render(uint32_t *pixels);
