#include "hal.h"
#include "z80.h"
#include "replay.h"
uint8_t vdp_hv8(void) { return (uint8_t)replay_io_read(VDP_HV_PORT, 1); }
uint16_t vdp_hv16(void) { return (uint16_t)replay_io_read(VDP_HV_PORT, 2); }
uint16_t vdp_status(void) { return (uint16_t)replay_io_read(VDP_CTRL_PORT, 2); }
uint16_t vdp_read(void) { return (uint16_t)replay_io_read(VDP_DATA_PORT, 2); }
uint8_t z80_busack(void) { return (uint8_t)replay_io_read(Z80_BUSREQ, 1); }
uint8_t z80_ram_read8(uint16_t off) { return (uint8_t)replay_io_read(Z80_RAM_BASE + off, 1); }
uint8_t pad_read(uint32_t port) { return (uint8_t)replay_io_read(port, 1); }
static uint16_t sr = 0x2700;
uint16_t cpu_get_sr(void) { return sr; }
void cpu_set_sr(uint16_t v) { sr = v; }                                   /* (the captures place the interrupts) */
uint32_t io_read32(uint32_t addr) { return (uint32_t)replay_io_read(addr, 4); }
uint16_t io_read16(uint32_t addr) { return (uint16_t)replay_io_read(addr, 2); }
uint8_t sram_read8(uint32_t off) { return (uint8_t)replay_io_read(SRAM_BASE + off, 1); }
#include "game.h"
/* A vblank wait: the interrupt the 68k took in its wait loop, replayed (its RAM changes; its
 * own calls and port accesses go with it). Without one recorded (an old capture, or the C waits
 * where the 68k did not), fail and do what every wait waits for, so nothing hangs. */
void hal_wait_vblank(void)
{
    if (replay_irq()) return;
    replay_fail("vblank wait with no recorded interrupt");
    g.disflags &= (uint16_t)~HI(dfok); g.vcount++; g.palcount = 0xFFFF;
}
/* Writes are checked against the capture's write log, in order (captures made before the shim
 * logged writes have none, and then nothing is checked). */
void vdp_ctrl(uint32_t v) { replay_io_write(VDP_CTRL_PORT, 4, v); }
void vdp_ctrl16(uint16_t v) { replay_io_write(VDP_CTRL_PORT, 2, v); }
void vdp_data(uint16_t v) { replay_io_write(VDP_DATA_PORT, 2, v); }
void vdp_data32(uint32_t v) { replay_io_write(VDP_DATA_PORT, 4, v); }
void psg_write(uint8_t v) { replay_io_write(PSG_PORT, 1, v); }
void z80_busreq(uint16_t v) { replay_io_write(Z80_BUSREQ, 2, v); }
void sram_write16(uint32_t off, uint16_t v) { replay_io_write(SRAM_BASE + off, 2, v); }
void z80_ram_write8(uint16_t off, uint8_t v) { replay_io_write(Z80_RAM_BASE + off, 1, v); }
void z80_reset(uint16_t v) { replay_io_write(Z80_RESET, 2, v); }
void pad_write(uint32_t port, uint8_t v) { replay_io_write(port, 1, v); }
void tmss_write(uint32_t v) { (void)v; }                                  /* (the captures do not log $A14000) */
void z80_ym_port(int port, uint8_t value) { (void)port; (void)value; }   /* the captures do not follow the Z80 */
void z80_ym_reset(void) {}
