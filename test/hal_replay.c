#include "hal.h"
#include "replay.h"
uint8_t vdp_hv8(void) { return (uint8_t)replay_io_read(VDP_HV_PORT, 1); }
uint16_t vdp_hv16(void) { return (uint16_t)replay_io_read(VDP_HV_PORT, 2); }
uint16_t vdp_status(void) { return (uint16_t)replay_io_read(VDP_CTRL_PORT, 2); }
uint16_t vdp_read(void) { return (uint16_t)replay_io_read(VDP_DATA_PORT, 2); }
uint8_t z80_busack(void) { return (uint8_t)replay_io_read(Z80_BUSREQ, 1); }
#include "game.h"
/* A routine that waits for the vblank (setvideo spinning on dfok, forcefade on palcount, waitx
 * on vcount) was interrupted in the capture and its record is skipped, so this only runs for a
 * record that would otherwise hang: do what every wait is waiting for. */
void hal_wait_vblank(void) { g.disflags &= (uint16_t)~HI(dfok); g.vcount++; g.palcount = 0xFFFF; }
/* Writes are checked against the capture's write log, in order (captures made before the shim
 * logged writes have none, and then nothing is checked). */
void vdp_ctrl(uint32_t v) { replay_io_write(VDP_CTRL_PORT, 4, v); }
void vdp_ctrl16(uint16_t v) { replay_io_write(VDP_CTRL_PORT, 2, v); }
void vdp_data(uint16_t v) { replay_io_write(VDP_DATA_PORT, 2, v); }
void vdp_data32(uint32_t v) { replay_io_write(VDP_DATA_PORT, 4, v); }
void psg_write(uint8_t v) { replay_io_write(PSG_PORT, 1, v); }
void z80_busreq(uint16_t v) { replay_io_write(Z80_BUSREQ, 2, v); }
