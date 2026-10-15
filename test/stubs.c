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

STUB(randomd0) uint16_t randomd0(uint16_t range)
{
    TAKE_RET(randomd0, 0); ARG16(R_D0, range, randomd0); replay_apply(c); return (uint16_t)c->regs_out[R_D0];
}
STUB(sfx) void sfx(int16_t n)
{
    TAKE(sfx);
    uint16_t pushed = (uint16_t)(c->stack[4] << 8 | c->stack[5]);
    if (pushed != (uint16_t)n) replay_fail("sfx: %04x, recorded %04x", (uint16_t)n, pushed);
    replay_apply(c);
}
STUB(song) void song(int16_t n)
{
    TAKE(song);
    uint16_t pushed = (uint16_t)(c->stack[4] << 8 | c->stack[5]);
    if (pushed != (uint16_t)n) replay_fail("song: %04x, recorded %04x", (uint16_t)n, pushed);
    replay_apply(c);
}
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
STUB(wallcollduringcheck) void wallcollduringcheck(struct SortObj *a2, struct SortObj *a3)
{
    TAKE(wallcollduringcheck); ARGPTR(R_A2, a2, wallcollduringcheck); ARGPTR(R_A3, a3, wallcollduringcheck); replay_apply(c);
}
#define ARG32(reg, v, name) if (c->regs_in[reg] != (uint32_t)(v)) replay_fail(#name ": " #reg " = %08x, recorded %08x", (uint32_t)(v), c->regs_in[reg])
#define ARGTEAM(reg, p, name) ARGPTR(reg, p, name)
STUB(randomd0s) uint16_t randomd0s(uint16_t range) { TAKE_RET(randomd0s, 0); ARG16(R_D0, range, randomd0s); replay_apply(c); return (uint16_t)c->regs_out[R_D0]; }
STUB(stop_current_song) void stop_current_song(void) { TAKE(stop_current_song); replay_apply(c); }
STUB(ChooseSong) void ChooseSong(void) { TAKE(ChooseSong); replay_apply(c); }
STUB(assreplace) void assreplace(uint16_t ass, struct SortObj *o) { TAKE(assreplace); ARG16(R_D0, ass, assreplace); ARGPTR(R_A3, o, assreplace); replay_apply(c); }
STUB(assinsert) void assinsert(uint16_t ass, struct SortObj *o) { TAKE(assinsert); ARG16(R_D0, ass, assinsert); ARGPTR(R_A3, o, assinsert); replay_apply(c); }
STUB(EndOneTimer) void EndOneTimer(struct SortObj *o) { TAKE(EndOneTimer); ARGPTR(R_A3, o, EndOneTimer); replay_apply(c); }
STUB(sroot) uint16_t sroot(uint32_t v) { TAKE_RET(sroot, 0); ARG32(R_D0, v, sroot); replay_apply(c); return (uint16_t)c->regs_out[R_D0]; }
STUB(GetPlayerCount) uint16_t GetPlayerCount(uint8_t *team) { TAKE_RET(GetPlayerCount, 0); ARGTEAM(R_A2, team, GetPlayerCount); replay_apply(c); return (uint16_t)c->regs_out[R_D0]; }
STUB(Set4WayPlayerStub) void Set4WayPlayerStub(struct SortObj *o) { TAKE(Set4WayPlayerStub); ARGPTR(R_A3, o, Set4WayPlayerStub); replay_apply(c); }
STUB(getFgtbyte) uint16_t getFgtbyte(struct SortObj *a2, struct SortObj *a3)
{
    TAKE_RET(getFgtbyte, 0); ARGPTR(R_A2, a2, getFgtbyte); ARGPTR(R_A3, a3, getFgtbyte); replay_apply(c); return (uint16_t)c->regs_out[R_D0];
}
STUB(chkFgtBit1) int chkFgtBit1(struct SortObj *a2, struct SortObj *a3)
{
    TAKE_RET(chkFgtBit1, 1); ARGPTR(R_A2, a2, chkFgtBit1); ARGPTR(R_A3, a3, chkFgtBit1); replay_apply(c); return (c->regs_out[R_SR] >> 2) & 1;
}

/* ---- callees of checks94 that are not ported yet ---- */
#define CCR(c) ((uint8_t)((c)->regs_out[R_SR] & 0xFF))
#define ZF(c) (((c)->regs_out[R_SR] >> 2) & 1)
static uint32_t stk32(const Call *c, int off) { return (uint32_t)c->stack[off] << 24 | c->stack[off + 1] << 16 | c->stack[off + 2] << 8 | c->stack[off + 3]; }
/* printz / printsmallz read the string that follows the call: compare it with the C copy */
#define INLINE_STRING(name) do { uint32_t ret = stk32(c, 0); \
    if (memcmp(rom_ptr(ret), s, n) != 0) replay_fail(#name ": inline string differs from the ROM at %06x", ret); } while (0)
STUB(printz) void printz(const uint8_t *s, unsigned n) { TAKE(printz); INLINE_STRING(printz); replay_apply(c); }
STUB(printsmallz) void printsmallz(const uint8_t *s, unsigned n) { TAKE(printsmallz); INLINE_STRING(printsmallz); replay_apply(c); }
STUB(ReadGoaliePulled) uint8_t ReadGoaliePulled(struct SortObj *o) { TAKE_RET(ReadGoaliePulled, 0); ARGPTR(R_A3, o, ReadGoaliePulled); replay_apply(c); return CCR(c); }
STUB(ShootoutShootCheck) int ShootoutShootCheck(struct SortObj *o) { TAKE_RET(ShootoutShootCheck, 0); ARGPTR(R_A3, o, ShootoutShootCheck); replay_apply(c); return ZF(c); }
STUB(SkatePath) void SkatePath(int16_t *x, int16_t *y, struct SortObj *o)
{
    TAKE(SkatePath); ARG16(R_D0, *x, SkatePath); ARG16(R_D1, *y, SkatePath); ARGPTR(R_A3, o, SkatePath); replay_apply(c);
    *x = (int16_t)c->regs_out[R_D0]; *y = (int16_t)c->regs_out[R_D1];
}
STUB(NextShooter) void NextShooter(void) { TAKE(NextShooter); replay_apply(c); }
STUB(StartShootoutPath) void StartShootoutPath(void) { TAKE(StartShootoutPath); replay_apply(c); }
STUB(CountShootoutGoals) void CountShootoutGoals(void) { TAKE(CountShootoutGoals); replay_apply(c); }
STUB(CheckScoreLeader) void CheckScoreLeader(void) { TAKE(CheckScoreLeader); replay_apply(c); }
STUB(RandomFaceoffAnim) uint16_t RandomFaceoffAnim(void) { TAKE_RET(RandomFaceoffAnim, 0xFFFF); replay_apply(c); return (uint16_t)c->regs_out[R_D0]; }
STUB(LeadSong) void LeadSong(void) { TAKE(LeadSong); replay_apply(c); }
STUB(PeriodOver) void PeriodOver(void) { TAKE(PeriodOver); replay_apply(c); }
STUB(resetplstuff) void resetplstuff(void) { TAKE(resetplstuff); replay_apply(c); }
STUB(SprSort) void SprSort(void) { TAKE(SprSort); replay_apply(c); }
STUB(forceblack) void forceblack(void) { TAKE(forceblack); replay_apply(c); }
STUB(eraser) void eraser(uint16_t w, uint16_t h, uint16_t attr) { TAKE(eraser); ARG16(R_D0, w, eraser); ARG16(R_D1, h, eraser); ARG16(R_D2, attr, eraser); replay_apply(c); }
STUB(Framer) void Framer(uint16_t w, uint16_t h) { TAKE(Framer); ARG16(R_D0, w, Framer); ARG16(R_D1, h, Framer); replay_apply(c); }
STUB(DecompressGraphicsPlain) uint16_t DecompressGraphicsPlain(uint32_t tiles, uint16_t start)
{
    TAKE_RET(DecompressGraphicsPlain, start); ARG32(R_A2, tiles, DecompressGraphicsPlain); ARG16(R_D4, start, DecompressGraphicsPlain); replay_apply(c); return (uint16_t)c->regs_out[R_D4];
}
STUB(dobitmap) uint16_t dobitmap(uint32_t pal, uint32_t map, uint32_t tiles, uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t start, uint16_t palbits)
{
    TAKE_RET(dobitmap, start); if (palbits) ARG32(R_A0, pal, dobitmap); ARG32(R_A1, map, dobitmap); ARG32(R_A2, tiles, dobitmap);
    ARG16(R_D0, x, dobitmap); ARG16(R_D1, y, dobitmap); ARG16(R_D2, w, dobitmap); ARG16(R_D3, h, dobitmap); ARG16(R_D4, start, dobitmap); ARG16(R_D5, palbits, dobitmap);
    replay_apply(c); return (uint16_t)c->regs_out[R_D4];
}
STUB(chgplayer) void chgplayer(uint16_t pad) { TAKE(chgplayer); ARG16(R_D4, pad, chgplayer); replay_apply(c); }
STUB(CountButtonPress) void CountButtonPress(int16_t pad, struct SortObj *o) { TAKE(CountButtonPress); ARG16(R_D4, pad, CountButtonPress); ARGPTR(R_A3, o, CountButtonPress); replay_apply(c); }
STUB(PSandSOpassdir) void PSandSOpassdir(struct SortObj *o) { TAKE(PSandSOpassdir); ARGPTR(R_A3, o, PSandSOpassdir); replay_apply(c); }
STUB(box) void box(void) { TAKE(box); replay_apply(c); }
STUB(ShowGoalBox) void ShowGoalBox(void) { TAKE(ShowGoalBox); replay_apply(c); }
STUB(PenaltyShotBox) void PenaltyShotBox(void) { TAKE(PenaltyShotBox); replay_apply(c); }
STUB(ShootoutWonBy) void ShootoutWonBy(void) { TAKE(ShootoutWonBy); replay_apply(c); }
STUB(ShowThreeStars) void ShowThreeStars(void) { TAKE(ShowThreeStars); replay_apply(c); }
STUB(GetPlayerName) uint32_t GetPlayerName(void) { TAKE_RET(GetPlayerName, 0); replay_apply(c); return c->regs_out[R_A1]; }
STUB(PushTime) uint32_t PushTime(uint16_t t) { TAKE_RET(PushTime, 0); ARG16(R_D0, t, PushTime); replay_apply(c); return c->regs_out[R_A1]; }
STUB(PushNumber) uint32_t PushNumber(uint16_t n) { TAKE_RET(PushNumber, 0); ARG16(R_D0, n, PushNumber); replay_apply(c); return c->regs_out[R_A1]; }
STUB(PushNumberWidth) uint32_t PushNumberWidth(uint16_t n, uint16_t w) { TAKE_RET(PushNumberWidth, 0); ARG16(R_D0, n, PushNumberWidth); ARG16(R_D1, w, PushNumberWidth); replay_apply(c); return c->regs_out[R_A1]; }
STUB(appendz) void appendz(const uint8_t *s, unsigned n, uint32_t buf) { TAKE(appendz); INLINE_STRING(appendz); ARG32(R_A3, buf, appendz); replay_apply(c); }
STUB(appstring) void appstring(uint32_t src, uint32_t buf) { TAKE(appstring); ARG32(R_A1, src, appstring); ARG32(R_A3, buf, appstring); replay_apply(c); }
STUB(printbig) void printbig(uint32_t str) { TAKE(printbig); ARG32(R_A1, str, printbig); replay_apply(c); }
STUB(printsmall) void printsmall(uint32_t str) { TAKE(printsmall); ARG32(R_A1, str, printsmall); replay_apply(c); }
STUB(PutTeamBlock) void PutTeamBlock(uint16_t off) { TAKE(PutTeamBlock); ARG16(R_D0, off, PutTeamBlock); replay_apply(c); }
STUB(ConvertByteToDigits) void ConvertByteToDigits(uint16_t bcd, uint32_t dst) { TAKE(ConvertByteToDigits); ARG16(R_D0, bcd, ConvertByteToDigits); ARG32(R_A1, dst, ConvertByteToDigits); replay_apply(c); }
STUB(DoFill) void DoFill(uint16_t words, uint16_t vram, uint16_t tile) { TAKE(DoFill); ARG16(R_D0, words, DoFill); ARG16(R_D1, vram, DoFill); ARG16(R_D2, tile, DoFill); replay_apply(c); }
STUB(LoadHomeTeamGfx) void LoadHomeTeamGfx(uint16_t start) { TAKE(LoadHomeTeamGfx); ARG16(R_D4, start, LoadHomeTeamGfx); replay_apply(c); }
STUB(GetShifter) uint16_t GetShifter(void) { TAKE_RET(GetShifter, 0); replay_apply(c); return (uint16_t)c->regs_out[R_D1]; }
STUB(WeightedRandomSelect) uint16_t WeightedRandomSelect(uint16_t n) { TAKE_RET(WeightedRandomSelect, 0); ARG16(R_D0, n, WeightedRandomSelect); replay_apply(c); return (uint16_t)c->regs_out[R_D0]; }
STUB(ReadCrowdRecord) void ReadCrowdRecord(uint32_t team, uint32_t buf) { TAKE(ReadCrowdRecord); ARG32(R_D1, team, ReadCrowdRecord); ARG32(R_A0, buf, ReadCrowdRecord); replay_apply(c); }
STUB(print) uint32_t print(uint32_t str) { TAKE_RET(print, str); ARG32(R_A1, str, print); replay_apply(c); return c->regs_out[R_A1]; }
STUB(FormatPlayerName) uint32_t FormatPlayerName(uint16_t player, uint8_t *tm) { TAKE_RET(FormatPlayerName, 0); ARG16(R_D0, player, FormatPlayerName); ARGPTR(R_A2, tm, FormatPlayerName); replay_apply(c); return c->regs_out[R_A1]; }
STUB(FormatPlayerNumInitial) uint32_t FormatPlayerNumInitial(uint16_t player, uint8_t *tm) { TAKE_RET(FormatPlayerNumInitial, 0); ARG16(R_D0, player, FormatPlayerNumInitial); ARGPTR(R_A2, tm, FormatPlayerNumInitial); replay_apply(c); return c->regs_out[R_A1]; }
STUB(HotColdIcon) void HotColdIcon(uint8_t *tm) { TAKE(HotColdIcon); ARGPTR(R_A2, tm, HotColdIcon); replay_apply(c); }
STUB(ShowInjuryBox) void ShowInjuryBox(void) { TAKE(ShowInjuryBox); replay_apply(c); }
STUB(ClearPenalties) void ClearPenalties(void) { TAKE(ClearPenalties); replay_apply(c); }
STUB(ExitToOpening) void ExitToOpening(void) { TAKE(ExitToOpening); replay_apply(c); }
/* video94 / sound94 / title94 / setup94 / menu94 helpers of the display and replay modules */
STUB(DoDMA) void DoDMA(uint32_t src, uint16_t words, uint16_t vram) { TAKE(DoDMA); ARG32(R_A0, src, DoDMA); ARG16(R_D0, words, DoDMA); ARG16(R_D1, vram, DoDMA); replay_apply(c); }
STUB(Vmaddr) void Vmaddr(uint16_t vram) { TAKE(Vmaddr); ARG16(R_D0, vram, Vmaddr); replay_apply(c); }
STUB(cramfade) void cramfade(void) { TAKE(cramfade); replay_apply(c); }
STUB(forceblack2) void forceblack2(void) { TAKE(forceblack2); replay_apply(c); }
STUB(p_music_vblank) void p_music_vblank(void) { TAKE(p_music_vblank); replay_apply(c); }
STUB(vcountwait) void vcountwait(void) { TAKE(vcountwait); replay_apply(c); }
STUB(ReloadRinkGraphics) void ReloadRinkGraphics(void) { TAKE(ReloadRinkGraphics); replay_apply(c); }
STUB(PlaceBoardFall) void PlaceBoardFall(struct SortObj *o) { TAKE(PlaceBoardFall); ARGPTR(R_A3, o, PlaceBoardFall); replay_apply(c); }
STUB(SetSflags8Bit1) void SetSflags8Bit1(void) { TAKE(SetSflags8Bit1); replay_apply(c); }
#define READJOY(name) STUB(name) void name(uint16_t *d0, uint16_t *d1, uint16_t *d2, uint16_t *d3) { TAKE(name); replay_apply(c); *d0 = (uint16_t)c->regs_out[R_D0]; *d1 = (uint16_t)c->regs_out[R_D1]; *d2 = (uint16_t)c->regs_out[R_D2]; *d3 = (uint16_t)c->regs_out[R_D3]; }
READJOY(ReadJoy1) READJOY(ReadJoy2) READJOY(ReadJoy3) READJOY(ReadJoy4)
