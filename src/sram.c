/* sram94.asm: the battery save RAM, one byte in the odd half of each word at $200000. */
#include "game.h"
#include <string.h>
#include "hal.h"
#include "symbols_gen.h"

/* sram94.asm:ReadSRAM. d0 = first save RAM byte, d1 = count, a0 = destination. */
void ReadSRAM(uint32_t at, uint32_t count, uint32_t dst)
{
    uint16_t off = (uint16_t)(at << 1);
    uint8_t *p = mem_ptr(dst);
    uint16_t n = (uint16_t)(count - 1);
    do { *p++ = sram_read8((uint32_t)(int16_t)off + 1); off += 2; } while (n-- != 0);
}

/* sram94.asm:WriteSRAM. d0 = first save RAM byte, d1 = count, a0 = source (each byte written as
 * the low byte of a word). */
void WriteSRAM(uint32_t at, uint32_t count, uint32_t src)
{
    uint16_t off = (uint16_t)(at << 1);
    const uint8_t *p = mem_ptr(src);
    uint16_t n = (uint16_t)(count - 1);
    do { sram_write16((uint32_t)(int16_t)off, *p++); off += 2; } while (n-- != 0);
}

/* sram94.asm:MakeSRAMChecksum. Read all of save RAM to M68K_RAM, put the byte sum of bytes
 * 0-$1FFD in byte $1FFF and its complement in $1FFE, and write those two back. */
void MakeSRAMChecksum(void)
{
    ReadSRAM(0, 0x2000, RAM_BASE);
    uint8_t *ram = (uint8_t *)&g;
    uint8_t sum = 0;
    for (int i = 0; i < 0x1FFE; i++) sum = (uint8_t)(sum + ram[i]);
    ram[0x1FFF] = sum;
    ram[0x1FFE] = (uint8_t)~sum;
    WriteSRAM(0x1FFE, 2, RAM_BASE + 0x1FFE);
}

/* sram94.asm:InitSaveRAM. Copy save RAM to work RAM $0000-$1FFF and check it (ValidateSRAM); a
 * bad one is cleared and read again. With the vblank counting vcount only (VBcount). Two
 * power-on tests that never return: Start+A+C held writes and reads back every bit of every
 * byte, then $12 $34 $56 $78 to bytes 0-3, and flashes the screen green (red at the first bad
 * byte); Start+B+C held flashes green if bytes 0-3 are $12345678, else red. */
void InitSaveRAM(void)
{
    g.vbint = SYM_VBcount;
    cpu_set_sr(0x2500);
    g.ValidSRAM = 0;
    uint16_t d0, d1, d2, held;
    ReadJoy1(&d0, &d1, &d2, &held);
    uint16_t colour = 0xE, step = 2;                      /* red, fading by 2 a frame */
    if ((uint8_t)held == 0xE0) {
        for (uint32_t off = 0; off < 0x4000; off += 2)
            for (uint16_t bit = 1; bit < 0x100; bit <<= 1) {
                sram_write16(off, bit);
                if (sram_read8(off + 1) != (uint8_t)bit) goto flash;
            }
        sram_write16(0, 0x12); sram_write16(2, 0x34);     /* (two move.l: a word to each byte) */
        sram_write16(4, 0x56); sram_write16(6, 0x78);
        colour = 0xE0; step = 0x20;                       /* green */
        goto flash;
    }
    if ((uint8_t)held == 0xB0) {
        uint32_t v = 0;
        for (uint32_t off = 1; off < 8; off += 2) v = v << 8 | sram_read8(off);
        if (v == 0x12345678) { colour = 0xE0; step = 0x20; }
        goto flash;
    }
    ReadSRAM(0, 0x2000, RAM_BASE);
    ValidateSRAM();
    if ((int16_t)g.ValidSRAM >= 0) return;
    ClearSRAM();
    ReadSRAM(0, 0x2000, RAM_BASE);
    ValidateSRAM();
    return;
flash:
    for (uint16_t c = colour;; c = (uint16_t)(c - step)) {
        vdp_ctrl(CRAM_W);                                 /* colour 0: the background */
        vdp_data(c & colour);
        SRAMWaitVblank();
    }
}

/* sram94.asm:SRAMWaitVblank. Wait for the next vblank. */
void SRAMWaitVblank(void)
{
    vcountwait();
}

/* sram94.asm:VBcount. The vblank while InitSaveRAM runs: vcount + 1 only. */
void VBcount(void)
{
    g.vcount++;
}

/* sram94.asm:ValidateSRAM. Check the copy of save RAM at work RAM 0: byte $1FFF must be the sum
 * of bytes 0-$1FFD and byte $1FFE its complement. ValidSRAM = 0 if so, else $FF in its high
 * byte. */
void ValidateSRAM(void)
{
    const uint8_t *ram = (const uint8_t *)&g;
    uint8_t sum = 0;
    for (int i = 0; i < 0x1FFE; i++) sum = (uint8_t)(sum + ram[i]);
    int bad = (ram[0x1FFF] != sum) + (ram[0x1FFE] != (uint8_t)~sum) + ((uint8_t)~ram[0x1FFE] != ram[0x1FFF]);
    if (!bad) g.ValidSRAM = 0;
    else ST_W(g.ValidSRAM);
}

/* sram94.asm:ClearSRAM. Zero work RAM $0000-$1FFF but byte $1FFE = $FF (the checksum of
 * zeros), write it to save RAM, then byte 0 = 1 to save RAM byte 1, then MakeSRAMChecksum. */
void ClearSRAM(void)
{
    uint8_t *ram = (uint8_t *)&g;
    memset(ram, 0, 0x2000);
    ram[RAM_OFF_sramcleared] = 0xFF;
    WriteSRAM(0, 0x2000, RAM_BASE);
    ram[0] = 1;
    WriteSRAM(1, 1, RAM_BASE);
    MakeSRAMChecksum();
}
