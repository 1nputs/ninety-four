/* hal.h on md-runtime: the VDP, pads and save RAM are the runtime's; the Z80 is src/z80.c, the
 * sound relay ported to C, whose YM2612 writes go to the runtime's FM chip at their time.
 *
 * Time: the runtime makes each frame's sound at the handshake, every write at its master clock in
 * the frame. The game's own code has no clock natively, so each access it makes to the Z80's bus
 * or the PSG counts as a few 68k cycles: when the game waits on the Z80 (polling Z80 RAM for the
 * end of a command) the relay runs on meanwhile, as it would beside a real 68k. The vblank
 * interrupt starts each frame; the relay runs the rest of it through the runtime's sound tick. */
#include <stdio.h>
#include <string.h>
#include "game.h"
#include "hal.h"
#include "native.h"
#include "z80.h"

mdrt *native_m;
static uint64_t frame_base;             /* the relay's master clock at this frame's start */
static uint32_t t68k;                   /* master clocks the game has spent in this frame */
static long frames;

#define BUS_ACCESS 56                   /* a 68k byte access through the Z80's bus, with its instruction */
#define PORT_ACCESS 28                  /* a write to the PSG */

/* the relay catches up with the game */
static void relay_now(void)
{
    uint64_t now = frame_base + t68k;
    while (now > z80.now) {
        uint64_t step = now - z80.now > 1u << 30 ? 1u << 30 : now - z80.now;
        z80_relay_run((uint32_t)step);
    }
}

/* the relay's YM2612 writes and its reset, to the runtime's chip at their time in the frame */
void z80_ym_port(int port, uint8_t value)
{
    mdrt_ym_write(native_m, z80.zc > frame_base ? (uint32_t)(z80.zc - frame_base) : 0, port, value);
}
void z80_ym_reset(void)
{
    mdrt_ym_reset(native_m, z80.now > frame_base ? (uint32_t)(z80.now - frame_base) : 0);
}

/* the runtime's sound tick: the relay through the frame */
static void sound_tick(void *user, uint32_t until)
{
    (void)user;
    uint64_t target = frame_base + until;
    while (target > z80.now) {
        uint64_t step = target - z80.now > 1u << 30 ? 1u << 30 : target - z80.now;
        z80_relay_run((uint32_t)step);
    }
}

void native_hal_start(mdrt *m, uint64_t start)
{
    native_m = m;
    frame_base = start;
    t68k = 0;
    mdrt_set_sound_tick(m, sound_tick, NULL);
}

static mdrt_vdp *vdp(void) { return mdrt_get_vdp(native_m); }
uint8_t vdp_hv8(void) { return mdrt_vdp_hv8(vdp(), 0); }
uint16_t vdp_hv16(void) { return mdrt_vdp_hv16(vdp()); }
uint16_t vdp_status(void) { return mdrt_vdp_status(vdp()); }
uint16_t vdp_read(void) { return mdrt_vdp_read16(vdp()); }
void vdp_ctrl(uint32_t v) { mdrt_vdp_ctrl32(vdp(), v); }
void vdp_ctrl16(uint16_t v) { mdrt_vdp_ctrl16(vdp(), v); }
void vdp_data(uint16_t v) { mdrt_vdp_data16(vdp(), v); }
void vdp_data32(uint32_t v) { mdrt_vdp_data32(vdp(), v); }

void psg_write(uint8_t v) { t68k += PORT_ACCESS; mdrt_psg_port(native_m, t68k, v); }

void z80_busreq(uint16_t v) { t68k += BUS_ACCESS; relay_now(); z80_relay_busreq(v); }
uint8_t z80_busack(void) { t68k += BUS_ACCESS; relay_now(); return 0; }   /* granted at once */
void z80_reset(uint16_t v) { t68k += BUS_ACCESS; relay_now(); z80_relay_reset(v); }
uint8_t z80_ram_read8(uint16_t off) { t68k += BUS_ACCESS; relay_now(); return z80_ram[off & 0x1FFF]; }
void z80_ram_write8(uint16_t off, uint8_t v) { t68k += BUS_ACCESS; relay_now(); z80_ram[off & 0x1FFF] = v; }

uint8_t pad_read(uint32_t port) { return mdrt_pad_read(mdrt_get_pads(native_m), port); }
void pad_write(uint32_t port, uint8_t v) { mdrt_pad_write(mdrt_get_pads(native_m), port, v); }

/* save RAM: the bytes on the odd addresses */
uint8_t sram_read8(uint32_t off) { return mdrt_sram_read8(mdrt_get_sram(native_m), off | 1); }
void sram_write16(uint32_t off, uint16_t v) { mdrt_sram_write8(mdrt_get_sram(native_m), off | 1, (uint8_t)v); }

/* the frame's end: the handshake, then the next frame begins with the vblank interrupt */
void hal_wait_vblank(void)
{
    native_frame_done(++frames);
    if (mdrt_wait_vblank(native_m)) native_quit();
    frame_base += mdrt_frame_mclks(native_m);
    t68k = 0;
    z80_relay_frame();                                    /* (the YM2612's busy grid) */
    if (vdp()->s.reg[1] & 0x20) call68k(g.vbint);         /* the vblank interrupt */
    mdrt_vblank_end(native_m);
}
