/* Recorded stand-ins for routines that are not ported yet. Weak, so a real port in src/ overrides
 * its stub at link time. A stub checks the arguments the C caller passes against the registers
 * the 68k caller passed, applies the callee's RAM changes and returns its results, so any routine
 * can be ported and tested before its callees. Every routine of NHL '94 is ported now; the macros
 * stay as the pattern for one that is not (a port of NHL '95 starts here):
 *
 *     STUB(vtoa) uint16_t vtoa(int16_t x, int16_t y)
 *     {
 *         TAKE_RET(vtoa, 0); ARG16(R_D0, x, vtoa); ARG16(R_D1, y, vtoa);
 *         replay_apply(c); return (uint16_t)c->regs_out[R_D0];
 *     }
 */
#include <stdio.h>
#include "game.h"
#include "replay.h"
#include "symbols_gen.h"

#define STUB(name) __attribute__((weak))
#define TAKE(name) const Call *c = replay_next(SYM_##name); if (!c) { replay_fail("no recorded call of " #name); return; }
#define TAKE_RET(name, val) const Call *c = replay_next(SYM_##name); if (!c) { replay_fail("no recorded call of " #name); return val; }
#define ARG16(reg, v, name) if ((uint16_t)c->regs_in[reg] != (uint16_t)(v)) replay_fail(#name ": " #reg " = %04x, recorded %04x", (uint16_t)(v), (uint16_t)c->regs_in[reg])
#define ARG32(reg, v, name) if (c->regs_in[reg] != (uint32_t)(v)) replay_fail(#name ": " #reg " = %08x, recorded %08x", (uint32_t)(v), c->regs_in[reg])

/* call68k (a jsr (a0) to an address kept in RAM) to a routine dispatch.c does not know: its
 * recorded call */
void call68k_missing(uint32_t addr)
{
    const Call *c = replay_next(addr);
    if (!c) { replay_fail("no recorded call of %06x (call68k)", addr); return; }
    replay_apply(c);
}
