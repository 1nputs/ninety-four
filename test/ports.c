/* One adapter per ported routine: unpack the 68k registers of the recorded call into C
 * arguments, call the port, and check any register results. */
#include "game.h"
#include "ports.h"
#include "symbols_gen.h"
#include "rom.h"
#include <stdio.h>

static void run_randomd0(const Record *r)
{
    uint16_t out = randomd0((uint16_t)r->regs_in[R_D0]);
    expect_reg16(r, R_D0, out, "d0");
}

static void run_vtoa(const Record *r)
{
    uint16_t out = vtoa((int16_t)r->regs_in[R_D0], (int16_t)r->regs_in[R_D1]);
    expect_reg16(r, R_D0, out, "d0");
}

static void run_SetSPA(const Record *r)
{
    SetSPA((struct SortObj *)ram_ptr(r->regs_in[R_A3]), (uint16_t)r->regs_in[R_D1]);
}

static uint32_t be32(const uint8_t *p) { return (uint32_t)p[0] << 24 | p[1] << 16 | p[2] << 8 | p[3]; }

static void run_GetHot(const Record *r)
{
    const uint8_t *sp = r->ram_in + (r->regs_in[R_A7] & 0xFFFF);
    int16_t hx, hy;
    GetHot((const struct SortObj *)ram_ptr(be32(sp + 4)), &hx, &hy);
    expect_reg16(r, R_D0, (uint16_t)hx, "d0 (hot x)");
    expect_reg16(r, R_D1, (uint16_t)hy, "d1 (hot y)");
}

#define OBJ(reg) ((struct SortObj *)ram_ptr(r->regs_in[reg]))
static void run_checkcoll(const Record *r) { checkcoll((int16_t)r->regs_in[R_D2], (int16_t)r->regs_in[R_D3], OBJ(R_A3)); }
static void run_checkplcoll(const Record *r) { checkplcoll((int16_t)r->regs_in[R_D2], (int16_t)r->regs_in[R_D3], OBJ(R_A3)); }
static void run_checkcx(const Record *r) { checkcx((uint16_t)r->regs_in[R_D4], (int16_t)r->regs_in[R_D2], (int16_t)r->regs_in[R_D5], OBJ(R_A3)); }
static void run_newcheck(const Record *r) { newcheck(); }
static void run_checkint(const Record *r) { checkint(OBJ(R_A2), OBJ(R_A3)); }
static void run_checkcheck(const Record *r) { checkcheck(OBJ(R_A2), OBJ(R_A3), (int16_t)r->regs_in[R_D4]); }
static void run_CCStart(const Record *r) { CCStart(OBJ(R_A2), OBJ(R_A3), (int16_t)r->regs_in[R_D4]); }
static void run_checkagr(const Record *r) { expect_reg16(r, R_D0, checkagr(OBJ(R_A2), OBJ(R_A3)), "d0"); }
static void run_holdcheck(const Record *r) { holdcheck(OBJ(R_A2), OBJ(R_A3)); }
static void run_Bcheck(const Record *r) { Bcheck(OBJ(R_A2), OBJ(R_A3)); }
static void run_FallDown(const Record *r) { FallDown(OBJ(R_A2), OBJ(R_A3)); }
static void run_setInjuryType(const Record *r) { setInjuryType(OBJ(R_A2), OBJ(R_A3)); }

#define TEAM(reg) ((uint8_t *)ram_ptr(r->regs_in[reg]))
static void run_checkwallcoll(const Record *r) { checkwallcoll((int16_t)r->regs_in[R_D2], (int16_t)r->regs_in[R_D3], OBJ(R_A3)); }
static void run_checkgoal(const Record *r) { checkgoal(OBJ(R_A2), OBJ(R_A3), (int16_t)r->regs_in[R_D2], (int16_t)r->regs_in[R_D3]); }
static void run_GetPeriodTimeRemaining(const Record *r) { expect_reg16(r, R_D0, GetPeriodTimeRemaining(), "d0"); }
static void run_checkgoalp(const Record *r) { checkgoalp(OBJ(R_A2), OBJ(R_A3), (int16_t)r->regs_in[R_D2], (int16_t)r->regs_in[R_D3]); }
static void run_CheckBump(const Record *r) { CheckBump(OBJ(R_A2), OBJ(R_A3)); }
static void run_wallcollb(const Record *r) { wallcollb((int16_t)r->regs_in[R_D0], (int16_t)r->regs_in[R_D1], OBJ(R_A3)); }
static void run_wallcoll(const Record *r) { wallcoll((int16_t)r->regs_in[R_D0], (int16_t)r->regs_in[R_D1], OBJ(R_A3)); }
static void run_checkpuckcoll_sfx(const Record *r) { checkpuckcoll_sfx(); }
static void run_checkpuckcoll(const Record *r) { checkpuckcoll(OBJ(R_A3)); }
static void run_puckstick(const Record *r) { puckstick(OBJ(R_A2), OBJ(R_A3), r->regs_in[R_D0]); }
static void run_puckglue(const Record *r) { puckglue(OBJ(R_A2), OBJ(R_A3)); }
static void run_puckbody(const Record *r) { puckbody(OBJ(R_A2), OBJ(R_A3), r->regs_in[R_D0]); }
static void run_puckgoalie(const Record *r) { puckgoalie(OBJ(R_A2), OBJ(R_A3), (int16_t)r->regs_in[R_D0], (int16_t)r->regs_in[R_D1]); }
static void run_deflect(const Record *r) { deflect(OBJ(R_A3)); }
static void run_makepde(const Record *r) { expect_reg16(r, R_D0, makepde((int8_t)r->regs_in[R_D0], OBJ(R_A0)), "d0"); }
static void run_getpde(const Record *r) { expect_reg16(r, R_D0, (uint16_t)getpde(OBJ(R_A3)), "d0"); }
static void run_setpde(const Record *r) { setpde(TEAM(R_A2), (int16_t)r->regs_in[R_D1], (int16_t)r->regs_in[R_D0]); }
static void run_setpersonel(const Record *r) { setpersonel(TEAM(R_A2)); }
static void run_SetPlList(const Record *r) { SetPlList(TEAM(R_A2)); }
static void run_TryAddPlayerToList(const Record *r) { int ok = TryAddPlayerToList(TEAM(R_A2), (uint16_t)r->regs_in[R_D0], (int16_t)r->regs_in[R_D4]); expect_reg16(r, R_D1, ok ? (uint16_t)r->regs_in[R_D0] : 0, "d1"); }
static void run_SetupPenaltyShot(const Record *r) { SetupPenaltyShot(TEAM(R_A2)); }
static void run_forcepldata(const Record *r) { forcepldata(TEAM(R_A2)); }
static void run_ResetBench(const Record *r) { ResetBench(); }
static void run_Setplass(const Record *r) { Setplass(OBJ(R_A3)); }
static void run_setplayer(const Record *r) { setplayer((uint16_t)r->regs_in[R_D3], OBJ(R_A3)); }
static void run_ClampNibble(const Record *r) { expect_reg16(r, R_D3, (uint16_t)((r->regs_in[R_D3] & 0xFF00) | ClampNibble((uint8_t)r->regs_in[R_D3])) & 0xFF, "d3 (low byte)"); }


/* ---- checks94 ---- */
#define D16(reg) ((int16_t)r->regs_in[reg])
static skate_aux aux_from(uint32_t a0)
{
    if (a0 == SYM_EvadePC) return EvadePC;
    const uint8_t *code = rom_ptr(a0);
    if (code[0] == 0x4E && code[1] == 0x75) return NULL;      /* an rts: rtss2, rtss11, asspuckc's .x */
    if (a0 > SYM_asspuckc && a0 < SYM_chk4lc) return asspuckc_chkdir;
    printf("    skateto: unknown aux routine %06x\n", a0);
    return NULL;
}
#define TO(name) static void run_##name(const Record *r) { name(TEAM(R_A2), OBJ(R_A3)); }
TO(asswingo) TO(asscenterd) TO(asscentero) TO(assgoaliectrl) TO(assgoaliecpu) TO(checkanim) TO(assgoalietopuck)
TO(chkpuckc) TO(asspuckc) TO(assbreakaway) TO(assnearest) TO(asspassrec) TO(assshoot) TO(puckshootout)
TO(puckpenshot) TO(puckfaceoff) TO(puckfaceoff2) TO(pucknorm) TO(puckshadow)
#define O(name) static void run_##name(const Record *r) { name(OBJ(R_A3)); }
O(breakaway) O(BreakawayOffsidesFlagSet) O(chkpk) O(chkpk2) O(chk4shot) O(CompShoot) O(chk4pass) O(checkob) O(check4check)
O(SkateToTempTarget) O(UpdatePenaltyShotEnd) O(WaitForFaceoffLineChanges) O(updatefaceoff) O(Endfaceoff) O(puckunflip)
O(skatetopuck) O(assexit) O(dostop) O(stopna)
#define V(name) static void run_##name(const Record *r) { name(); }
V(EndPenaltyShotPlay) V(ChkGoalies) V(ReturnGoalies) V(ResetAndSelectPlayers) V(findpc)
static void run_ClampYPosition(const Record *r)
{
    int16_t d0 = D16(R_D0), d1 = D16(R_D1);
    ClampYPosition(&d0, &d1, D16(R_D3), OBJ(R_A3));
    expect_reg16(r, R_D0, (uint16_t)d0, "d0"); expect_reg16(r, R_D1, (uint16_t)d1, "d1");
}
static void run_AdjustFacingDirection(const Record *r) { AdjustFacingDirection(D16(R_D0), OBJ(R_A3)); }
static void run_goaliesave(const Record *r) { goaliesave(ram_ptr(r->regs_in[R_A0]), D16(R_D3), OBJ(R_A3)); }
static void run_chk4lc(const Record *r) { chk4lc(TEAM(R_A2), OBJ(R_A3)); }
static void run_EvadePC(const Record *r) { expect_reg16(r, R_D0, EvadePC((uint16_t)r->regs_in[R_D0], OBJ(R_A3)), "d0"); }
static void run_SelectPenaltyShotSkater(const Record *r) { SelectPenaltyShotSkater(); }
static void run_SetFaceoffComputerLine(const Record *r) { SetFaceoffComputerLine((uint16_t)r->regs_in[R_D0], TEAM(R_A2), TEAM(R_A1)); }
static void run_StartFaceoffLineChange(const Record *r) { StartFaceoffLineChange((uint16_t)r->regs_in[R_D0], OBJ(R_A3)); }
static void run_UpdateFaceoffLineChangeTimer(const Record *r) { UpdateFaceoffLineChangeTimer((uint16_t)r->regs_in[R_D0], OBJ(R_A3)); }
static void run_CPgoalie(const Record *r) { CPgoalie(D16(R_D1), TEAM(R_A1), TEAM(R_A2)); }
static void run_CompLine(const Record *r) { CompLine(TEAM(R_A2), TEAM(R_A1)); }
static void run_ChkOffsides(const Record *r) { ChkOffsides(D16(R_D0), OBJ(R_A3)); }
static void run_ClearOffsidesIfAllPlayers(const Record *r) { int16_t d0 = D16(R_D0); ClearOffsidesIfAllPlayers(TEAM(R_A2), &d0); expect_reg16(r, R_D0, (uint16_t)d0, "d0"); }
static void run_a2offsides(const Record *r) { a2offsides(OBJ(R_A2), OBJ(R_A3)); }
static void run_a2touchpuck(const Record *r) { a2touchpuck(OBJ(R_A2), OBJ(R_A3)); }
static void run_puckIChk(const Record *r) { expect_reg16(r, R_D0, (uint16_t)puckIChk(D16(R_D0)), "d0"); }
static void run_puckflip(const Record *r) { puckflip(D16(R_D0), OBJ(R_A3)); }
static void run_skateto(const Record *r) { skateto(D16(R_D0), D16(R_D1), aux_from(r->regs_in[R_A0]), OBJ(R_A3)); }
static void run_avdgoal(const Record *r)
{
    int16_t x = D16(R_D0), y = D16(R_D1);
    avdgoal(&x, &y, OBJ(R_A3));
    expect_reg16(r, R_D0, (uint16_t)x, "d0"); expect_reg16(r, R_D1, (uint16_t)y, "d1");
}
static void run_skatetopuckinit(const Record *r)
{
    int16_t x, y;
    skatetopuckinit(&x, &y);
    expect_reg16(r, R_D0, (uint16_t)x, "d0"); expect_reg16(r, R_D1, (uint16_t)y, "d1");
}
static void run_assinsert(const Record *r) { assinsert((uint16_t)r->regs_in[R_D0], OBJ(R_A3)); }
static void run_assreplace(const Record *r) { assreplace((uint16_t)r->regs_in[R_D0], OBJ(R_A3)); }
static void run_doplayeracc(const Record *r) { doplayeracc((uint16_t)r->regs_in[R_D0], OBJ(R_A3)); }
static void run_goalieacc(const Record *r) { goalieacc((uint16_t)r->regs_in[R_D0], OBJ(R_A3)); }
static void run_noturn0(const Record *r) { noturn0((uint16_t)r->regs_in[R_D0], D16(R_D2), OBJ(R_A3)); }
static void run_noturn(const Record *r) { noturn(D16(R_D2), OBJ(R_A3)); }
static void run_playeracc(const Record *r) { playeracc(D16(R_D2), OBJ(R_A3)); }

/* ---- assign94 ---- */
TO(assbench) TO(asseben) TO(asspenalty) TO(assdopen) TO(assepen) TO(assgoaliebreakwait) TO(assfaceoff) TO(assfaceoffpl)
TO(assfight) TO(assfwatch) TO(assstanley) TO(assscore) TO(assdefo) TO(assdefd) TO(asswingd)
static void run_check4bench(const Record *r) { check4bench(OBJ(R_A3)); }
O(donothing)

/* ---- input94 ---- */
static void run_doinput(const Record *r) { doinput((uint16_t)r->regs_in[R_D0], (uint16_t)r->regs_in[R_D1], (uint16_t)r->regs_in[R_D2], (uint16_t)r->regs_in[R_D3], D16(R_D4), OBJ(R_A3)); }
static void run_ClampTargetY(const Record *r)
{
    int16_t d0 = D16(R_D0), d1 = D16(R_D1);
    ClampTargetY(&d0, &d1, D16(R_D3), OBJ(R_A3));
    expect_reg16(r, R_D0, (uint16_t)d0, "d0"); expect_reg16(r, R_D1, (uint16_t)d1, "d1");
}
static void run_getGoalieSCnum(const Record *r) { expect_reg16(r, R_D0, (uint16_t)getGoalieSCnum(D16(R_D0)), "d0"); }
static void run_SetLCmode(const Record *r) { SetLCmode(OBJ(R_A3)); }
static void run_SetLCmode2(const Record *r) { SetLCmode2(TEAM(R_A2)); }
static void run_getlchoice(const Record *r) { expect_reg16(r, R_D0, (uint16_t)getlchoice(D16(R_D0), TEAM(R_A2)), "d0"); }
static void run_lcfound(const Record *r) { lcfound((uint16_t)r->regs_in[R_D2], TEAM(R_A2), OBJ(R_A3)); }
O(burst) O(dopass) O(Sweepcheck) O(SetShotMode)
static void run_Acheck(const Record *r) { Acheck(OBJ(R_A0), OBJ(R_A3)); }
static void run_changeplayer(const Record *r) { changeplayer((uint16_t)r->regs_in[R_D4]); }
static void run_setc1player(const Record *r) { setc1player((uint16_t)r->regs_in[R_D0]); }
static void run_setc2player(const Record *r) { setc2player((uint16_t)r->regs_in[R_D0]); }
static void run_Findhittype(const Record *r) { int z = Findhittype((uint16_t)r->regs_in[R_D0], OBJ(R_A3)); if (z != (int)((r->regs_out[R_SR] >> 2) & 1)) { printf("    Z flag: got %d, 68k %d\n", z, (int)((r->regs_out[R_SR] >> 2) & 1)); } }
static void run_ShotMode(const Record *r) { ShotMode((uint16_t)r->regs_in[R_D0], (uint16_t)r->regs_in[R_D2], OBJ(R_A3)); }
static void run_setpads(const Record *r) { setpads(D16(R_D4), OBJ(R_A3)); }
static void run_loadTeamStruct(const Record *r) { uint8_t *a1; uint8_t *a2 = loadTeamStruct(OBJ(R_A3), &a1); expect_reg16(r, R_A2, (uint16_t)(a2 - (uint8_t *)&g), "a2"); expect_reg16(r, R_A1, (uint16_t)(a1 - (uint8_t *)&g), "a1"); }

static void run_doinput_cbut(const Record *r) { doinput_cbut((uint16_t)r->regs_in[R_D1], (uint16_t)r->regs_in[R_D3], OBJ(R_A3)); }
O(doinput_chkanim)
static void run_doinput_onetimer(const Record *r) { doinput_onetimer((uint16_t)r->regs_in[R_D1], D16(R_D4), OBJ(R_A3)); }
static void run_doinput_ispc(const Record *r) { doinput_ispc((uint16_t)r->regs_in[R_D0], (uint16_t)r->regs_in[R_D1], (uint16_t)r->regs_in[R_D2], (uint16_t)r->regs_in[R_D3], D16(R_D4), OBJ(R_A3)); }
static void run_doinput_islocked(const Record *r) { doinput_islocked((uint16_t)r->regs_in[R_D1], D16(R_D4), OBJ(R_A3)); }
O(setpassmode) O(holdplayer) O(prepshot) O(doshot) O(shotdiradj)
static void run_passmode(const Record *r) { passmode((uint16_t)r->regs_in[R_D0], (uint16_t)r->regs_in[R_D2], OBJ(R_A3)); }
static void run_faceoffinput(const Record *r) { faceoffinput((uint16_t)r->regs_in[R_D0], (uint16_t)r->regs_in[R_D1], OBJ(R_A3)); }
static void run_lineinput(const Record *r) { lineinput((uint16_t)r->regs_in[R_D0], (uint16_t)r->regs_in[R_D1], (uint16_t)r->regs_in[R_D2], OBJ(R_A3)); }
static void run_lcfound2(const Record *r) { lcfound2(TEAM(R_A2)); }
static void run_setlccords(const Record *r) { uint16_t d0, d1; setlccords(TEAM(R_A2), &d0, &d1); expect_reg16(r, R_D0, d0, "d0"); expect_reg16(r, R_D1, d1, "d1"); }
static void run_getlchoice2(const Record *r) { expect_reg16(r, R_D0, (uint16_t)getlchoice2(D16(R_D0), D16(R_D2), TEAM(R_A2)), "d0"); }
static void run_passto(const Record *r) { passto(OBJ(R_A0), OBJ(R_A3)); }
static void run_passtoa0(const Record *r) { passtoa0(OBJ(R_A0), OBJ(R_A3)); }
static void run_restorepl(const Record *r) { expect_reg16(r, R_D0, (uint16_t)restorepl(D16(R_D1), D16(R_D0)), "d0"); }
static void run_CalcAngleOffset(const Record *r) { expect_reg16(r, R_D4, (uint16_t)CalcAngleOffset(D16(R_D0), D16(R_D1), D16(R_D2), D16(R_D3), D16(R_D4)), "d4"); }

/* ---- penalty94 ---- */
static void run_AddPenalty(const Record *r) { AddPenalty((uint16_t)r->regs_in[R_D0], OBJ(R_A3)); }
static void run_AddPenalty2(const Record *r) { AddPenalty2((uint16_t)r->regs_in[R_D0], OBJ(R_A3)); }
V(PenaltyManager) V(chkprogress) V(InProgress) V(checkfornewpen) V(limitfo) V(UpdatePA) V(SetPA2) V(ClearPenaltyMessage)
V(ClearPenaltyBuffer) V(updatepentime) V(chkatop) V(updatepwrplay) V(ClrHor) V(SetHor) V(printscores1) V(EASNLogo) V(USBoard)
V(ChkShotStat) V(SetupTeamForIntermission) V(InitScores) V(UpdateScores) V(ClearTickerArea)
static void run_PenShotChk(const Record *r) { expect_reg16(r, R_D0, PenShotChk((uint16_t)r->regs_in[R_D0], OBJ(R_A2), OBJ(R_A3)), "d0"); }
static void run_getBAplayerInfo(const Record *r) { uint8_t ccr = getBAplayerInfo((uint16_t)r->regs_in[R_D0], OBJ(R_A3)); if ((ccr & CCR_N) != (r->regs_out[R_SR] & CCR_N)) printf("    N flag: got %d, 68k %d\n", !!(ccr & CCR_N), !!(r->regs_out[R_SR] & CCR_N)); }
static void run_coinsearch(const Record *r) { coinsearch(TEAM(R_A2)); }
static void run_Stop4Pen(const Record *r) { uint32_t a0 = r->regs_in[R_A0]; Stop4Pen((a0 & 0xFF0000) == 0xFF0000 ? ram_ptr(a0) : rom_ptr(a0), OBJ(R_A3)); }
static void run_SetPA(const Record *r) { SetPA((uint16_t)r->regs_in[R_D0]); }
static void run_PushRef(const Record *r) { PushRef((uint16_t)r->regs_in[R_D0]); }
static void run_prefmes(const Record *r) { prefmes(D16(R_D0)); }
static void run_PenGoalStuff(const Record *r) { PenGoalStuff(TEAM(R_A1), TEAM(R_A2)); }
static void run_ProcessPenaltyList(const Record *r) { ProcessPenaltyList(TEAM(R_A2)); }
static void run_RemovePlayerFromList(const Record *r) { uint8_t *a0 = RemovePlayerFromList(ram_ptr(r->regs_in[R_A0])); expect_reg16(r, R_A0, (uint16_t)(a0 - (uint8_t *)&g), "a0"); }
static void run_releasepl(const Record *r) { releasepl(TEAM(R_A2), (uint16_t)r->regs_in[R_D0]); }
static void run_GetLowestPen(const Record *r) { expect_reg16(r, R_D0, GetLowestPen(TEAM(R_A2), TEAM(R_A3)), "d0"); }
static void run_PrintTeamNameAndScore(const Record *r) { PrintTeamNameAndScore(TEAM(R_A2)); }
static void run_PrintTeamLogoAndScore(const Record *r) { PrintTeamLogoAndScore(TEAM(R_A2), (uint16_t)r->regs_in[R_D0]); }
static void run_pplpen(const Record *r) { pplpen(TEAM(R_A2), (uint16_t)r->regs_in[R_D0]); }
static void run_linebar(const Record *r) { linebar((uint16_t)r->regs_in[R_D0], TEAM(R_A2)); }
static void run_getlinee(const Record *r) { expect_reg16(r, R_D0, getlinee((uint16_t)r->regs_in[R_D0], TEAM(R_A2)), "d0"); }
static void run_AvgCline(const Record *r) { expect_reg16(r, R_D0, AvgCline(TEAM(R_A2)), "d0"); }
static void run_RestoreTeamEnergy(const Record *r) { RestoreTeamEnergy(TEAM(R_A2)); }
static void run_reenergizeteam(const Record *r) { reenergizeteam(TEAM(R_A2)); }
static void run_SetScore(const Record *r) { SetScore(ram_ptr(r->regs_in[R_A0])); }
static void run_NewTicker(const Record *r) { const uint8_t *gs = NULL; expect_reg16(r, R_D3, (uint16_t)NewTicker(&gs), "d3"); }
static void run_NewTicker2(const Record *r) { const uint8_t *gs = NULL; expect_reg16(r, R_D3, (uint16_t)NewTicker2(&gs), "d3"); }
static void run_CheckGameTickerStatus(const Record *r) { const uint8_t *gs = NULL; expect_reg16(r, R_D3, (uint16_t)CheckGameTickerStatus(D16(R_D3), &gs), "d3"); if (gs) expect_reg16(r, R_A0, (uint16_t)(gs - (const uint8_t *)&g), "a0"); }
static void run_NewTicker3(const Record *r) { NewTicker3(D16(R_D3), ram_ptr(r->regs_in[R_A0])); }
static void run_SetTickerAreaPosition(const Record *r) { uint16_t d0, d1; SetTickerAreaPosition(&d0, &d1); expect_reg16(r, R_D0, d0, "d0"); expect_reg16(r, R_D1, d1, "d1"); }
static void run_PrintStringFromList(const Record *r) { PrintStringFromList(r->regs_in[R_A1], (uint16_t)r->regs_in[R_D0]); }
static void run_AdvanceStringPtr(const Record *r) { uint32_t a1 = AdvanceStringPtr(r->regs_in[R_A1], (uint16_t)r->regs_in[R_D0]); if (a1 != r->regs_out[R_A1]) printf("    a1: got %06x, 68k %06x\n", a1, r->regs_out[R_A1]); }

/* ---- goalie94, onetimer94, crowd94 ---- */
V(ManualGoalieMenu) V(RunArenaAnim) V(NextArenaFrame) V(SetCameraTop) V(EndArenaAnim) V(LoadCrowdRec) V(Crowd_Noise) V(BuildHotColdLists)
static void run_StartArenaAnim(const Record *r) { StartArenaAnim((uint16_t)r->regs_in[R_D0]); }
static void run_SetFaceoffAnim(const Record *r) { SetFaceoffAnim((uint16_t)r->regs_in[R_D0]); }
O(puckvzadj) O(OneTimerPass) O(OneTimerTarget) O(stopna2)
TO(assonetimer)
static void run_setonetimeranim(const Record *r) { expect_reg16(r, R_D1, setonetimeranim(OBJ(R_A3)), "d1"); }
static void run_PuckOnAttackHalf(const Record *r) { int z = PuckOnAttackHalf(OBJ(R_A3)); if (z != (int)((r->regs_out[R_SR] >> 2) & 1)) printf("    Z flag: got %d, 68k %d\n", z, (int)((r->regs_out[R_SR] >> 2) & 1)); }
static void run_onetimershot(const Record *r) { onetimershot(OBJ(R_A2), OBJ(R_A3)); }
static void run_CheckOneTimerFacing(const Record *r) { int z = CheckOneTimerFacing((uint16_t)r->regs_in[R_D0], OBJ(R_A3)); if (z != (int)((r->regs_out[R_SR] >> 2) & 1)) printf("    Z flag: got %d, 68k %d\n", z, (int)((r->regs_out[R_SR] >> 2) & 1)); }
static void run_checkcornercoll(const Record *r) { checkcornercoll(D16(R_D2), D16(R_D3), OBJ(R_A3)); }
static void run_HotColdLoop(const Record *r) { HotColdLoop(TEAM(R_A0), D16(R_D1)); }
static void run_Create_HotCold_Table(const Record *r) { Create_HotCold_Table(TEAM(R_A0)); }
static void run_Clamp0to100(const Record *r) { expect_reg16(r, R_D3, Clamp0to100(D16(R_D3)), "d3"); }
static void run_AttributeCalc(const Record *r) { expect_reg16(r, R_D3, (uint16_t)((r->regs_in[R_D3] & 0xFF00) | AttributeCalc((uint8_t)r->regs_in[R_D3], OBJ(R_A3))), "d3"); }
#define HOTCOLD(name) static void run_##name(const Record *r) { uint8_t *tm; expect_reg16(r, R_D1, name(&tm), "d1"); expect_reg16(r, R_A1, (uint16_t)(tm - (uint8_t *)&g), "a1"); }
HOTCOLD(NextHomeHotPlayer) HOTCOLD(NextAwayHotPlayer) HOTCOLD(NextHomeColdPlayer) HOTCOLD(NextAwayColdPlayer)
static void run_SortHotColdStarters(const Record *r) { SortHotColdStarters(TEAM(R_A0)); }
static void run_CopyHottestPlayer(const Record *r) { CopyHottestPlayer(ram_ptr(r->regs_in[R_A0])); }
static void run_CopyColdestPlayer(const Record *r) { CopyColdestPlayer(ram_ptr(r->regs_in[R_A0])); }
static void run_CompareHotColdTotals(const Record *r) { expect_reg16(r, R_D0, CompareHotColdTotals(), "d0"); }
static void run_GetHotColdTotal(const Record *r) { expect_reg16(r, R_D1, GetHotColdTotal(TEAM(R_A0)), "d1"); }

/* ---- period94, hockey94, display94 ---- */
V(EndShootout) V(ClearGameStats) V(SetStatScroll) V(PrintStatScrollArrows) V(DisplayTeamStatsScreen) V(updatePPTeamTime)
V(ClearShotData) V(restoreteams) V(ResetClock) V(periodicevents) V(CheckInjury) V(RestoreBenchEnergy) V(CheckLateGameSong)
V(UpdateCwdExcite) V(updatecrowdf) V(clockcont) V(clockcont_0) V(demoread) V(startpause) V(startpause1) V(startpause2) V(startpause3)
V(startpause4) V(SetupPauseScreen) V(updatesound) V(KillCrowd)
O(ClampPositionBox) O(ShortenMsgTimer)
static void run_AddArenaAnimSprite(const Record *r) { uint8_t *sat = ram_ptr(r->regs_in[R_A6]); uint16_t link = (uint16_t)r->regs_in[R_D6]; AddArenaAnimSprite(&sat, &link); expect_reg16(r, R_A6, (uint16_t)(sat - (uint8_t *)&g), "a6"); expect_reg16(r, R_D6, link, "d6"); }
static void run_PrintPlayerNameRight(const Record *r) { PrintPlayerNameRight((uint16_t)r->regs_in[R_D0], TEAM(R_A2)); }
static void run_SkipNameNumber(const Record *r) { uint32_t a1 = SkipNameNumber((uint16_t)r->regs_in[R_D0], TEAM(R_A2)); expect_reg16(r, R_A1, (uint16_t)a1, "a1"); }
static void run_StatScrollRow(const Record *r) { expect_reg16(r, R_D3, StatScrollRow(), "d3"); }
static void run_StatScrollRowEnd(const Record *r) { expect_reg16(r, R_D3, StatScrollRowEnd(), "d3"); }
static void run_PrintStatValue(const Record *r) { PrintStatValue(rom_ptr(r->regs_in[R_A0]), TEAM(R_A2)); }
static void run_GetTeamRating(const Record *r) { expect_reg16(r, R_D0, GetTeamRating(TEAM(R_A2)), "d0"); }
static void run_GetPeriodTime(const Record *r) { expect_reg16(r, R_D0, GetPeriodTime(), "d0"); }
static void run_EndDemoOnPress(const Record *r) { EndDemoOnPress((uint16_t)r->regs_in[R_D1]); }
static void run_seta2(const Record *r) { expect_reg16(r, R_A2, (uint16_t)(seta2() - (uint8_t *)&g), "a2"); }

/* display94: the sprite builders take a5 / a6 / d6 in a struct Vid and leave them advanced */
#define VID_IN(v) struct Vid v = { ram_ptr(r->regs_in[R_A5]), ram_ptr(r->regs_in[R_A6]), (uint16_t)r->regs_in[R_D6] }
#define VID_OUT(v) expect_reg16(r, R_A5, (uint16_t)(v.dma - (uint8_t *)&g), "a5"); expect_reg16(r, R_A6, (uint16_t)(v.sat - (uint8_t *)&g), "a6"); expect_reg16(r, R_D6, v.link, "d6")
#define VID_RUN(name) static void run_##name(const Record *r) { VID_IN(v); name(&v); VID_OUT(v); }
static void run_VBlank(const Record *r) { VBlank(); }
static void run_vb2(const Record *r) { vb2(); }
static void run_DumpSprites(const Record *r) { DumpSprites(); }
static void run_DumpSprites2(const Record *r) { DumpSprites2(); }
static void run_DoDMAList(const Record *r) { DoDMAList(); }
static void run_SetScroll2(const Record *r) { SetScroll2(); }
static void run_setvideo(const Record *r) { setvideo(); }
VID_RUN(updatescroll) VID_RUN(showref) VID_RUN(checkfo) VID_RUN(showzam) VID_RUN(showcrowd) VID_RUN(showclock)
VID_RUN(checksso) VID_RUN(setsortcords) VID_RUN(setffo) VID_RUN(uppads)
static void run_SetSframe(const Record *r) { VID_IN(v); SetSframe(r->regs_in[R_A0], D16(R_D0), D16(R_D1), (uint16_t)r->regs_in[R_D2], (uint16_t)r->regs_in[R_D3], &v); VID_OUT(v); }
static void run_QueuePadLabel(const Record *r) { VID_IN(v); QueuePadLabel((uint16_t)r->regs_in[R_D1], ram_ptr(r->regs_in[R_A0]), &v); VID_OUT(v); }
static void run_RenderSmallFontChar(const Record *r) { VID_IN(v); RenderSmallFontChar((uint16_t)r->regs_in[R_D2], (uint16_t)r->regs_in[R_D0], ram_ptr(r->regs_in[R_A0]), &v); VID_OUT(v); }
static void run_addframe(const Record *r) { VID_IN(v); addframe(OBJ(R_A3), &v); VID_OUT(v); }
static void run_addframe2(const Record *r) { VID_IN(v); addframe2(D16(R_D0), D16(R_D1), OBJ(R_A3), &v); VID_OUT(v); }
static void run_find3d(const Record *r) { int16_t d0 = D16(R_D0), d1 = D16(R_D1); find3d(&d0, &d1, D16(R_D2)); expect_reg16(r, R_D0, (uint16_t)d0, "d0"); expect_reg16(r, R_D1, (uint16_t)d1, "d1"); }
/* replay94 */
static void run_getpzjoy(const Record *r) { uint16_t d0, d1, d2, d3; getpzjoy(&d0, &d1, &d2, &d3); expect_reg16(r, R_D0, d0, "d0"); expect_reg16(r, R_D1, d1, "d1"); expect_reg16(r, R_D2, d2, "d2"); expect_reg16(r, R_D3, d3, "d3"); }
static void run_ShowReplayIcon(const Record *r) { ShowReplayIcon(); }
static void run_EraseReplayIcon(const Record *r) { EraseReplayIcon(); }
static void run_ShowReplayBanner(const Record *r) { ShowReplayBanner(); }
static void run_EraseReplayBanner(const Record *r) { EraseReplayBanner(); }
#define A4_RUN(name) static void run_##name(const Record *r) { uint32_t a4 = r->regs_in[R_A4]; uint16_t d7 = name(&a4, ram_ptr(r->regs_in[R_A5])); expect_reg16(r, R_A4, (uint16_t)a4, "a4"); expect_reg16(r, R_D7, d7, "d7"); }
A4_RUN(suba4) A4_RUN(adda4) A4_RUN(adda42)
static void run_adda43(const Record *r) { uint32_t a4 = r->regs_in[R_A4]; adda43(&a4); expect_reg16(r, R_A4, (uint16_t)a4, "a4"); }
static void run_UpdateCameraPos(const Record *r) { UpdateCameraPos(OBJ(R_A3), ram_ptr(r->regs_in[R_A5])); }
static void run_RevReplayAdj(const Record *r) { expect_reg16(r, R_D7, RevReplayAdj(r->regs_in[R_A4], ram_ptr(r->regs_in[R_A5])), "d7"); }
static void run_RestoreReplayFrame(const Record *r) { expect_reg16(r, R_D7, RestoreReplayFrame(r->regs_in[R_A4], ram_ptr(r->regs_in[R_A5])), "d7"); }
static void run_ClampReplayView(const Record *r) { ClampReplayView(); }
static void run_updatereplay(const Record *r) { updatereplay(); }
static void run_updateplayers(const Record *r) { updateplayers(); }
static void run_updateanim(const Record *r) { updateanim(OBJ(R_A3)); }
static void run_freezewindow(const Record *r) { freezewindow(); }
static void run_checkwindow(const Record *r) { checkwindow(); }

const Port ports[] = {
    { SYM_checkwallcoll, "checkwallcoll", run_checkwallcoll },
    { SYM_checkgoal, "checkgoal", run_checkgoal },
    { SYM_GetPeriodTimeRemaining, "GetPeriodTimeRemaining", run_GetPeriodTimeRemaining },
    { SYM_checkgoalp, "checkgoalp", run_checkgoalp },
    { SYM_CheckBump, "CheckBump", run_CheckBump },
    { SYM_wallcollb, "wallcollb", run_wallcollb },
    { SYM_wallcoll, "wallcoll", run_wallcoll },
    { SYM_checkpuckcoll_sfx, "checkpuckcoll_sfx", run_checkpuckcoll_sfx },
    { SYM_checkpuckcoll, "checkpuckcoll", run_checkpuckcoll },
    { SYM_puckstick, "puckstick", run_puckstick },
    { SYM_puckglue, "puckglue", run_puckglue },
    { SYM_puckbody, "puckbody", run_puckbody },
    { SYM_puckgoalie, "puckgoalie", run_puckgoalie },
    { SYM_deflect, "deflect", run_deflect },
    { SYM_makepde, "makepde", run_makepde },
    { SYM_getpde, "getpde", run_getpde },
    { SYM_setpde, "setpde", run_setpde },
    { SYM_setpersonel, "setpersonel", run_setpersonel },
    { SYM_SetPlList, "SetPlList", run_SetPlList },
    { SYM_TryAddPlayerToList, "TryAddPlayerToList", run_TryAddPlayerToList },
    { SYM_SetupPenaltyShot, "SetupPenaltyShot", run_SetupPenaltyShot },
    { SYM_forcepldata, "forcepldata", run_forcepldata },
    { SYM_ResetBench, "ResetBench", run_ResetBench },
    { SYM_Setplass, "Setplass", run_Setplass },
    { SYM_setplayer, "setplayer", run_setplayer },
    { SYM_ClampNibble, "ClampNibble", run_ClampNibble },
    { SYM_checkcoll, "checkcoll", run_checkcoll },
    { SYM_checkplcoll, "checkplcoll", run_checkplcoll },
    { SYM_checkcx, "checkcx", run_checkcx },
    { SYM_newcheck, "newcheck", run_newcheck },
    { SYM_checkint, "checkint", run_checkint },
    { SYM_checkcheck, "checkcheck", run_checkcheck },
    { SYM_CCStart, "CCStart", run_CCStart },
    { SYM_checkagr, "checkagr", run_checkagr },
    { SYM_holdcheck, "holdcheck", run_holdcheck },
    { SYM_Bcheck, "Bcheck", run_Bcheck },
    { SYM_FallDown, "FallDown", run_FallDown },
    { SYM_setInjuryType, "setInjuryType", run_setInjuryType },
    { SYM_randomd0, "randomd0", run_randomd0 },
    { SYM_vtoa, "vtoa", run_vtoa },
    { SYM_SetSPA, "SetSPA", run_SetSPA },
    { SYM_GetHot, "GetHot", run_GetHot },
    { SYM_asswingo, "asswingo", run_asswingo },
    { SYM_asscenterd, "asscenterd", run_asscenterd },
    { SYM_asscentero, "asscentero", run_asscentero },
    { SYM_assgoaliectrl, "assgoaliectrl", run_assgoaliectrl },
    { SYM_assgoaliecpu, "assgoaliecpu", run_assgoaliecpu },
    { SYM_checkanim, "checkanim", run_checkanim },
    { SYM_ClampYPosition, "ClampYPosition", run_ClampYPosition },
    { SYM_AdjustFacingDirection, "AdjustFacingDirection", run_AdjustFacingDirection },
    { SYM_goaliesave, "goaliesave", run_goaliesave },
    { SYM_assgoalietopuck, "assgoalietopuck", run_assgoalietopuck },
    { SYM_breakaway, "breakaway", run_breakaway },
    { SYM_BreakawayOffsidesFlagSet, "BreakawayOffsidesFlagSet", run_BreakawayOffsidesFlagSet },
    { SYM_chkpuckc, "chkpuckc", run_chkpuckc },
    { SYM_asspuckc, "asspuckc", run_asspuckc },
    { SYM_chk4lc, "chk4lc", run_chk4lc },
    { SYM_chkpk, "chkpk", run_chkpk },
    { SYM_chkpk2, "chkpk2", run_chkpk2 },
    { SYM_chk4shot, "chk4shot", run_chk4shot },
    { SYM_CompShoot, "CompShoot", run_CompShoot },
    { SYM_chk4pass, "chk4pass", run_chk4pass },
    { SYM_EvadePC, "EvadePC", run_EvadePC },
    { SYM_checkob, "checkob", run_checkob },
    { SYM_assbreakaway, "assbreakaway", run_assbreakaway },
    { SYM_assnearest, "assnearest", run_assnearest },
    { SYM_check4check, "check4check", run_check4check },
    { SYM_asspassrec, "asspassrec", run_asspassrec },
    { SYM_SkateToTempTarget, "SkateToTempTarget", run_SkateToTempTarget },
    { SYM_assshoot, "assshoot", run_assshoot },
    { SYM_puckshootout, "puckshootout", run_puckshootout },
    { SYM_SelectPenaltyShotSkater, "SelectPenaltyShotSkater", run_SelectPenaltyShotSkater },
    { SYM_puckpenshot, "puckpenshot", run_puckpenshot },
    { SYM_UpdatePenaltyShotEnd, "UpdatePenaltyShotEnd", run_UpdatePenaltyShotEnd },
    { SYM_EndPenaltyShotPlay, "EndPenaltyShotPlay", run_EndPenaltyShotPlay },
    { SYM_puckfaceoff, "puckfaceoff", run_puckfaceoff },
    { SYM_SetFaceoffComputerLine, "SetFaceoffComputerLine", run_SetFaceoffComputerLine },
    { SYM_StartFaceoffLineChange, "StartFaceoffLineChange", run_StartFaceoffLineChange },
    { SYM_WaitForFaceoffLineChanges, "WaitForFaceoffLineChanges", run_WaitForFaceoffLineChanges },
    { SYM_UpdateFaceoffLineChangeTimer, "UpdateFaceoffLineChangeTimer", run_UpdateFaceoffLineChangeTimer },
    { SYM_ChkGoalies, "ChkGoalies", run_ChkGoalies },
    { SYM_ReturnGoalies, "ReturnGoalies", run_ReturnGoalies },
    { SYM_CPgoalie, "CPgoalie", run_CPgoalie },
    { SYM_CompLine, "CompLine", run_CompLine },
    { SYM_puckfaceoff2, "puckfaceoff2", run_puckfaceoff2 },
    { SYM_ResetAndSelectPlayers, "ResetAndSelectPlayers", run_ResetAndSelectPlayers },
    { SYM_updatefaceoff, "updatefaceoff", run_updatefaceoff },
    { SYM_Endfaceoff, "Endfaceoff", run_Endfaceoff },
    { SYM_pucknorm, "pucknorm", run_pucknorm },
    { SYM_ChkOffsides, "ChkOffsides", run_ChkOffsides },
    { SYM_ClearOffsidesIfAllPlayers, "ClearOffsidesIfAllPlayers", run_ClearOffsidesIfAllPlayers },
    { SYM_a2offsides, "a2offsides", run_a2offsides },
    { SYM_a2touchpuck, "a2touchpuck", run_a2touchpuck },
    { SYM_puckIChk, "puckIChk", run_puckIChk },
    { SYM_puckunflip, "puckunflip", run_puckunflip },
    { SYM_puckflip, "puckflip", run_puckflip },
    { SYM_puckshadow, "puckshadow", run_puckshadow },
    { SYM_findpc, "findpc", run_findpc },
    { SYM_skateto, "skateto", run_skateto },
    { SYM_avdgoal, "avdgoal", run_avdgoal },
    { SYM_skatetopuckinit, "skatetopuckinit", run_skatetopuckinit },
    { SYM_skatetopuck, "skatetopuck", run_skatetopuck },
    { SYM_assexit, "assexit", run_assexit },
    { SYM_assinsert, "assinsert", run_assinsert },
    { SYM_assreplace, "assreplace", run_assreplace },
    { SYM_doplayeracc, "doplayeracc", run_doplayeracc },
    { SYM_goalieacc, "goalieacc", run_goalieacc },
    { SYM_noturn0, "noturn0", run_noturn0 },
    { SYM_noturn, "noturn", run_noturn },
    { SYM_playeracc, "playeracc", run_playeracc },
    { SYM_dostop, "dostop", run_dostop },
    { SYM_stopna, "stopna", run_stopna },
    { SYM_check4bench, "check4bench", run_check4bench },
    { SYM_assbench, "assbench", run_assbench },
    { SYM_asseben, "asseben", run_asseben },
    { SYM_asspenalty, "asspenalty", run_asspenalty },
    { SYM_assdopen, "assdopen", run_assdopen },
    { SYM_assepen, "assepen", run_assepen },
    { SYM_assgoaliebreakwait, "assgoaliebreakwait", run_assgoaliebreakwait },
    { SYM_assfaceoff, "assfaceoff", run_assfaceoff },
    { SYM_assfaceoffpl, "assfaceoffpl", run_assfaceoffpl },
    { SYM_assfight, "assfight", run_assfight },
    { SYM_assfwatch, "assfwatch", run_assfwatch },
    { SYM_donothing, "donothing", run_donothing },
    { SYM_assstanley, "assstanley", run_assstanley },
    { SYM_assscore, "assscore", run_assscore },
    { SYM_assdefo, "assdefo", run_assdefo },
    { SYM_assdefd, "assdefd", run_assdefd },
    { SYM_asswingd, "asswingd", run_asswingd },
    { SYM_doinput, "doinput", run_doinput },
    { SYM_ClampTargetY, "ClampTargetY", run_ClampTargetY },
    { SYM_getGoalieSCnum, "getGoalieSCnum", run_getGoalieSCnum },
    { SYM_SetLCmode, "SetLCmode", run_SetLCmode },
    { SYM_SetLCmode2, "SetLCmode2", run_SetLCmode2 },
    { SYM_getlchoice, "getlchoice", run_getlchoice },
    { SYM_lcfound, "lcfound", run_lcfound },
    { SYM_burst, "burst", run_burst },
    { SYM_dopass, "dopass", run_dopass },
    { SYM_Sweepcheck, "Sweepcheck", run_Sweepcheck },
    { SYM_SetShotMode, "SetShotMode", run_SetShotMode },
    { SYM_Acheck, "Acheck", run_Acheck },
    { SYM_changeplayer, "changeplayer", run_changeplayer },
    { SYM_setc1player, "setc1player", run_setc1player },
    { SYM_setc2player, "setc2player", run_setc2player },
    { SYM_Findhittype, "Findhittype", run_Findhittype },
    { SYM_ShotMode, "ShotMode", run_ShotMode },
    { SYM_setpads, "setpads", run_setpads },
    { SYM_loadTeamStruct, "loadTeamStruct", run_loadTeamStruct },
    { SYM_doinput_cbut, "doinput_cbut", run_doinput_cbut },
    { SYM_doinput_chkanim, "doinput_chkanim", run_doinput_chkanim },
    { SYM_doinput_onetimer, "doinput_onetimer", run_doinput_onetimer },
    { SYM_doinput_ispc, "doinput_ispc", run_doinput_ispc },
    { SYM_doinput_islocked, "doinput_islocked", run_doinput_islocked },
    { SYM_setpassmode, "setpassmode", run_setpassmode },
    { SYM_passmode, "passmode", run_passmode },
    { SYM_holdplayer, "holdplayer", run_holdplayer },
    { SYM_faceoffinput, "faceoffinput", run_faceoffinput },
    { SYM_lineinput, "lineinput", run_lineinput },
    { SYM_lcfound2, "lcfound2", run_lcfound2 },
    { SYM_setlccords, "setlccords", run_setlccords },
    { SYM_getlchoice2, "getlchoice2", run_getlchoice2 },
    { SYM_passto, "passto", run_passto },
    { SYM_passtoa0, "passtoa0", run_passtoa0 },
    { SYM_restorepl, "restorepl", run_restorepl },
    { SYM_prepshot, "prepshot", run_prepshot },
    { SYM_doshot, "doshot", run_doshot },
    { SYM_shotdiradj, "shotdiradj", run_shotdiradj },
    { SYM_CalcAngleOffset, "CalcAngleOffset", run_CalcAngleOffset },
    { SYM_AddPenalty, "AddPenalty", run_AddPenalty },
    { SYM_AddPenalty2, "AddPenalty2", run_AddPenalty2 },
    { SYM_PenaltyManager, "PenaltyManager", run_PenaltyManager },
    { SYM_chkprogress, "chkprogress", run_chkprogress },
    { SYM_PenShotChk, "PenShotChk", run_PenShotChk },
    { SYM_getBAplayerInfo, "getBAplayerInfo", run_getBAplayerInfo },
    { SYM_InProgress, "InProgress", run_InProgress },
    { SYM_coinsearch, "coinsearch", run_coinsearch },
    { SYM_checkfornewpen, "checkfornewpen", run_checkfornewpen },
    { SYM_Stop4Pen, "Stop4Pen", run_Stop4Pen },
    { SYM_limitfo, "limitfo", run_limitfo },
    { SYM_UpdatePA, "UpdatePA", run_UpdatePA },
    { SYM_SetPA, "SetPA", run_SetPA },
    { SYM_SetPA2, "SetPA2", run_SetPA2 },
    { SYM_PushRef, "PushRef", run_PushRef },
    { SYM_prefmes, "prefmes", run_prefmes },
    { SYM_ClearPenaltyMessage, "ClearPenaltyMessage", run_ClearPenaltyMessage },
    { SYM_PenGoalStuff, "PenGoalStuff", run_PenGoalStuff },
    { SYM_ClearPenaltyBuffer, "ClearPenaltyBuffer", run_ClearPenaltyBuffer },
    { SYM_updatepentime, "updatepentime", run_updatepentime },
    { SYM_ProcessPenaltyList, "ProcessPenaltyList", run_ProcessPenaltyList },
    { SYM_RemovePlayerFromList, "RemovePlayerFromList", run_RemovePlayerFromList },
    { SYM_chkatop, "chkatop", run_chkatop },
    { SYM_releasepl, "releasepl", run_releasepl },
    { SYM_GetLowestPen, "GetLowestPen", run_GetLowestPen },
    { SYM_updatepwrplay, "updatepwrplay", run_updatepwrplay },
    { SYM_ClrHor, "ClrHor", run_ClrHor },
    { SYM_SetHor, "SetHor", run_SetHor },
    { SYM_printscores1, "printscores1", run_printscores1 },
    { SYM_PrintTeamNameAndScore, "PrintTeamNameAndScore", run_PrintTeamNameAndScore },
    { SYM_PrintTeamLogoAndScore, "PrintTeamLogoAndScore", run_PrintTeamLogoAndScore },
    { SYM_EASNLogo, "EASNLogo", run_EASNLogo },
    { SYM_USBoard, "USBoard", run_USBoard },
    { SYM_pplpen, "pplpen", run_pplpen },
    { SYM_linebar, "linebar", run_linebar },
    { SYM_getlinee, "getlinee", run_getlinee },
    { SYM_AvgCline, "AvgCline", run_AvgCline },
    { SYM_ChkShotStat, "ChkShotStat", run_ChkShotStat },
    { SYM_SetupTeamForIntermission, "SetupTeamForIntermission", run_SetupTeamForIntermission },
    { SYM_RestoreTeamEnergy, "RestoreTeamEnergy", run_RestoreTeamEnergy },
    { SYM_reenergizeteam, "reenergizeteam", run_reenergizeteam },
    { SYM_SetScore, "SetScore", run_SetScore },
    { SYM_InitScores, "InitScores", run_InitScores },
    { SYM_UpdateScores, "UpdateScores", run_UpdateScores },
    { SYM_NewTicker, "NewTicker", run_NewTicker },
    { SYM_NewTicker2, "NewTicker2", run_NewTicker2 },
    { SYM_CheckGameTickerStatus, "CheckGameTickerStatus", run_CheckGameTickerStatus },
    { SYM_NewTicker3, "NewTicker3", run_NewTicker3 },
    { SYM_ClearTickerArea, "ClearTickerArea", run_ClearTickerArea },
    { SYM_SetTickerAreaPosition, "SetTickerAreaPosition", run_SetTickerAreaPosition },
    { SYM_PrintStringFromList, "PrintStringFromList", run_PrintStringFromList },
    { SYM_AdvanceStringPtr, "AdvanceStringPtr", run_AdvanceStringPtr },
    { SYM_ManualGoalieMenu, "ManualGoalieMenu", run_ManualGoalieMenu },
    { SYM_RunArenaAnim, "RunArenaAnim", run_RunArenaAnim },
    { SYM_NextArenaFrame, "NextArenaFrame", run_NextArenaFrame },
    { SYM_SetCameraTop, "SetCameraTop", run_SetCameraTop },
    { SYM_StartArenaAnim, "StartArenaAnim", run_StartArenaAnim },
    { SYM_SetFaceoffAnim, "SetFaceoffAnim", run_SetFaceoffAnim },
    { SYM_EndArenaAnim, "EndArenaAnim", run_EndArenaAnim },
    { SYM_puckvzadj, "puckvzadj", run_puckvzadj },
    { SYM_OneTimerPass, "OneTimerPass", run_OneTimerPass },
    { SYM_OneTimerTarget, "OneTimerTarget", run_OneTimerTarget },
    { SYM_assonetimer, "assonetimer", run_assonetimer },
    { SYM_setonetimeranim, "setonetimeranim", run_setonetimeranim },
    { SYM_PuckOnAttackHalf, "PuckOnAttackHalf", run_PuckOnAttackHalf },
    { SYM_onetimershot, "onetimershot", run_onetimershot },
    { SYM_CheckOneTimerFacing, "CheckOneTimerFacing", run_CheckOneTimerFacing },
    { SYM_LoadCrowdRec, "LoadCrowdRec", run_LoadCrowdRec },
    { SYM_Crowd_Noise, "Crowd_Noise", run_Crowd_Noise },
    { SYM_stopna2, "stopna2", run_stopna2 },
    { SYM_checkcornercoll, "checkcornercoll", run_checkcornercoll },
    { SYM_HotColdLoop, "HotColdLoop", run_HotColdLoop },
    { SYM_Create_HotCold_Table, "Create_HotCold_Table", run_Create_HotCold_Table },
    { SYM_Clamp0to100, "Clamp0to100", run_Clamp0to100 },
    { SYM_AttributeCalc, "AttributeCalc", run_AttributeCalc },
    { SYM_NextHomeHotPlayer, "NextHomeHotPlayer", run_NextHomeHotPlayer },
    { SYM_NextAwayHotPlayer, "NextAwayHotPlayer", run_NextAwayHotPlayer },
    { SYM_BuildHotColdLists, "BuildHotColdLists", run_BuildHotColdLists },
    { SYM_SortHotColdStarters, "SortHotColdStarters", run_SortHotColdStarters },
    { SYM_NextHomeColdPlayer, "NextHomeColdPlayer", run_NextHomeColdPlayer },
    { SYM_NextAwayColdPlayer, "NextAwayColdPlayer", run_NextAwayColdPlayer },
    { SYM_CopyHottestPlayer, "CopyHottestPlayer", run_CopyHottestPlayer },
    { SYM_CopyColdestPlayer, "CopyColdestPlayer", run_CopyColdestPlayer },
    { SYM_CompareHotColdTotals, "CompareHotColdTotals", run_CompareHotColdTotals },
    { SYM_GetHotColdTotal, "GetHotColdTotal", run_GetHotColdTotal },
    { SYM_EndShootout, "EndShootout", run_EndShootout },
    { SYM_ClampPositionBox, "ClampPositionBox", run_ClampPositionBox },
    { SYM_ClearGameStats, "ClearGameStats", run_ClearGameStats },
    { SYM_AddArenaAnimSprite, "AddArenaAnimSprite", run_AddArenaAnimSprite },
    { SYM_PrintPlayerNameRight, "PrintPlayerNameRight", run_PrintPlayerNameRight },
    { SYM_SkipNameNumber, "SkipNameNumber", run_SkipNameNumber },
    { SYM_SetStatScroll, "SetStatScroll", run_SetStatScroll },
    { SYM_StatScrollRow, "StatScrollRow", run_StatScrollRow },
    { SYM_StatScrollRowEnd, "StatScrollRowEnd", run_StatScrollRowEnd },
    { SYM_PrintStatScrollArrows, "PrintStatScrollArrows", run_PrintStatScrollArrows },
    { SYM_DisplayTeamStatsScreen, "DisplayTeamStatsScreen", run_DisplayTeamStatsScreen },
    { SYM_PrintStatValue, "PrintStatValue", run_PrintStatValue },
    { SYM_updatePPTeamTime, "updatePPTeamTime", run_updatePPTeamTime },
    { SYM_GetTeamRating, "GetTeamRating", run_GetTeamRating },
    { SYM_ShortenMsgTimer, "ShortenMsgTimer", run_ShortenMsgTimer },
    { SYM_ClearShotData, "ClearShotData", run_ClearShotData },
    { SYM_restoreteams, "restoreteams", run_restoreteams },
    { SYM_ResetClock, "ResetClock", run_ResetClock },
    { SYM_GetPeriodTime, "GetPeriodTime", run_GetPeriodTime },
    { SYM_periodicevents, "periodicevents", run_periodicevents },
    { SYM_CheckInjury, "CheckInjury", run_CheckInjury },
    { SYM_RestoreBenchEnergy, "RestoreBenchEnergy", run_RestoreBenchEnergy },
    { SYM_CheckLateGameSong, "CheckLateGameSong", run_CheckLateGameSong },
    { SYM_UpdateCwdExcite, "UpdateCwdExcite", run_UpdateCwdExcite },
    { SYM_updatecrowdf, "updatecrowdf", run_updatecrowdf },
    { SYM_clockcont, "clockcont", run_clockcont },
    { SYM_clockcont_0, "clockcont_0", run_clockcont_0 },
    { SYM_EndDemoOnPress, "EndDemoOnPress", run_EndDemoOnPress },
    { SYM_demoread, "demoread", run_demoread },
    { SYM_startpause, "startpause", run_startpause },
    { SYM_startpause1, "startpause1", run_startpause1 },
    { SYM_startpause2, "startpause2", run_startpause2 },
    { SYM_startpause3, "startpause3", run_startpause3 },
    { SYM_startpause4, "startpause4", run_startpause4 },
    { SYM_SetupPauseScreen, "SetupPauseScreen", run_SetupPauseScreen },
    { SYM_seta2, "seta2", run_seta2 },
    { SYM_updatesound, "updatesound", run_updatesound },
    { SYM_KillCrowd, "KillCrowd", run_KillCrowd },
    { SYM_VBlank, "VBlank", run_VBlank },
    { SYM_vb2, "vb2", run_vb2 },
    { SYM_DumpSprites, "DumpSprites", run_DumpSprites },
    { SYM_DumpSprites2, "DumpSprites2", run_DumpSprites2 },
    { SYM_DoDMAList, "DoDMAList", run_DoDMAList },
    { SYM_SetScroll2, "SetScroll2", run_SetScroll2 },
    { SYM_setvideo, "setvideo", run_setvideo },
    { SYM_updatescroll, "updatescroll", run_updatescroll },
    { SYM_showref, "showref", run_showref },
    { SYM_checkfo, "checkfo", run_checkfo },
    { SYM_showzam, "showzam", run_showzam },
    { SYM_SetSframe, "SetSframe", run_SetSframe },
    { SYM_showcrowd, "showcrowd", run_showcrowd },
    { SYM_showclock, "showclock", run_showclock },
    { SYM_checksso, "checksso", run_checksso },
    { SYM_setsortcords, "setsortcords", run_setsortcords },
    { SYM_setffo, "setffo", run_setffo },
    { SYM_uppads, "uppads", run_uppads },
    { SYM_QueuePadLabel, "QueuePadLabel", run_QueuePadLabel },
    { SYM_RenderSmallFontChar, "RenderSmallFontChar", run_RenderSmallFontChar },
    { SYM_addframe, "addframe", run_addframe },
    { SYM_addframe2, "addframe2", run_addframe2 },
    { SYM_find3d, "find3d", run_find3d },
    { SYM_getpzjoy, "getpzjoy", run_getpzjoy },
    { SYM_ShowReplayIcon, "ShowReplayIcon", run_ShowReplayIcon },
    { SYM_EraseReplayIcon, "EraseReplayIcon", run_EraseReplayIcon },
    { SYM_ShowReplayBanner, "ShowReplayBanner", run_ShowReplayBanner },
    { SYM_EraseReplayBanner, "EraseReplayBanner", run_EraseReplayBanner },
    { SYM_suba4, "suba4", run_suba4 },
    { SYM_adda4, "adda4", run_adda4 },
    { SYM_adda42, "adda42", run_adda42 },
    { SYM_adda43, "adda43", run_adda43 },
    { SYM_UpdateCameraPos, "UpdateCameraPos", run_UpdateCameraPos },
    { SYM_RevReplayAdj, "RevReplayAdj", run_RevReplayAdj },
    { SYM_RestoreReplayFrame, "RestoreReplayFrame", run_RestoreReplayFrame },
    { SYM_ClampReplayView, "ClampReplayView", run_ClampReplayView },
    { SYM_updatereplay, "updatereplay", run_updatereplay },
    { SYM_updateplayers, "updateplayers", run_updateplayers },
    { SYM_updateanim, "updateanim", run_updateanim },
    { SYM_freezewindow, "freezewindow", run_freezewindow },
    { SYM_checkwindow, "checkwindow", run_checkwindow },
    { 0, NULL, NULL }
};
