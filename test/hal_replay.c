#include "hal.h"
#include "replay.h"
uint8_t vdp_hv8(void) { return (uint8_t)replay_io_read(0xC00008, 1); }
uint16_t vdp_hv16(void) { return (uint16_t)replay_io_read(0xC00008, 2); }
#include "game.h"
/* A routine that waits for the vblank (setvideo spinning on dfok) was interrupted in the capture
 * and its record is skipped, so this only runs for a record that would otherwise hang: take the
 * frame as the vblank would. */
void hal_wait_vblank(void) { g.disflags &= (uint16_t)~HI(dfok); }
void vdp_ctrl(uint32_t v) { (void)v; }   /* VDP writes are not recorded; lockstep checks VRAM later */
void vdp_data(uint16_t v) { (void)v; }
void vdp_ctrl16(uint16_t v) { (void)v; }
void psg_write(uint8_t v) { (void)v; }
