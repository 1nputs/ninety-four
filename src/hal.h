/* Hardware the game logic touches directly. Tests replay recorded values; the native build
 * will implement these over the emulated VDP. */
#pragma once
#include <stdint.h>
uint8_t vdp_hv8(void);     /* move.b (VDP_CNTR).l: V counter */
uint16_t vdp_hv16(void);   /* move.w (VDP_CNTR).l: HV counter word */
void hal_wait_vblank(void); /* spin until vcount changes */
void vdp_ctrl(uint32_t v);   /* move.l to the VDP control port (two words for move.w) */
void vdp_data(uint16_t v);   /* move.w to the VDP data port */
void vdp_ctrl16(uint16_t v); /* move.w to the VDP control port (a register write) */
void psg_write(uint8_t v);   /* move.b to the PSG port */
