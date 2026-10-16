/* Hardware the game logic touches directly. Tests replay recorded reads and check writes
 * against the recorded order (test/hal_replay.c); the native build implements these over its
 * own VDP, PSG and Z80 models. Each function is one 68k access of the width it names. */
#pragma once
#include <stdint.h>
#define VDP_DATA_PORT 0xC00000u
#define VDP_CTRL_PORT 0xC00004u
#define VDP_HV_PORT   0xC00008u
#define PSG_PORT      0xC00011u
#define Z80_BUSREQ    0xA11100u
#define SRAM_BASE     0x200000u
#define Z80_RESET     0xA11200u
#define Z80_RAM_BASE  0xA00000u
/* VDP commands (equals.inc): a write to VSRAM or CRAM at address 0; + (address << 16) for another
 * address, as the asm adds (n<<16) */
#define VSRAM_W       0x40000010u
#define CRAM_W        0xC0000000u
uint8_t vdp_hv8(void);       /* move.b (VDP_CNTR).l: V counter */
uint16_t vdp_hv16(void);     /* move.w (VDP_CNTR).l: HV counter word */
uint16_t vdp_status(void);   /* move.w (VDP_CTRL).l: status word (bit 1 = DMA busy) */
uint16_t vdp_read(void);     /* move.w (VDP_DATA).l: read at the current address */
void hal_wait_vblank(void);  /* spin until vcount changes */
void vdp_ctrl(uint32_t v);   /* move.l to the VDP control port (a command: two words) */
void vdp_ctrl16(uint16_t v); /* move.w to the VDP control port (a register write or half a command) */
void vdp_data(uint16_t v);   /* move.w to the VDP data port */
void vdp_data32(uint32_t v); /* move.l to the VDP data port (two words) */
void psg_write(uint8_t v);   /* move.b to the PSG port */
void z80_busreq(uint16_t v); /* move.w to IO_Z80BUS: $100 requests the bus, 0 releases it */
uint8_t z80_busack(void);    /* move.b (IO_Z80BUS).l: bit 0 set while the Z80 still has the bus */
uint8_t sram_read8(uint32_t off);              /* move.b from save RAM: $200000 + off */
void sram_write16(uint32_t off, uint16_t v);   /* move.w to save RAM (the byte is the odd half) */
uint8_t z80_ram_read8(uint16_t off);           /* move.b from Z80 RAM ($A00000 + off), bus held */
void z80_ram_write8(uint16_t off, uint8_t v);  /* move.b to Z80 RAM */
void z80_reset(uint16_t v);                    /* move.w to IO_Z80RES: 0 holds the Z80 in reset, $100 runs it */
uint8_t pad_read(uint32_t port);               /* move.b from a pad data port ($A10003 / $A10005) */
void pad_write(uint32_t port, uint8_t v);      /* move.b to a pad data port (TH select, 4 way adaptor select) */
uint16_t cpu_get_sr(void);                     /* move sr,-(sp): the status register (its interrupt mask) */
void cpu_set_sr(uint16_t sr);                  /* move #sr,sr / move (sp)+,sr: a vblank that came while masked is taken at the unmask */
uint32_t io_read32(uint32_t addr);             /* tst.l of the I/O chip's registers ($A10000-$A1001F) */
uint16_t io_read16(uint32_t addr);             /* tst.w of them */
void tmss_write(uint32_t v);                   /* move.l to $A14000 (TMSS: 'SEGA' unlocks the VDP) */
