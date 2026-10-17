/* A hardware layer that does nothing (hal.h): the ports read 0 and writes go nowhere, for tests
 * that call the game's routines on RAM set up by hand (rules_test.c). */
#include <stdint.h>
#include "hal.h"

static uint16_t sr = 0x2700;
uint8_t vdp_hv8(void) { return 0; }
uint16_t vdp_hv16(void) { return 0; }
uint16_t vdp_status(void) { return 0; }
uint16_t vdp_read(void) { return 0; }
void hal_wait_vblank(void) {}
void vdp_ctrl(uint32_t v) { (void)v; }
void vdp_ctrl16(uint16_t v) { (void)v; }
void vdp_data(uint16_t v) { (void)v; }
void vdp_data32(uint32_t v) { (void)v; }
void psg_write(uint8_t v) { (void)v; }
void z80_busreq(uint16_t v) { (void)v; }
uint8_t z80_busack(void) { return 0; }
uint8_t sram_read8(uint32_t off) { (void)off; return 0; }
void sram_write16(uint32_t off, uint16_t v) { (void)off; (void)v; }
uint8_t z80_ram_read8(uint16_t off) { (void)off; return 0; }
void z80_ram_write8(uint16_t off, uint8_t v) { (void)off; (void)v; }
void z80_reset(uint16_t v) { (void)v; }
uint8_t pad_read(uint32_t port) { (void)port; return 0x7F; }
void pad_write(uint32_t port, uint8_t v) { (void)port; (void)v; }
uint16_t cpu_get_sr(void) { return sr; }
void cpu_set_sr(uint16_t v) { sr = v; }
uint32_t io_read32(uint32_t addr) { (void)addr; return 0; }
uint16_t io_read16(uint32_t addr) { (void)addr; return 0; }
void tmss_write(uint32_t v) { (void)v; }

/* the relay's YM2612 (z80.h): nowhere */
#include "z80.h"
void z80_ym_port(int port, uint8_t value) { (void)port; (void)value; }
void z80_ym_reset(void) {}
