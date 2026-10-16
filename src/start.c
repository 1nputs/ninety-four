/* main94.asm:Start and sega/SegaInit.asm: power on. Sega's hardware init (the VDP's registers,
 * VRAM cleared by a DMA fill, the Z80's startup program, work RAM, CRAM, VSRAM and the PSG
 * cleared), the ROM checksum (checksum94.asm), then Begin. */
#include <string.h>
#include "game.h"
#include "hal.h"
#include "rom.h"
#include "symbols_gen.h"

/* sega/SegaInit.asm:SegaInit. A cold start (the I/O chip's control registers still 0) sets
 * everything up from the tables at reg_set; a reset (hot start) only checks the VDP. */
void SegaInit(void)
{
    if (io_read32(0xA10008) == 0 && io_read16(0xA1000C) == 0) {
        const uint8_t *t = rom_ptr(SYM_vreg_dt);          /* the VDP's registers, then the rest */
        if (pad_read(0xA10001) & 0xF) tmss_write(0x53454741);   /* 'SEGA' to the TMSS register */
        vdp_status();
        for (int r = 0; r < 24; r++) vdp_ctrl16((uint16_t)(0x8000 | r << 8 | *t++));
        vdp_ctrl(rd32(t)); t += 4;                        /* VRAM fill from 0, $FFFF bytes */
        vdp_data(0);
        z80_busreq(0x100);
        z80_reset(0x100);
        while (z80_busack() & 1) {}
        for (int i = 0; i < 38; i++) z80_ram_write8((uint16_t)i, *t++);   /* the startup program */
        z80_reset(0);
        z80_busreq(0);
        z80_reset(0x100);
        memset(&g, 0, RAM_SIZE);
        vdp_ctrl(rd32(t)); t += 4;                        /* reg 1 = 4 (display off), reg 15 = 2 */
        vdp_ctrl(rd32(t)); t += 4;                        /* CRAM */
        for (int i = 0; i < 32; i++) vdp_data32(0);
        vdp_ctrl(rd32(t)); t += 4;                        /* VSRAM */
        for (int i = 0; i < 20; i++) vdp_data32(0);
        for (int i = 0; i < 4; i++) psg_write(*t++);      /* the four channels silent */
        z80_reset(0);
    }
    vdp_status();                                         /* CHECK_VDP */
}

/* main94.asm:Start. Power on: SegaInit, the checksum, then the game. */
void Start(void)
{
    SegaInit();
    ValidationRoutine();
    Begin();
}
