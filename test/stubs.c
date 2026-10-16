/* Recorded stand-ins for routines that are not ported yet. Weak, so a real port in src/
 * overrides its stub at link time. Each stub checks the arguments the C caller passes against
 * the registers the 68k caller passed, applies the callee's RAM changes, and returns its results. */
#include <stdio.h>
#include "game.h"
#include "replay.h"
#include "symbols_gen.h"
#include "rom.h"
#include <string.h>

#define STUB(name) __attribute__((weak))
#define TAKE(name) const Call *c = replay_next(SYM_##name); if (!c) { replay_fail("no recorded call of " #name); return; }
#define TAKE_RET(name, val) const Call *c = replay_next(SYM_##name); if (!c) { replay_fail("no recorded call of " #name); return val; }
#define ARG16(reg, v, name) if ((uint16_t)c->regs_in[reg] != (uint16_t)(v)) replay_fail(#name ": " #reg " = %04x, recorded %04x", (uint16_t)(v), (uint16_t)c->regs_in[reg])
#define ARGPTR(reg, p, name) if ((uint16_t)c->regs_in[reg] != (uint16_t)((uint8_t *)(p) - (uint8_t *)&g)) replay_fail(#name ": " #reg " = %04x, recorded %04x", (uint16_t)((uint8_t *)(p) - (uint8_t *)&g), (uint16_t)c->regs_in[reg])

STUB(GetHot) void GetHot(const struct SortObj *o, int16_t *hx, int16_t *hy)
{
    TAKE(GetHot);
    uint32_t pushed = (uint32_t)c->stack[4] << 24 | c->stack[5] << 16 | c->stack[6] << 8 | c->stack[7];
    if ((uint16_t)pushed != (uint16_t)((const uint8_t *)o - (const uint8_t *)&g)) replay_fail("GetHot: struct %04x, recorded %04x", (uint16_t)((const uint8_t *)o - (const uint8_t *)&g), (uint16_t)pushed);
    replay_apply(c); *hx = (int16_t)c->regs_out[R_D0]; *hy = (int16_t)c->regs_out[R_D1];
}
STUB(vtoa) uint16_t vtoa(int16_t x, int16_t y)
{
    TAKE_RET(vtoa, 8); ARG16(R_D0, x, vtoa); ARG16(R_D1, y, vtoa); replay_apply(c); return (uint16_t)c->regs_out[R_D0];
}
STUB(SetSPA) void SetSPA(struct SortObj *o, uint16_t spa)
{
    TAKE(SetSPA); ARGPTR(R_A3, o, SetSPA); ARG16(R_D1, spa, SetSPA); replay_apply(c);
}

/* ---- callees of the collision code that are not ported yet ---- */
STUB(checkwallcoll) void checkwallcoll(int16_t x, int16_t y, struct SortObj *o)
{
    TAKE(checkwallcoll); ARG16(R_D2, x, checkwallcoll); ARG16(R_D3, y, checkwallcoll); ARGPTR(R_A3, o, checkwallcoll); replay_apply(c);
}
#define ARG32(reg, v, name) if (c->regs_in[reg] != (uint32_t)(v)) replay_fail(#name ": " #reg " = %08x, recorded %08x", (uint32_t)(v), c->regs_in[reg])
#define ARGTEAM(reg, p, name) ARGPTR(reg, p, name)
STUB(assreplace) void assreplace(uint16_t ass, struct SortObj *o) { TAKE(assreplace); ARG16(R_D0, ass, assreplace); ARGPTR(R_A3, o, assreplace); replay_apply(c); }
STUB(assinsert) void assinsert(uint16_t ass, struct SortObj *o) { TAKE(assinsert); ARG16(R_D0, ass, assinsert); ARGPTR(R_A3, o, assinsert); replay_apply(c); }

/* ---- callees of checks94 that are not ported yet ---- */
#define CCR(c) ((uint8_t)((c)->regs_out[R_SR] & 0xFF))
#define ZF(c) (((c)->regs_out[R_SR] >> 2) & 1)
static uint32_t stk32(const Call *c, int off) { return (uint32_t)c->stack[off] << 24 | c->stack[off + 1] << 16 | c->stack[off + 2] << 8 | c->stack[off + 3]; }
/* printz / printsmallz read the string that follows the call: compare it with the C copy */
#define INLINE_STRING(name) do { uint32_t ret = stk32(c, 0); \
    if (memcmp(rom_ptr(ret), s, n) != 0) replay_fail(#name ": inline string differs from the ROM at %06x", ret); } while (0)
STUB(ExitToOpening) void ExitToOpening(void) { TAKE(ExitToOpening); replay_apply(c); }
/* video94 / sound94 / title94 / setup94 / menu94 helpers of the display and replay modules */
/* call68k to a routine that is not ported yet: its recorded call */
void call68k_missing(uint32_t addr)
{
    const Call *c = replay_next(addr);
    if (!c) { replay_fail("no recorded call of %06x (call68k)", addr); return; }
    replay_apply(c);
}
/* the intermission and the game over: frame drivers / front end, not ported yet */
STUB(GameOver) void GameOver(void) { TAKE(GameOver); replay_apply(c); }
STUB(PlayoffScreen) void PlayoffScreen(void) { TAKE(PlayoffScreen); replay_apply(c); }
STUB(ScoutingReport) void ScoutingReport(void) { TAKE(ScoutingReport); replay_apply(c); }
