/* hal.h for the native build: the VDP model, two 3-button pads, save RAM in a file, and the Z80's
 * side of the command handshake. Sound is not produced yet: the Z80 program is not run, so a
 * command the 68k uploads is taken at once (Z80 RAM $96 back to $7D) and the PSG is ignored. */
#include <stdio.h>
#include <string.h>
#include "hal.h"
#include "vdp.h"
#include "native.h"

uint8_t zram[0x2000];
static uint8_t sram[0x2000];
static uint16_t hv;
static uint8_t pad_th[2] = { 0x40, 0x40 };
static int sram_dirty;

uint16_t native_pads[2];                                  /* held buttons: NATIVE_UP ... */

uint8_t vdp_hv8(void) { hv = (uint16_t)(hv * 0x6255 + 0x3619); return (uint8_t)(hv >> 8); }
uint16_t vdp_hv16(void) { hv = (uint16_t)(hv * 0x6255 + 0x3619); return hv; }
uint16_t vdp_status(void) { return vdp_status_read(); }
uint16_t vdp_read(void) { return vdp_data_read(); }
void vdp_ctrl(uint32_t v) { vdp_ctrl_write((uint16_t)(v >> 16)); vdp_ctrl_write((uint16_t)v); }
void vdp_ctrl16(uint16_t v) { vdp_ctrl_write(v); }
void vdp_data(uint16_t v) { vdp_data_write(v); }
void vdp_data32(uint32_t v) { vdp_data_write((uint16_t)(v >> 16)); vdp_data_write((uint16_t)v); }
void psg_write(uint8_t v) { (void)v; }

void z80_busreq(uint16_t v)
{
    if (v == 0 && zram[0x96] != 0x7D) {                   /* released after a command: done at once */
        zram[0x96] = 0x7D;
        zram[0x97] = 0;
    }
}
uint8_t z80_busack(void) { return 0; }
void z80_reset(uint16_t v) { (void)v; }
uint8_t z80_ram_read8(uint16_t off) { return zram[off & 0x1FFF]; }
void z80_ram_write8(uint16_t off, uint8_t v) { zram[off & 0x1FFF] = v; }

/* a 3-button pad: with TH high the data port reads 0 1 C B R L D U, with TH low 0 0 St A 0 0 D U
 * (active low) */
uint8_t pad_read(uint32_t port)
{
    int n = port == 0xA10005u;
    uint16_t b = native_pads[n];
    if (pad_th[n] & 0x40)
        return (uint8_t)(0x40 | (~((b & NATIVE_C ? 0x20 : 0) | (b & NATIVE_B ? 0x10 : 0) | (b & NATIVE_RIGHT ? 8 : 0) |
                                   (b & NATIVE_LEFT ? 4 : 0) | (b & NATIVE_DOWN ? 2 : 0) | (b & NATIVE_UP ? 1 : 0)) & 0x3F));
    return (uint8_t)(~((b & NATIVE_START ? 0x20 : 0) | (b & NATIVE_A ? 0x10 : 0) | (b & NATIVE_DOWN ? 2 : 0) | (b & NATIVE_UP ? 1 : 0)) & 0x33);
}
void pad_write(uint32_t port, uint8_t v)
{
    if (port == 0xA10003u) pad_th[0] = v;
    else if (port == 0xA10005u) pad_th[1] = v;
}

uint8_t sram_read8(uint32_t off) { return sram[(off >> 1) & 0x1FFF]; }
void sram_write16(uint32_t off, uint16_t v) { sram[(off >> 1) & 0x1FFF] = (uint8_t)v; sram_dirty = 1; }

void native_sram_load(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) return;
    if (fread(sram, 1, sizeof sram, f) != sizeof sram) memset(sram, 0, sizeof sram);
    fclose(f);
}

void native_sram_save(const char *path)
{
    if (!sram_dirty) return;
    FILE *f = fopen(path, "wb");
    if (!f) return;
    fwrite(sram, 1, sizeof sram, f);
    fclose(f);
    sram_dirty = 0;
}

void hal_wait_vblank(void) { native_frame(); }
