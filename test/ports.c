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
    SetSPA(obj_at(ram_at(r->regs_in[R_A3])), (uint16_t)r->regs_in[R_D1]);
}

static uint32_t be32(const uint8_t *p) { return (uint32_t)p[0] << 24 | p[1] << 16 | p[2] << 8 | p[3]; }

static void run_GetHot(const Record *r)
{
    const uint8_t *sp = r->ram_in + (r->regs_in[R_A7] & 0xFFFF);
    int16_t hx, hy;
    GetHot(obj_at(ram_at(be32(sp + 4))), &hx, &hy);
    expect_reg16(r, R_D0, (uint16_t)hx, "d0 (hot x)");
    expect_reg16(r, R_D1, (uint16_t)hy, "d1 (hot y)");
}

#define OBJ(reg) obj_at(ram_at(r->regs_in[reg]))
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

#define TEAM(reg) team_at(ram_at(r->regs_in[reg]))
#define PLACE(reg) ram_at(r->regs_in[reg])                  /* (a place a register holds) */
#define BYTES_AT(reg) ((uint8_t *)&g + (uint16_t)r->regs_in[reg])   /* (a byte table inside a team struct: penbox, the goals) */
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
V(EndPenaltyShotPlay) V(ChkGoalies) V(ReturnGoalies) V(findpc)
static void run_ResetAndSelectPlayers(const Record *r) { ResetAndSelectPlayers(OBJ(R_A3)); }
static void run_ClampYPosition(const Record *r)
{
    int16_t d0 = D16(R_D0), d1 = D16(R_D1);
    ClampYPosition(&d0, &d1, D16(R_D3), OBJ(R_A3));
    expect_reg16(r, R_D0, (uint16_t)d0, "d0"); expect_reg16(r, R_D1, (uint16_t)d1, "d1");
}
static void run_AdjustFacingDirection(const Record *r) { AdjustFacingDirection(D16(R_D0), OBJ(R_A3)); }
static void run_goaliesave(const Record *r) { goaliesave(ram_at(r->regs_in[R_A0]), D16(R_D3), OBJ(R_A3)); }
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
static void run_changeplayer(const Record *r) { changeplayer((uint16_t)r->regs_in[R_D4], OBJ(R_A3)); }
static void run_setc1player(const Record *r) { setc1player((uint16_t)r->regs_in[R_D0]); }
static void run_setc2player(const Record *r) { setc2player((uint16_t)r->regs_in[R_D0]); }
static void run_Findhittype(const Record *r) { int z = Findhittype((uint16_t)r->regs_in[R_D0], OBJ(R_A3)); if (z != (int)((r->regs_out[R_SR] >> 2) & 1)) { printf("    Z flag: got %d, 68k %d\n", z, (int)((r->regs_out[R_SR] >> 2) & 1)); } }
static void run_ShotMode(const Record *r) { ShotMode((uint16_t)r->regs_in[R_D0], (uint16_t)r->regs_in[R_D2], OBJ(R_A3)); }
static void run_setpads(const Record *r) { setpads(D16(R_D4), OBJ(R_A3)); }
static void run_loadTeamStruct(const Record *r) { struct Team *a1; struct Team *a2 = loadTeamStruct(OBJ(R_A3), &a1); expect_reg16(r, R_A2, (uint16_t)rp_addr(team_place(a2)), "a2"); expect_reg16(r, R_A1, (uint16_t)rp_addr(team_place(a1)), "a1"); }

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
static void run_Stop4Pen(const Record *r) { Stop4Pen(r->regs_in[R_A0], OBJ(R_A3)); }
static void run_SetPA(const Record *r) { SetPA((uint16_t)r->regs_in[R_D0]); }
static void run_PushRef(const Record *r) { PushRef((uint16_t)r->regs_in[R_D0]); }
static void run_prefmes(const Record *r) { prefmes(D16(R_D0)); }
static void run_PenGoalStuff(const Record *r) { PenGoalStuff(TEAM(R_A1), TEAM(R_A2)); }
static void run_ProcessPenaltyList(const Record *r) { ProcessPenaltyList(TEAM(R_A2)); }
static void run_RemovePlayerFromList(const Record *r) { uint8_t *a0 = RemovePlayerFromList(BYTES_AT(R_A0)); expect_reg16(r, R_A0, (uint16_t)(a0 - (uint8_t *)&g), "a0"); }
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
static void run_SetScore(const Record *r) { SetScore(ram_at(r->regs_in[R_A0])); }
static void run_NewTicker(const Record *r) { rp gs = { 0 }; expect_reg16(r, R_D3, (uint16_t)NewTicker(&gs), "d3"); }
static void run_NewTicker2(const Record *r) { rp gs = { 0 }; expect_reg16(r, R_D3, (uint16_t)NewTicker2(&gs), "d3"); }
static void run_CheckGameTickerStatus(const Record *r) { rp gs = { 0 }; expect_reg16(r, R_D3, (uint16_t)CheckGameTickerStatus(D16(R_D3), &gs), "d3"); if (rp_addr(gs) != RAM_BASE) expect_reg16(r, R_A0, (uint16_t)rp_addr(gs), "a0"); }
static void run_NewTicker3(const Record *r) { NewTicker3(D16(R_D3), ram_at(r->regs_in[R_A0])); }
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
#define HOTCOLD(name) static void run_##name(const Record *r) { struct Team *tm; expect_reg16(r, R_D1, name(&tm), "d1"); expect_reg16(r, R_A1, (uint16_t)rp_addr(team_place(tm)), "a1"); }
HOTCOLD(NextHomeHotPlayer) HOTCOLD(NextAwayHotPlayer) HOTCOLD(NextHomeColdPlayer) HOTCOLD(NextAwayColdPlayer)
static void run_SortHotColdStarters(const Record *r) { SortHotColdStarters(TEAM(R_A0)); }
static void run_CopyHottestPlayer(const Record *r) { CopyHottestPlayer(ram_at(r->regs_in[R_A0])); }
static void run_CopyColdestPlayer(const Record *r) { CopyColdestPlayer(ram_at(r->regs_in[R_A0])); }
static void run_CompareHotColdTotals(const Record *r) { expect_reg16(r, R_D0, CompareHotColdTotals(), "d0"); }
static void run_GetHotColdTotal(const Record *r) { expect_reg16(r, R_D1, GetHotColdTotal(TEAM(R_A0)), "d1"); }

/* ---- period94, hockey94, display94 ---- */
V(EndShootout) V(ClearGameStats) V(SetStatScroll) V(PrintStatScrollArrows) V(DisplayTeamStatsScreen) V(updatePPTeamTime)
V(ClearShotData) V(restoreteams) V(ResetClock) V(periodicevents) V(CheckInjury) V(RestoreBenchEnergy) V(CheckLateGameSong)
V(UpdateCwdExcite) V(updatecrowdf) V(clockcont) V(clockcont_0) V(demoread) V(startpause) V(startpause1) V(startpause2) V(startpause3)
V(startpause4) V(SetupPauseScreen) V(updatesound) V(KillCrowd)
O(ClampPositionBox) O(ShortenMsgTimer)
static void run_AddArenaAnimSprite(const Record *r) { rp sat = ram_at(r->regs_in[R_A6]); uint16_t link = (uint16_t)r->regs_in[R_D6]; AddArenaAnimSprite(&sat, &link); expect_reg16(r, R_A6, (uint16_t)rp_addr(sat), "a6"); expect_reg16(r, R_D6, link, "d6"); }
static void run_PrintPlayerNameRight(const Record *r) { PrintPlayerNameRight((uint16_t)r->regs_in[R_D0], TEAM(R_A2)); }
static void run_SkipNameNumber(const Record *r) { uint32_t a1 = SkipNameNumber((uint16_t)r->regs_in[R_D0], TEAM(R_A2)); expect_reg16(r, R_A1, (uint16_t)a1, "a1"); }
static void run_StatScrollRow(const Record *r) { expect_reg16(r, R_D3, StatScrollRow(), "d3"); }
static void run_StatScrollRowEnd(const Record *r) { expect_reg16(r, R_D3, StatScrollRowEnd(), "d3"); }
static void run_PrintStatValue(const Record *r) { PrintStatValue(rom_ptr(r->regs_in[R_A0]), TEAM(R_A2)); }
static void run_GetTeamRating(const Record *r) { expect_reg16(r, R_D0, GetTeamRating(TEAM(R_A2)), "d0"); }
static void run_GetPeriodTime(const Record *r) { expect_reg16(r, R_D0, GetPeriodTime(), "d0"); }
static void run_EndDemoOnPress(const Record *r) { EndDemoOnPress((uint16_t)r->regs_in[R_D1]); }
static void run_seta2(const Record *r) { expect_reg16(r, R_A2, (uint16_t)rp_addr(team_place(seta2())), "a2"); }

/* display94: the sprite builders take a5 / a6 / d6 in a struct Vid and leave them advanced */
#define VID_IN(v) struct Vid v = { ram_at(r->regs_in[R_A5]), ram_at(r->regs_in[R_A6]), (uint16_t)r->regs_in[R_D6] }
#define VID_OUT(v) expect_reg16(r, R_A5, (uint16_t)rp_addr(v.dma), "a5"); expect_reg16(r, R_A6, (uint16_t)rp_addr(v.sat), "a6"); expect_reg16(r, R_D6, v.link, "d6")
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
static void run_QueuePadLabel(const Record *r) { VID_IN(v); QueuePadLabel((uint16_t)r->regs_in[R_D1], ram_at(r->regs_in[R_A0]), &v); VID_OUT(v); }
static void run_RenderSmallFontChar(const Record *r) { VID_IN(v); RenderSmallFontChar((uint16_t)r->regs_in[R_D2], (uint16_t)r->regs_in[R_D0], ram_at(r->regs_in[R_A0]), &v); VID_OUT(v); }
static void run_addframe(const Record *r) { VID_IN(v); addframe(OBJ(R_A3), &v); VID_OUT(v); }
static void run_addframe2(const Record *r) { VID_IN(v); addframe2(D16(R_D0), D16(R_D1), OBJ(R_A3), &v); VID_OUT(v); }
static void run_find3d(const Record *r) { int16_t d0 = D16(R_D0), d1 = D16(R_D1); find3d(&d0, &d1, D16(R_D2)); expect_reg16(r, R_D0, (uint16_t)d0, "d0"); expect_reg16(r, R_D1, (uint16_t)d1, "d1"); }
/* replay94 */
static void run_getpzjoy(const Record *r) { uint16_t d0, d1, d2, d3; getpzjoy(&d0, &d1, &d2, &d3); expect_reg16(r, R_D0, d0, "d0"); expect_reg16(r, R_D1, d1, "d1"); expect_reg16(r, R_D2, d2, "d2"); expect_reg16(r, R_D3, d3, "d3"); }
static void run_ShowReplayIcon(const Record *r) { ShowReplayIcon(); }
static void run_EraseReplayIcon(const Record *r) { EraseReplayIcon(); }
static void run_ShowReplayBanner(const Record *r) { ShowReplayBanner(); }
static void run_EraseReplayBanner(const Record *r) { EraseReplayBanner(); }
#define A4_RUN(name) static void run_##name(const Record *r) { uint32_t a4 = r->regs_in[R_A4]; uint16_t d7 = name(&a4, ram_at(r->regs_in[R_A5])); expect_reg16(r, R_A4, (uint16_t)a4, "a4"); expect_reg16(r, R_D7, d7, "d7"); }
A4_RUN(suba4) A4_RUN(adda4) A4_RUN(adda42)
static void run_adda43(const Record *r) { uint32_t a4 = r->regs_in[R_A4]; adda43(&a4); expect_reg16(r, R_A4, (uint16_t)a4, "a4"); }
static void run_UpdateCameraPos(const Record *r) { UpdateCameraPos(OBJ(R_A3), ram_at(r->regs_in[R_A5])); }
static void run_RevReplayAdj(const Record *r) { expect_reg16(r, R_D7, RevReplayAdj(r->regs_in[R_A4], ram_at(r->regs_in[R_A5])), "d7"); }
static void run_RestoreReplayFrame(const Record *r) { expect_reg16(r, R_D7, RestoreReplayFrame(r->regs_in[R_A4], ram_at(r->regs_in[R_A5])), "d7"); }
static void run_ClampReplayView(const Record *r) { ClampReplayView(); }
static void run_updatereplay(const Record *r) { updatereplay(); }
static void run_updateplayers(const Record *r) { updateplayers(); }
static void run_updateanim(const Record *r) { updateanim(OBJ(R_A3)); }
static void run_freezewindow(const Record *r) { freezewindow(); }
static void run_checkwindow(const Record *r) { checkwindow(); }


/* ---- video94 ---- */
#define R16(reg) ((uint16_t)r->regs_in[reg])
static uint16_t stack_word(const Record *r, int off) { const uint8_t *sp = r->ram_in + ((r->regs_in[R_A7] + off) & 0xFFFF); return (uint16_t)(sp[0] << 8 | sp[1]); }
static uint32_t ret_addr(const Record *r) { const uint8_t *sp = r->ram_in + (r->regs_in[R_A7] & 0xFFFF); return be32(sp); }
static const uint8_t *inline_string(const Record *r) { return rom_ptr(ret_addr(r)); }   /* (the code after the call: the cartridge) */
static void run_randomd0s(const Record *r) { expect_reg16(r, R_D0, randomd0s(R16(R_D0)), "d0"); }
static void run_sroot(const Record *r) { expect_reg16(r, R_D0, sroot(r->regs_in[R_D0]), "d0"); }
static void run_sfx(const Record *r) { sfx((int16_t)stack_word(r, 4)); }
static void run_song(const Record *r) { song((int16_t)stack_word(r, 4)); }
static void run_UnpackNibbles(const Record *r) { UnpackNibbles(r->regs_in[R_A0], R16(R_D0)); }
static void run_WeightedRandomSelect(const Record *r) { expect_reg16(r, R_D0, WeightedRandomSelect(R16(R_D0)), "d0"); }
static void run_remap(const Record *r) { remap(r->regs_in[R_A0], r->regs_in[R_A1], R16(R_D0), R16(R_D1)); }
static void run_waitx(const Record *r) { uint16_t d0, d1, d2, d3; waitx(R16(R_D0), &d0, &d1, &d2, &d3); expect_reg16(r, R_D1, d1, "d1"); }
static void run_waitxsr(const Record *r) { expect_reg16(r, R_D1, waitxsr(R16(R_D0)), "d1"); }
static void run_waitjoy(const Record *r) { expect_reg16(r, R_D1, waitjoy(), "d1"); }
static void run_orjoy(const Record *r) { expect_reg16(r, R_D1, orjoy(), "d1"); }
static void run_nodiag(const Record *r) { uint16_t d1 = R16(R_D1); nodiag(&d1, R16(R_D3)); expect_reg16(r, R_D1, d1, "d1"); }
static void run_ProcessInputWithRepeat(const Record *r) { uint16_t d1 = R16(R_D1); ProcessInputWithRepeat(&d1, R16(R_D2), R16(R_D3)); expect_reg16(r, R_D1, d1, "d1"); }
static void run_ReadJoy(const Record *r) { uint16_t d0, d1; ReadJoy((uint8_t)r->regs_in[R_D0], &d0, &d1); expect_reg16(r, R_D0, d0, "d0"); expect_reg16(r, R_D1, d1, "d1"); }
#define JOY(name) static void run_##name(const Record *r) { uint16_t d0, d1, d2, d3; name(&d0, &d1, &d2, &d3); \
    expect_reg16(r, R_D0, d0, "d0"); expect_reg16(r, R_D1, d1, "d1"); expect_reg16(r, R_D2, d2, "d2"); expect_reg16(r, R_D3, d3, "d3"); }
JOY(ReadJoy1) JOY(ReadJoy2) JOY(ReadJoy3) JOY(ReadJoy4)
static void run_DoDMApro(const Record *r) { DoDMApro(r->regs_in[R_A0], R16(R_D0), R16(R_D1)); }
static void run_DoDMA(const Record *r) { DoDMA(r->regs_in[R_A0], R16(R_D0), R16(R_D1)); }
static void run_DoDMA_nd2(const Record *r) { DoDMA_nd2(R16(R_D0), R16(R_D1), R16(R_D2)); }
static void run_DoFill(const Record *r) { DoFill(R16(R_D0), R16(R_D1), R16(R_D2)); }
static void run_setVram(const Record *r) { setVram(R16(R_D0)); }
static void run_Vmaddr(const Record *r) { Vmaddr(R16(R_D0)); }
static void run_dobitmap(const Record *r)
{
    expect_reg16(r, R_D4, dobitmap(r->regs_in[R_A0], r->regs_in[R_A1], r->regs_in[R_A2], R16(R_D0), R16(R_D1), R16(R_D2), R16(R_D3), R16(R_D4), R16(R_D5)), "d4");
}
static void run_DecompressGraphicsWithCallback(const Record *r) { expect_reg16(r, R_D4, DecompressGraphicsWithCallback(r->regs_in[R_A2], R16(R_D4), ret_addr(r)), "d4"); }
static void run_DecompressGraphicsPlain(const Record *r) { expect_reg16(r, R_D4, DecompressGraphicsPlain(r->regs_in[R_A2], R16(R_D4)), "d4"); }
static void run_DecompressGraphics(const Record *r) { expect_reg16(r, R_D4, DecompressGraphics(r->regs_in[R_A2], R16(R_D4)), "d4"); }
static void run_DecompressBytecode(const Record *r) { DecompressBytecode(r->regs_in[R_A0], R16(R_D1)); }
static struct Unpack unpack_in(const Record *r) { struct Unpack u = { r->regs_in[R_A0], (uint8_t)r->regs_in[R_D1], R16(R_D3) }; return u; }
static void unpack_out(const Record *r, const struct Unpack *u, int done)
{
    if (done) return;                              /* the end code returns from DecompressBytecode */
    expect_reg16(r, R_A0, (uint16_t)u->src, "a0"); expect_reg16(r, R_D1, u->idx, "d1"); expect_reg16(r, R_D3, u->vram, "d3");
}
#define OP(name) static void run_##name(const Record *r) { struct Unpack u = unpack_in(r); int done = name(&u); unpack_out(r, &u, done); }
OP(Opcode_CopyLiteral) OP(Opcode_ClearBytes) OP(Opcode_Fillbytes) OP(Opcode_CopyBackwardShort) OP(Opcode_CopyBackwardMedium)
OP(Opcode_CopyBackwardLong) OP(Opcode_CopyBackwardExtended1) OP(Opcode_CopyBackwardExtended2) OP(Opcode_CopyBackwardReverseShort)
OP(Opcode_CopyBackwardReverseMedium) OP(Opcode_CopyBackwardReverseLong)
static void run_CopyBackwardRun(const Record *r) { struct Unpack u = unpack_in(r); CopyBackwardRun(&u, R16(R_D0), (uint8_t)r->regs_in[R_D2]); unpack_out(r, &u, 0); }
static void run_CopyBackwardReverseRun(const Record *r) { struct Unpack u = unpack_in(r); CopyBackwardReverseRun(&u, R16(R_D0), (uint8_t)r->regs_in[R_D2]); unpack_out(r, &u, 0); }
static void run_FlushOutputBuffer(const Record *r) { struct Unpack u = unpack_in(r); FlushOutputBuffer(&u); expect_reg16(r, R_D3, u.vram, "d3"); }
static void run_eraser(const Record *r) { eraser(R16(R_D0), R16(R_D1), R16(R_D2)); }
static void run_Framer(const Record *r) { Framer(R16(R_D0), R16(R_D1)); }
static void run_printsmallz(const Record *r) { const uint8_t *s = inline_string(r); printsmallz(s, rd16(s)); }
static void run_printz(const Record *r) { const uint8_t *s = inline_string(r); printz(s, rd16(s)); }
static void run_printbigz(const Record *r) { const uint8_t *s = inline_string(r); printbigz(s, rd16(s)); }
static void run_appendz(const Record *r) { const uint8_t *s = inline_string(r); appendz(s, rd16(s), r->regs_in[R_A3]); }
static void run_printsmall(const Record *r) { expect_reg16(r, R_A1, printsmall(r->regs_in[R_A1]), "a1"); }
static void run_print(const Record *r) { expect_reg16(r, R_A1, print(r->regs_in[R_A1]), "a1"); }
static void run_printbig(const Record *r) { printbig(r->regs_in[R_A1]); }
static void run_appstring(const Record *r) { appstring(r->regs_in[R_A1], r->regs_in[R_A3]); }
#define CC(name) static void run_##name(const Record *r) { struct PrintState st = { { NULL, r->regs_in[R_A1] }, R16(R_D2), R16(R_D3) }; name(&st); \
    expect_reg16(r, R_A1, (uint16_t)st.p.at, "a1"); expect_reg16(r, R_D2, st.attr, "d2"); expect_reg16(r, R_D3, st.count, "d3"); }
CC(ControlCode_SetMapAndPosition) CC(ControlCode_SetMap) CC(ControlCode_SetAttribute) CC(ControlCode_SetX) CC(ControlCode_SetY)
CC(ControlCode_AddX) CC(ControlCode_AddY) CC(ControlCode_SetFont)
static void run_FormatAndPrintTime(const Record *r) { FormatAndPrintTime(R16(R_D0)); }
static void run_PushTime(const Record *r) { expect_reg16(r, R_A1, PushTime(R16(R_D0)), "a1"); }
static void run_PushNumber(const Record *r) { expect_reg16(r, R_A1, PushNumber(R16(R_D0)), "a1"); }
static void run_PushNumberWidth(const Record *r) { expect_reg16(r, R_A1, PushNumberWidth(R16(R_D0), R16(R_D1)), "a1"); }
static void run_PrintBigChar(const Record *r)
{
    struct BigPen pen = { R16(R_D4), R16(R_D5), R16(R_D6) };
    PrintBigChar((uint8_t)r->regs_in[R_D0], &pen);
    expect_reg16(r, R_D4, pen.x, "d4"); expect_reg16(r, R_D5, pen.y, "d5");
}
static void run_PutBigTile(const Record *r) { struct BigPen pen = { R16(R_D4), R16(R_D5), R16(R_D6) }; PutBigTile(R16(R_D3), &pen); }
static void run_AddSmallFont(const Record *r) { expect_reg16(r, R_D4, AddSmallFont(R16(R_D4)), "d4"); }
static void run_AddFramer(const Record *r) { expect_reg16(r, R_D4, AddFramer(R16(R_D4)), "d4"); }
static void run_AddTeamBlock(const Record *r) { expect_reg16(r, R_D4, AddTeamBlock(), "d4"); }
V(forceblack) V(forceblack2) V(forcefade) V(cramfade) V(CopyPaletteToCRAM) V(WaitDMA) V(setVram_0) V(xyVmMap)


/* ---- title94, data94, cards94, sram94, stats94, setup94, shootout94, fourway94, menu94 helpers ---- */
static void expect_z(const Record *r, int z) { expect_reg16(r, R_SR, z ? 4 : 0, "Z flag"); }
#define ZOUT(r) (((r)->regs_out[R_SR] >> 2) & 1)
static void expect_z_only(const Record *r, int z) { if (z != (int)ZOUT(r)) expect_z(r, z); }
static void run_EndOneTimer(const Record *r) { EndOneTimer(OBJ(R_A3)); }
static void run_getFgtbyte(const Record *r) { expect_reg16(r, R_D0, getFgtbyte(OBJ(R_A2), OBJ(R_A3)), "d0"); }
static void run_chkFgtBit1(const Record *r) { expect_z_only(r, chkFgtBit1(OBJ(R_A2), OBJ(R_A3))); }
static void run_ReadGoaliePulled(const Record *r)
{
    uint8_t ccr = ReadGoaliePulled(OBJ(R_A3));
    if ((ccr & (CCR_Z | CCR_N)) != (r->regs_out[R_SR] & (CCR_Z | CCR_N))) expect_reg16(r, R_SR, ccr, "ccr (N, Z)");
}
static void run_ShootoutShootCheck(const Record *r) { expect_z_only(r, ShootoutShootCheck(OBJ(R_A3))); }
static void run_SkatePath(const Record *r)
{
    int16_t x = D16(R_D0), y = D16(R_D1);
    SkatePath(&x, &y, OBJ(R_A3));
    expect_reg16(r, R_D0, (uint16_t)x, "d0"); expect_reg16(r, R_D1, (uint16_t)y, "d1");
}
static void run_NextPathPoint(const Record *r) { NextPathPoint(OBJ(R_A3)); }
static void run_StartShootoutPath(const Record *r) { StartShootoutPath(OBJ(R_A3)); }
static void run_RandomFaceoffAnim(const Record *r) { expect_reg16(r, R_D0, RandomFaceoffAnim(), "d0"); }
static void run_LeadSongExit(const Record *r) { LeadSongExit(stack_word(r, 0)); }
static void run_chgplayer(const Record *r) { chgplayer(R16(R_D4), OBJ(R_A3)); }
static void run_swpchk(const Record *r) { swpchk(OBJ(R_A3)); }
static void run_j_setc1player(const Record *r) { j_setc1player(R16(R_D0)); }
static void run_j_setc2player(const Record *r) { j_setc2player(R16(R_D0)); }
static void run_CountButtonPress(const Record *r) { CountButtonPress(D16(R_D4), OBJ(R_A3)); }
static void run_PSandSOpassdir(const Record *r) { PSandSOpassdir(OBJ(R_A3)); }
static void run_LoadHomeTeamGfx(const Record *r) { expect_reg16(r, R_D4, LoadHomeTeamGfx(R16(R_D4)), "d4"); }
static void run_HotColdIcon(const Record *r) { HotColdIcon(TEAM(R_A2)); }
static void run_clrTmPdst(const Record *r) { clrTmPdst(TEAM(R_A0)); }
static void run_PlaceBoardFall(const Record *r) { PlaceBoardFall(OBJ(R_A3)); }
static void run_PrintParenNumber(const Record *r) { PrintParenNumber(BYTES_AT(R_A2), R16(R_D0)); }
static void run_PrintPlayerGoals(const Record *r) { PrintPlayerGoals(R16(R_D0), TEAM(R_A2)); }
static void run_PrintPlayerAssists(const Record *r) { PrintPlayerAssists(R16(R_D0), TEAM(R_A2)); }
static void run_CalcStarScores(const Record *r) { CalcStarScores(R16(R_D2)); }
static void run_CalcTeamStarScores(const Record *r)
{
    rp out = PLACE(R_A4);
    expect_reg16(r, R_D3, (uint16_t)CalcTeamStarScores(TEAM(R_A2), TEAM(R_A3), &out, R16(R_D5)), "d3");
    expect_reg16(r, R_A4, (uint16_t)rp_addr(out), "a4");
}
static void run_TakeTopStarScore(const Record *r)
{
    struct Team *tm;
    expect_reg16(r, R_D0, TakeTopStarScore(&tm), "d0");
    expect_reg16(r, R_A2, (uint16_t)rp_addr(team_place(tm)), "a2");
}
static void run_GetPlayerNamePointer(const Record *r) { expect_reg16(r, R_A0, (uint16_t)GetPlayerNamePointer(R16(R_D0), TEAM(R_A2)), "a0"); }
static void run_ConvertByteToDigits(const Record *r) { expect_reg16(r, R_A1, (uint16_t)rp_addr(ConvertByteToDigits(R16(R_D0), PLACE(R_A1))), "a1"); }
static void run_FinalizeTextBuffer(const Record *r) { expect_reg16(r, R_A1, (uint16_t)FinalizeTextBuffer(PLACE(R_A1)), "a1"); }
static void run_FormatPlayerName(const Record *r) { expect_reg16(r, R_A1, (uint16_t)FormatPlayerName(R16(R_D0), TEAM(R_A2)), "a1"); }
static void run_FormatPlayerNumInitial(const Record *r) { expect_reg16(r, R_A1, (uint16_t)FormatPlayerNumInitial(R16(R_D0), TEAM(R_A2)), "a1"); }
static void run_getname(const Record *r) { expect_reg16(r, R_A1, (uint16_t)getname(R16(R_D0), TEAM(R_A2)), "a1"); }
static void run_GetPlayerName(const Record *r) { expect_reg16(r, R_A1, (uint16_t)GetPlayerName(), "a1"); }
static void run_GetPlayerNumInitial(const Record *r) { expect_reg16(r, R_A1, (uint16_t)GetPlayerNumInitial(), "a1"); }
static void run_GetShifter(const Record *r) { uint16_t d2; expect_reg16(r, R_D1, GetShifter(&d2), "d1"); expect_reg16(r, R_D2, d2, "d2"); }
static void run_wallcollduringcheck(const Record *r) { wallcollduringcheck(OBJ(R_A2), OBJ(R_A3)); }
static void run_CrowdRecordIO(const Record *r) { CrowdRecordIO(r->regs_in[R_D1], r->regs_in[R_A0]); }
static void run_ReadCrowdRecord(const Record *r) { ReadCrowdRecord(r->regs_in[R_D1], r->regs_in[R_A0]); }
static void run_ReadSRAM(const Record *r) { ReadSRAM(r->regs_in[R_D0], r->regs_in[R_D1], r->regs_in[R_A0]); }
static void run_WriteSRAM(const Record *r) { WriteSRAM(r->regs_in[R_D0], r->regs_in[R_D1], r->regs_in[R_A0]); }
static void run_GetPlayerCount(const Record *r) { expect_reg16(r, R_D0, GetPlayerCount(TEAM(R_A2)), "d0"); }
static void run_GetGoalieCount(const Record *r) { expect_reg16(r, R_D0, GetGoalieCount(TEAM(R_A2)), "d0"); }
static void run_ReloadEnergyBarTiles(const Record *r) { expect_reg16(r, R_D4, ReloadEnergyBarTiles(), "d4"); }
static void run_ReloadCrowdTiles(const Record *r) { expect_reg16(r, R_D4, ReloadCrowdTiles(), "d4"); }
static void run_NextShooter(const Record *r) { NextShooter(OBJ(R_A3)); }
static void run_Set4WayPlayerStub(const Record *r) { Set4WayPlayerStub(OBJ(R_A3)); }
static void run_PrintMenuItem(const Record *r) { expect_reg16(r, R_A1, (uint16_t)PrintMenuItem(r->regs_in[R_A1]), "a1"); }
static void run_UpdateMenuSelection(const Record *r) { expect_reg16(r, R_D1, UpdateMenuSelection(), "d1"); }
static void run_DrawMenuScreen(const Record *r) { expect_reg16(r, R_D1, DrawMenuScreen(), "d1"); }
static void run_HandleMenuInput(const Record *r) { uint16_t d1 = R16(R_D1); expect_z_only(r, HandleMenuInput(&d1)); expect_reg16(r, R_D1, d1, "d1"); }
static void run_PutTeamBlock(const Record *r) { PutTeamBlock(R16(R_D0)); }
V(ChooseSong) V(CheckScoreLeader) V(LeadSong) V(ClearLeadSong) V(rtsChgplayer) V(ClearPenalties) V(SetSflags8Bit1) V(box) V(ShowGoalBox)
V(PenaltyShotBox) V(ShowThreeStars) V(ShowInjuryBox) V(CountGoalies) V(ReloadRinkGraphics) V(PeriodOver) V(resetplstuff) V(SprSort)
V(setupEASNmap) V(setupIceRinkPal) V(setplayercolors) V(ShootoutModeBox) V(CountShootoutGoals) V(ShootoutWonBy) V(SetMenuPrintX) V(vcountwait)


/* ---- sound94 ---- */
#define SLOT(reg) ram_at(r->regs_in[reg])
static void run_play_sfx_or_music_track(const Record *r) { play_sfx_or_music_track(R16(R_D0)); }
static void run_ReadPad4Way1(const Record *r) { expect_reg16(r, R_D0, ReadPad4Way1(), "d0"); }
static void run_ReadPad4Way2(const Record *r) { expect_reg16(r, R_D0, ReadPad4Way2(), "d0"); }
static void run_ReadPad4Way3(const Record *r) { expect_reg16(r, R_D0, ReadPad4Way3(), "d0"); }
static void run_ReadPad4Way4(const Record *r) { expect_reg16(r, R_D0, ReadPad4Way4(), "d0"); }
static void run_ReadPad4WayPort1(const Record *r) { expect_reg16(r, R_D0, ReadPad4WayPort1(), "d0"); }
static void run_ReadPad1(const Record *r) { expect_reg16(r, R_D0, (r->regs_out[R_D0] & 0xFF00) | ReadPad1(), "d0"); }
static void run_ReadPad2(const Record *r) { expect_reg16(r, R_D0, (r->regs_out[R_D0] & 0xFF00) | ReadPad2(), "d0"); }
static void run_ReadPadA0(const Record *r) { expect_reg16(r, R_D0, (r->regs_out[R_D0] & 0xFF00) | ReadPadA0(r->regs_in[R_A0] & 0xFFFFFF), "d0"); }
static void run_ReadPad3Button(const Record *r) { expect_reg16(r, R_D0, (r->regs_out[R_D0] & 0xFF00) | ReadPad3Button(r->regs_in[R_A0] & 0xFFFFFF), "d0"); }
static void run_ProcessOneMusicTrack(const Record *r) { ProcessOneMusicTrack(SLOT(R_A5), R16(R_D7)); }
#define CMDH(name) static void run_##name(const Record *r) { name(r->regs_in[R_A0], R16(R_D7)); }
CMDH(handle_command_00) CMDH(handle_command_10) CMDH(handle_command_30) CMDH(handle_command_40) CMDH(handle_command_60) CMDH(handle_command_skip)
static void run_ReleaseChannelAndNote(const Record *r) { ReleaseChannelAndNote(SLOT(R_A2)); }
static void run_UpdateChannelFrequencyAndVolume(const Record *r) { UpdateChannelFrequencyAndVolume(SLOT(R_A2), SLOT(R_A3)); }
static void run_SetChannelVolume(const Record *r) { SetChannelVolume(SLOT(R_A2), SLOT(R_A3)); }
V(p_turnoff) V(rtsfx) V(rtscmd10) V(ReadJoyData)
static void run_stop_current_song(const Record *r) { stop_current_song(R16(R_D0)); } V(ResetZ80Bus) V(p_music_vblank) V(z80_bus_release_delay)
V(UploadCommandBufferToZ80) V(StopPCMChannel) V(p_initialZ80) V(ClearAllTrackAndSFXSlots)


/* ---- the game loop: hockey94 drivers and the period start ---- */
static void run_defaultsprites(const Record *r) { expect_reg16(r, R_D4, defaultsprites(R16(R_D4)), "d4"); }
static void run_defaultsprites2(const Record *r) { expect_reg16(r, R_D4, defaultsprites2(R16(R_D4)), "d4"); }
static void run_setupTeamBlocksMap(const Record *r) { expect_reg16(r, R_D4, setupTeamBlocksMap(R16(R_D4)), "d4"); }
static void run_CopyTeamBlockMapData(const Record *r)
{
    expect_reg16(r, R_A1, (uint16_t)rp_addr(CopyTeamBlockMapData(r->regs_in[R_A0], R16(R_D0), R16(R_D1), PLACE(R_A1))), "a1");
}
static void run_ReloadRefHorTiles(const Record *r) { expect_reg16(r, R_D4, ReloadRefHorTiles(), "d4"); }
static void run_InitMenuState(const Record *r) { expect_reg16(r, R_D1, InitMenuState(r->regs_in[R_A0], r->regs_in[R_A1]), "d1"); }
static void run_never(const Record *r) { replay_fail("this routine does not return; its record cannot be run"); }
V(DoGameFrame) V(Pausemode) V(setupice) V(setSlotBit)


/* ---- the intermission, the highlights, the replay, the stats screens, the save records ---- */
static void run_StartHL(const Record *r) { StartHL(PLACE(R_A0), D16(R_D1)); }
static void run_StartHL2(const Record *r) { StartHL2(PLACE(R_A0), D16(R_D1)); }
static void run_InitTeamStructure(const Record *r) { InitTeamStructure(R16(R_D0), TEAM(R_A2)); }
static void run_ReloadRefTiles(const Record *r) { expect_reg16(r, R_D4, ReloadRefTiles(), "d4"); }
static void run_ReloadFaceOffTiles(const Record *r) { expect_reg16(r, R_D4, ReloadFaceOffTiles(), "d4"); }
static void run_ReloadFaceOffMap(const Record *r) { expect_reg16(r, R_D4, ReloadFaceOffMap(), "d4"); }
static void run_SetupScreen(const Record *r) { expect_reg16(r, R_D4, SetupScreen(R16(R_D0), R16(R_D1)), "d4"); }
static void run_WaitVSyncAndReadInput(const Record *r) { uint16_t d0, d2, d3; expect_reg16(r, R_D1, WaitVSyncAndReadInput(&d0, &d2, &d3), "d1"); }
static void run_PlayerRecordIO(const Record *r) { PlayerRecordIO(r->regs_in[R_D0], R16(R_D1), r->regs_in[R_A0]); }
static void run_ReadPlayerRecord(const Record *r) { ReadPlayerRecord(r->regs_in[R_D0], R16(R_D1), r->regs_in[R_A0]); }
static void run_WritePlayerRecord(const Record *r) { WritePlayerRecord(r->regs_in[R_D0], R16(R_D1), r->regs_in[R_A0]); }
static void run_TeamRecordIO(const Record *r) { TeamRecordIO(r->regs_in[R_D1], r->regs_in[R_A0]); }
static void run_ReadTeamRecord(const Record *r) { ReadTeamRecord(r->regs_in[R_D1], r->regs_in[R_A0]); }
static void run_WriteTeamRecord(const Record *r) { WriteTeamRecord(r->regs_in[R_D1], r->regs_in[R_A0]); }
static void run_WriteCrowdRecord(const Record *r) { WriteCrowdRecord(r->regs_in[R_D1], r->regs_in[R_A0]); }
static void run_UpdatePlayerRecords(const Record *r) { UpdatePlayerRecords(r->regs_in[R_D1], r->regs_in[R_A0], TEAM(R_A2), R16(R_D5), R16(R_D6)); }
static void run_UpdateTeamRecord(const Record *r) { UpdateTeamRecord(r->regs_in[R_A0], r->regs_in[R_D1], R16(R_D2), R16(R_D3), R16(R_D4), TEAM(R_A1), TEAM(R_A2)); }
static void run_UpdateCrowdRecord(const Record *r) { UpdateCrowdRecord(r->regs_in[R_A0], r->regs_in[R_D1], R16(R_D2), R16(R_D4), R16(R_D5), R16(R_D6), TEAM(R_A1)); }
V(Intermission) V(DoHiLights) V(setupice_highlight) V(clearTeamStats) V(setteams) V(ReplayMode) V(PeriodStatsScreen) V(GameStatisticsScreen)
V(rtsStatTables) V(ExitStatsScreen) V(AddPlayoffTeamStats) V(ReadTeamStats) V(CountPlayers) V(UpdateRecords) V(MakeSRAMChecksum)


/* ---- power on and the front end ---- */
static void run_SegaInit(const Record *r) { SegaInit(); }
static void run_ValidationRoutine(const Record *r) { ValidationRoutine(); }
static void run_EASportsScreen(const Record *r) { EASportsScreen(); }
static void run_Detect4WayPlay(const Record *r) { Detect4WayPlay(); }
static void run_DefaultMenus(const Record *r) { DefaultMenus(); }
static void run_InitSaveRAM(const Record *r) { InitSaveRAM(); }
static void run_SRAMWaitVblank(const Record *r) { SRAMWaitVblank(); }
static void run_VBcount(const Record *r) { VBcount(); }
static void run_ValidateSRAM(const Record *r) { ValidateSRAM(); }
static void run_ClearSRAM(const Record *r) { ClearSRAM(); }
static void run_ReadLineData(const Record *r) { ReadLineData(); }
static void run_HiScoreScreen(const Record *r) { HiScoreScreen(); }
static void run_newTitleScreen(const Record *r) { newTitleScreen(); }
static void run_CreditsPrintRow(const Record *r) { expect_reg16(r, R_A1, (uint16_t)CreditsPrintRow(r->regs_in[R_A1]), "a1"); }
static void run_CreditsWait(const Record *r) { CreditsWait(); }
static void run_VBlank_TitleScreen(const Record *r) { VBlank_TitleScreen(); }
static void run_CreditsLineScroll(const Record *r) { CreditsLineScroll(); }
static void run_CreditsScrollStep(const Record *r) { CreditsScrollStep(); }

/* ---- the game setup screen (optsetup94 and its helpers in data94 / cards94 / attract94 ...) ---- */
#define A(reg) (r->regs_in[reg])
static void run_SuperDiv(const Record *r) { expect_reg16(r, R_D0, SuperDiv(ram_at(A(R_A3)), R16(R_D0)), "d0"); }
static void run_ReadPassBits(const Record *r) { ReadPassBits(ram_at(A(R_A3))); }
static void run_GetRandomUnusedTeam(const Record *r)
{
    uint32_t used = A(R_D3);
    expect_reg16(r, R_D0, GetRandomUnusedTeam(&used), "d0");
    expect_reg32(r, R_D3, used, "d3");
}
static void run_InitializeGameStructures(const Record *r) { InitializeGameStructures(); }
static void run_FigureJoy(const Record *r) { FigureJoy(); }
static void run_maketree(const Record *r) { maketree(); }
static void run_NewPO(const Record *r) { NewPO(); }
static void run_ContinuePlayoffs(const Record *r) { ContinuePlayoffs(); }
static void run_InitShooters(const Record *r) { InitShooters(); }
static void run_ClearShootout(const Record *r) { ClearShootout(); }
static void run_UpdatePlayoffLevel(const Record *r) { UpdatePlayoffLevel(); }
static void run_DrawMatchupBitmaps(const Record *r) { DrawMatchupBitmaps(); }
static void run_DrawTeamBitmap(const Record *r) { expect_reg16(r, R_D4, DrawTeamBitmap(R16(R_D1), R16(R_D4)), "d4"); }
static void run_VBlank_SetOptions(const Record *r) { VBlank_SetOptions(); }
static void run_UnpackPicture(const Record *r) { UnpackPicture(A(R_A2)); }
static void run_ResetCardTimer(const Record *r) { ResetCardTimer(); }
static void run_PlayerCardTimer(const Record *r) { PlayerCardTimer(); }
static void run_PlayerCardScreen(const Record *r) { PlayerCardScreen(); }
static void run_PrintRecordValue(const Record *r) { PrintRecordValue(A(R_A1), R16(R_D0), R16(R_D1)); }
static void run_CopyString(const Record *r) { CopyString(A(R_A1), A(R_A3)); }
static void run_AppendNumber(const Record *r) { AppendNumber(R16(R_D0), A(R_A1)); }
static void run_GetRecordValue(const Record *r)
{
    uint16_t goalie, v = GetRecordValue(R16(R_D0), R16(R_D1), &goalie);
    expect_reg16(r, R_D0, v, "d0");
    expect_reg16(r, R_D1, goalie, "d1");
}
static void run_PrintRecordHolder(const Record *r) { PrintRecordHolder(A(R_A1), R16(R_D0), R16(R_D1), A(R_A0)); }
static void run_PrintRecordVs(const Record *r) { PrintRecordVs(A(R_A1), R16(R_D0), R16(R_D1), A(R_A0)); }
static void run_AppendRecordHolder(const Record *r) { AppendRecordHolder(A(R_A1), R16(R_D0), R16(R_D1), A(R_A0)); }
static void run_AppendRecordVs(const Record *r) { AppendRecordVs(A(R_A1), R16(R_D0), R16(R_D1), A(R_A0)); }
static void run_ReadNameLog(const Record *r) { ReadNameLog(); }
static void run_NameLogIO(const Record *r) { NameLogIO(); }
static void run_AppendUserName(const Record *r) { AppendUserName(A(R_A1), R16(R_D2)); }
static void run_AppendTeamName(const Record *r) { AppendTeamName(R16(R_D0), A(R_A1)); }
static void run_TrimSpaces(const Record *r) { TrimSpaces(A(R_A2)); }
static void run_GameSetUp(const Record *r) { GameSetUp(); }
static void run_GoNewPlayoffs(const Record *r) { GoNewPlayoffs(); }
static void run_GoContinuePlayoffs(const Record *r) { GoContinuePlayoffs(); }
static void run_MoveMenuFrame(const Record *r) { expect_reg16(r, R_D7, MoveMenuFrame(R16(R_D7), D16(R_D0)), "d7"); }
static void run_SetMenuArea(const Record *r) { uint16_t w, h; SetMenuArea(&w, &h); expect_reg16(r, R_D0, w, "d0"); expect_reg16(r, R_D1, h, "d1"); }
static void run_SetFrameRect(const Record *r) { uint16_t w, h; SetFrameRect(R16(R_D3), &w, &h); expect_reg16(r, R_D0, w, "d0"); expect_reg16(r, R_D1, h, "d1"); }
static void run_rtsSetup(const Record *r) { rtsSetup(); }
static void run_FixModeOptions(const Record *r) { FixModeOptions(R16(R_D7)); }
static void run_ChangeOption(const Record *r) { ChangeOption(R16(R_D7), D16(R_D2)); }
static void run_WrapOption(const Record *r) { expect_reg16(r, R_D3, WrapOption(R16(R_D3), D16(R_D2), R16(R_D4), R16(R_D5)), "d3"); }
static void run_PrintOptions(const Record *r) { PrintOptions(R16(R_D7)); }
static void run_PrintOptionValue(const Record *r) { PrintOptionValue(R16(R_D7)); }
static void run_PrintScrollMarks(const Record *r) { PrintScrollMarks(); }
static void run_PrintOptTeamName(const Record *r) { PrintOptTeamName(R16(R_D7)); }
static void run_PrintOptionNames(const Record *r) { PrintOptionNames(); }
static void run_setoptions(const Record *r) { setoptions(); }
static void run_ReadSetupPads(const Record *r) { expect_reg16(r, R_D1, ReadSetupPads(), "d1"); }
static void run_SetupDemo(const Record *r) { SetupDemo(); }
static void run_SetupStart(const Record *r) { SetupStart(); }
static void run_LoadSetupTiles(const Record *r) { expect_reg16(r, R_D4, LoadSetupTiles(), "d4"); }
static void run_UpdateRosterPlayers(const Record *r) { UpdateRosterPlayers(); }
static void run_EraseCard(const Record *r) { EraseCard(); }
static void run_DrawHomeBlock(const Record *r) { DrawHomeBlock(); }
static void run_DrawVisBlock(const Record *r) { DrawVisBlock(); }
static void run_DrawTeamLogo(const Record *r) { DrawTeamLogo(); }
static void run_LogoBoxRight(const Record *r) { LogoBoxRight(); }
static void run_LogoBoxHome(const Record *r) { LogoBoxHome(); }
static void run_LogoBoxLeft(const Record *r) { LogoBoxLeft(); }
static void run_LogoBoxVis(const Record *r) { LogoBoxVis(); }
static void run_DrawLogoBox(const Record *r) { DrawLogoBox(R16(R_D4)); }

/* ---- the name entry (records94) ---- */
static void run_UserNameEntry(const Record *r) { UserNameEntry(); }
static void run_NameEntryScreen(const Record *r) { NameEntryScreen(); }
static void run_NameLetterGrid(const Record *r) { NameLetterGrid(); }
static void run_NameEntryHelp(const Record *r) { NameEntryHelp(); }
static void run_PrintNamePrompt(const Record *r) { PrintNamePrompt(); }
static void run_SyncLetterCursor(const Record *r) { expect_reg16(r, R_D5, SyncLetterCursor(), "d5"); }
static void run_FindLetter(const Record *r) { expect_reg16(r, R_D0, FindLetter(R16(R_D0)), "d0"); }
static void run_PrintNameCursor(const Record *r) { PrintNameCursor(R16(R_D4)); }
static void run_rtsNameCursor(const Record *r) { rtsNameCursor(); }
static void run_PrintCurLetter(const Record *r) { PrintCurLetter(R16(R_D5)); }
static void run_LetterGridPos(const Record *r) { uint16_t w, h; LetterGridPos(R16(R_D5), &w, &h); expect_reg16(r, R_D0, w, "d0"); expect_reg16(r, R_D1, h, "d1"); }
static void run_PrintNameLog(const Record *r) { PrintNameLog(); }
static void run_MoveLogArrows(const Record *r) { MoveLogArrows(); }
static void run_PrintLogArrows(const Record *r) { PrintLogArrows(A(R_A1)); }
static void run_GetNameLength(const Record *r) { GetNameLength(); }
static void run_PrintNameField(const Record *r) { PrintNameField(A(R_A1)); }
static void run_NameEntryFramer(const Record *r) { NameEntryFramer(R16(R_D0), R16(R_D1)); }
static void run_GetLogName(const Record *r) { GetLogName(A(R_A1)); }
static void run_StoreUserName(const Record *r) { StoreUserName(); }
static void run_WriteNameRecord(const Record *r) { WriteNameRecord(); }
static void run_SkipOtherUserName(const Record *r)
{
    int stepped = SkipOtherUserName(D16(R_D0));           /* -> Z */
    expect_reg16(r, R_SR, (uint16_t)((r->regs_out[R_SR] & ~4u) | (stepped ? 4 : 0)), "sr (Z)");
}
static void run_WriteNameLog(const Record *r) { WriteNameLog(); }

/* ---- the playoff screen (setup94) ---- */
static void run_PlayoffScreen(const Record *r) { PlayoffScreen(); }
static void run_HandlePlayoffInput(const Record *r) { HandlePlayoffInput(); }
static void run_UpdatePlayoffScroll(const Record *r) { UpdatePlayoffScroll(); }
static void run_PlayoffScreen_waitvsync(const Record *r) { PlayoffScreen_waitvsync(); }
static void run_PrintSeriesScore(const Record *r) { PrintSeriesScore(ram_at(A(R_A2))); }
static void run_DrawTreeArrow(const Record *r) { DrawTreeArrow(R16(R_D0)); }
static void run_DrawTeamBlocks(const Record *r) { DrawTeamBlocks(R16(R_D1)); }
static void run_VBlank_Playoff(const Record *r) { VBlank_Playoff(); }
static void run_PlayoffScreenExit(const Record *r) { PlayoffScreenExit(); }   /* (pops its caller's return: abnormal, never run) */
static void run_DrawPlayoffSprite(const Record *r) { DrawPlayoffSprite(); }

/* ---- the scouting report (scout94, the text player of setup94, cards94 / title94 helpers) ---- */
static void run_ScoutingReport(const Record *r) { ScoutingReport(); }
static void run_ScoutCrowdRecord(const Record *r) { expect_reg32(r, R_A1, ScoutCrowdRecord(), "a1"); }
static void run_PageMatchup(const Record *r) { PageMatchup(D16(R_D0)); }
static void run_PrintMatchupRating(const Record *r) { expect_reg16(r, R_D0, PrintMatchupRating(TEAM(R_A0)), "d0"); }
static void run_PrintTwoSpaces(const Record *r) { PrintTwoSpaces(); }
static void run_DrawMatchupPictures(const Record *r) { DrawMatchupPictures(); }
static void run_DrawMatchupPicture(const Record *r) { DrawMatchupPicture(A(R_A0), R16(R_D4), R16(R_D5)); }
static void run_DrawMatchupLogo(const Record *r) { DrawMatchupLogo(A(R_A0), R16(R_D3), R16(R_D4), R16(R_D5)); }
static void run_GetTeamLogo(const Record *r) { uint16_t d3 = R16(R_D3); expect_reg32(r, R_A0, GetTeamLogo(&d3), "a0"); expect_reg16(r, R_D3, d3, "d3"); }
static void run_PrintMatchupRatings(const Record *r) { PrintMatchupRatings(); }
static void run_GetMatchupPlayers(const Record *r) { GetMatchupPlayers(); }
static void run_GetMatchupPlayer(const Record *r) { expect_reg16(r, R_D0, GetMatchupPlayer(TEAM(R_A0)), "d0"); }
static void run_RestartAdvantageMarks(const Record *r) { RestartAdvantageMarks(); }
static void run_PrintAdvantageMarks(const Record *r) { PrintAdvantageMarks(); }
static void run_GetTeamNickname(const Record *r) { expect_reg32(r, R_A1, GetTeamNickname(TEAM(R_A2)), "a1"); }
static void run_GetTeamArena(const Record *r) { expect_reg32(r, R_A1, GetTeamArena(TEAM(R_A2)), "a1"); }
static void run_InitScoutingDisplay(const Record *r) { expect_reg16(r, R_A0, (uint16_t)rp_addr(InitScoutingDisplay()), "a0"); }
static void run_UpdateScoutingDisplay(const Record *r) { UpdateScoutingDisplay(); }
static void run_ScrollDisplayUp(const Record *r) { ScrollDisplayUp(); }
static void run_PlayedByHome(const Record *r) { expect_reg32(r, R_A1, PlayedByHome(), "a1"); }
static void run_PlayedByAway(const Record *r) { expect_reg32(r, R_A1, PlayedByAway(), "a1"); }
static void run_AttribAdjust(const Record *r) { expect_reg16(r, R_D0, AttribAdjust(R16(R_D0)), "d0"); }
static void run_DrawPlayerPicture(const Record *r) { expect_reg32(r, R_A0, DrawPlayerPicture(R16(R_D0), R16(R_D1)), "a0"); }
static void run_DrawMatchupBitmap(const Record *r) { DrawMatchupBitmap(A(R_A0), A(R_A1), A(R_A2), R16(R_D4), R16(R_D5)); }
static void run_PlayedByText(const Record *r) { expect_reg32(r, R_A1, PlayedByText(R16(R_D0)), "a1"); }
static void run_PlayedByNone(const Record *r) { expect_reg32(r, R_A1, PlayedByNone(), "a1"); }
static void run_PlayedByExit(const Record *r) { expect_reg32(r, R_A1, PlayedByExit(A(R_A1)), "a1"); }
static void run_CalcAttrib(const Record *r)
{
    uint16_t div, v = CalcAttrib(R16(R_D0), TEAM(R_A2), A(R_D4), &div);
    expect_reg16(r, R_D0, v, "d0");
    expect_reg16(r, R_D1, div, "d1");
}

/* ---- the game over: the playoff password, the player stats lists ---- */
static void run_GameOver(const Record *r) { GameOver(); }
static void run_ClrPassBits(const Record *r) { ClrPassBits(ram_at(A(R_A3))); }
static void run_SuperAdd(const Record *r) { SuperAdd(ram_at(A(R_A3)), A(R_D0)); }
static void run_SuperMult(const Record *r) { SuperMult(ram_at(A(R_A3)), R16(R_D0)); }
static void run_PushBits(const Record *r) { PushBits(ram_at(A(R_A3)), R16(R_D0), R16(R_D1)); }
static void run_WritePassBits(const Record *r) { WritePassBits(ram_at(A(R_A3))); }
static void run_ResolveGames(const Record *r) { ResolveGames(); }
static void run_EncodePW(const Record *r) { EncodePW(); }
static void run_WriteLineData(const Record *r) { WriteLineData(); }
static void run_PlayoffStatsScreen(const Record *r) { PlayoffStatsScreen(); }
static void run_PlayerStatsScreen(const Record *r) { PlayerStatsScreen(TEAM(R_A2)); }
static void run_PlayerStatsList(const Record *r) { PlayerStatsList(TEAM(R_A2), R16(R_D7) != 0); }
static void run_UpdateStatsListScroll(const Record *r) { UpdateStatsListScroll(TEAM(R_A2), R16(R_D7) != 0); }
static void run_SetStatsListScrollReg(const Record *r) { SetStatsListScrollReg(); }
static void run_DrawStatsRowUp(const Record *r) { DrawStatsRowUp(TEAM(R_A2), R16(R_D7) != 0); }
static void run_DrawStatsRowDown(const Record *r) { DrawStatsRowDown(TEAM(R_A2), R16(R_D7) != 0); }
static void run_DrawStatsList(const Record *r) { DrawStatsList(TEAM(R_A2), R16(R_D7) != 0); }
static void run_DrawStatsRow(const Record *r) { DrawStatsRow(R16(R_D3), TEAM(R_A2), R16(R_D7) != 0); }
static void run_GetStatColumn(const Record *r) { expect_reg16(r, R_D2, GetStatColumn(R16(R_D0), R16(R_D1), TEAM(R_A2), R16(R_D7) != 0), "d2"); }
static void run_ReadStatColumn(const Record *r) { expect_reg16(r, R_D2, ReadStatColumn(R16(R_D0), R16(R_D1), TEAM(R_A2), R16(R_D7) != 0), "d2"); }
static void run_GetStatsValue(const Record *r) { expect_reg16(r, R_D2, GetStatsValue(R16(R_D1), R16(R_D0), R16(R_D2)), "d2"); }
static void run_DrawStatsListArrows(const Record *r) { DrawStatsListArrows(); }

/* ---- the menu screens (stats94, cards94, records94, shootout94; data94 / title94 helpers) ---- */
static void run_ShowScores(const Record *r) { ShowScores(); }
static void run_UpdateScoresScroll(const Record *r) { UpdateScoresScroll(); }
static void run_UpdateVertScrollReg(const Record *r) { UpdateVertScrollReg(); }
static void run_DisplayGameInfo(const Record *r) { DisplayGameInfo(R16(R_D3)); }
static void run_PrintGameInfo(const Record *r) { PrintGameInfo(R16(R_D0), R16(R_D1)); }
static void run_DrawScrollArrows(const Record *r) { DrawScrollArrows(); }
static void run_LineEditor(const Record *r) { LineEditor(TEAM(R_A2)); }
static void run_DrawLineIcon(const Record *r) { DrawLineIcon(R16(R_D5), TEAM(R_A2)); }
static void run_ClearLineArea(const Record *r) { ClearLineArea(); }
static void run_DrawTeamScreen(const Record *r) { DrawTeamScreen(TEAM(R_A2)); }
static void run_ClearLineEditorTop(const Record *r) { ClearLineEditorTop(); }
static void run_ClearMenuFlags(const Record *r) { ClearMenuFlags(); }
static void run_SetLineSlot(const Record *r) { SetLineSlot((uint8_t)R16(R_D0), R16(R_D2), TEAM(R_A2)); }
static void run_DecodeTeamLines(const Record *r) { DecodeTeamLines(TEAM(R_A2)); }
static void run_EncodeTeamLines(const Record *r) { EncodeTeamLines(TEAM(R_A2)); }
static void run_TeamRosterScreen(const Record *r) { TeamRosterScreen(TEAM(R_A2)); }
static void run_CheckPlayerListScroll(const Record *r) { CheckPlayerListScroll(TEAM(R_A2)); }
static void run_UpdatePlayerListScroll(const Record *r) { UpdatePlayerListScroll(R16(R_D0), TEAM(R_A2)); }
static void run_StopPlayerListScroll(const Record *r) { StopPlayerListScroll(); }
static void run_DisplayPlayerListUp(const Record *r) { DisplayPlayerListUp((uint16_t)(A(R_D0) >> 16), TEAM(R_A2)); }
static void run_DisplayPlayerListDown(const Record *r) { DisplayPlayerListDown((uint16_t)(A(R_D0) >> 16), TEAM(R_A2)); }
static void run_DisplayPlayerList(const Record *r) { DisplayPlayerList(TEAM(R_A2)); }
static void run_GetNameandAttrib(const Record *r) { GetNameandAttrib(R16(R_D0), A(R_D4), TEAM(R_A2)); }
static void run_AttribStatus(const Record *r) { AttribStatus(R16(R_D0), TEAM(R_A2)); }
static void run_AttribEnergy(const Record *r) { AttribEnergy(R16(R_D0), TEAM(R_A2)); }
static void run_AttribFighting(const Record *r) { AttribFighting(R16(R_D0), R16(R_D1)); }
static void run_AttribRating(const Record *r) { AttribRating(R16(R_D0), R16(R_D1)); }
static void run_AttribPrintPct(const Record *r) { AttribPrintPct(R16(R_D0)); }
static void run_AttribHanded(const Record *r) { AttribHanded(R16(R_D0)); }
static void run_AttribWeight(const Record *r) { AttribWeight(R16(R_D0)); }
static void run_ScoringSummaryScreen(const Record *r) { ScoringSummaryScreen(); }
static void run_CheckScoringScroll(const Record *r) { CheckScoringScroll(); }
static void run_UpdateScoringScroll(const Record *r) { UpdateScoringScroll(R16(R_D0)); }
static void run_DrawScoringRowUp(const Record *r) { DrawScoringRowUp((uint16_t)(A(R_D0) >> 16)); }
static void run_DrawScoringRowDown(const Record *r) { DrawScoringRowDown((uint16_t)(A(R_D0) >> 16)); }
static void run_DrawScoringRow(const Record *r) { DrawScoringRow(R16(R_D3)); }
static void run_PrintScorerName(const Record *r) { PrintScorerName((uint8_t)R16(R_D0), TEAM(R_A2)); }
static void run_PenaltySummaryScreen(const Record *r) { PenaltySummaryScreen(); }
static void run_CheckPenaltyScroll(const Record *r) { CheckPenaltyScroll(); }
static void run_UpdatePenaltyScroll(const Record *r) { UpdatePenaltyScroll(R16(R_D0)); }
static void run_DisplayPenaltyLineUp(const Record *r) { DisplayPenaltyLineUp((uint16_t)(A(R_D0) >> 16)); }
static void run_DisplayPenaltyLineDown(const Record *r) { DisplayPenaltyLineDown((uint16_t)(A(R_D0) >> 16)); }
static void run_DisplayPenaltyEntry(const Record *r) { DisplayPenaltyEntry(R16(R_D3)); }
static void run_DrawScrollArrowsPenalty(const Record *r) { DrawScrollArrowsPenalty(); }
static void run_CrowdMeterScreen(const Record *r) { CrowdMeterScreen(); }
static void run_PrintCrowdMeter(const Record *r) { PrintCrowdMeter(); }
static void run_PrintCrowdDB(const Record *r) { PrintCrowdDB(R16(R_D0)); }
static void run_TimeoutMenu(const Record *r) { TimeoutMenu(TEAM(R_A2)); }
static void run_SelectGoalieMenu(const Record *r) { SelectGoalieMenu(TEAM(R_A2)); }
static void run_DrawGoalieSelectList(const Record *r) { DrawGoalieSelectList(TEAM(R_A2)); }
static void run_GetDefenseStart(const Record *r) { expect_reg16(r, R_D0, GetDefenseStart(TEAM(R_A2)), "d0"); }
static void run_GetForwardCount(const Record *r) { expect_reg16(r, R_D0, GetForwardCount(TEAM(R_A2)), "d0"); }
static void run_PlayerCards(const Record *r) { PlayerCards(); }
static void run_WrapCardIndex(const Record *r) { WrapCardIndex((uint16_t)A(R_A0)); }
static void run_DrawPlayerCard(const Record *r) { DrawPlayerCard((uint16_t)A(R_A0)); }
static void run_PrintCardRecords(const Record *r) { PrintCardRecords(); }
static void run_ClearCardText(const Record *r) { ClearCardText(R16(R_D0), R16(R_D1)); }
#define CARD_LINE(fn) static void run_##fn(const Record *r) { fn(R16(R_D0), R16(R_D1), (uint8_t)R16(R_D2), (uint8_t)R16(R_D3), (uint8_t)R16(R_D4), (uint8_t)R16(R_D5)); }
CARD_LINE(PrintCardRecordLine)
CARD_LINE(PrintCrowdRecordLine)
CARD_LINE(PrintSaveRecordLine)
CARD_LINE(PrintGoalRecordLine)
static void run_PrintOverallRating(const Record *r) { PrintOverallRating(R16(R_D0), R16(R_D1), R16(R_D2)); }
static void run_PrintStartingLine(const Record *r) { PrintStartingLine(R16(R_D0), R16(R_D1)); }
static void run_PrintCardPosition(const Record *r) { PrintCardPosition(R16(R_D0), R16(R_D1), R16(R_D2)); }
static void run_PrintTeamRating(const Record *r) { PrintTeamRating(R16(R_D0), R16(R_D1)); }
static void run_PrintCardTeam(const Record *r) { PrintCardTeam(R16(R_D1)); }
static void run_PrintCentered(const Record *r) { PrintCentered(A(R_A0)); }
static void run_GetJerseyString(const Record *r) { GetJerseyString(R16(R_D0), A(R_A1)); }
static void run_PrintGoalsAssists(const Record *r) { PrintGoalsAssists(R16(R_D0), R16(R_D1), R16(R_D2)); }
static void run_PrintSaves(const Record *r) { PrintSaves(R16(R_D0), R16(R_D1), R16(R_D2)); }
static void run_DrawPictureBox(const Record *r) { DrawPictureBox(); }
static void run_GetCardTeam(const Record *r) { expect_reg32(r, R_A0, rp_addr(team_place(GetCardTeam())), "a0"); }
static void run_GetCardPlayer(const Record *r) { expect_reg16(r, R_D0, GetCardPlayer(), "d0"); }
static void run_BuildCardPlayerLists(const Record *r) { BuildCardPlayerLists(); }
static void run_BuildCardPlayerList(const Record *r) { BuildCardPlayerList(TEAM(R_A0), ram_at(A(R_A1))); }
static void run_RecordHoldersScreen(const Record *r) { RecordHoldersScreen(); }
static void run_PrintRecordPage(const Record *r) { PrintRecordPage(); }
static void run_PrintRecordTitles(const Record *r) { PrintRecordTitles(); }
static void run_PrintWinRecords(const Record *r) { PrintWinRecords(); }
static void run_ClearRecordArea(const Record *r) { ClearRecordArea(R16(R_D2)); }
static void run_PrintPlayerRecords(const Record *r) { PrintPlayerRecords(); }
static void run_PrintRecordName(const Record *r) { PrintRecordName(R16(R_D7), m8(A(R_A0) - 1)); }
static void run_CalcWinPercents(const Record *r) { CalcWinPercents(); }
static void run_ReadTeamRecords(const Record *r) { ReadTeamRecords(); }
static void run_ShootoutShooters(const Record *r) { ShootoutShooters(TEAM(R_A2)); }
static void run_PrintShooterList(const Record *r) { PrintShooterList(TEAM(R_A2)); }
static void run_ShootersBackground(const Record *r) { ShootersBackground(TEAM(R_A2)); }
static void run_ClearShooterScreen(const Record *r) { ClearShooterScreen(); }
static void run_ResetShooterScroll(const Record *r) { ResetShooterScroll(); }
static void run_PrintShooterSlots(const Record *r) { PrintShooterSlots(); }
static void run_PrintShooterBox(const Record *r) { PrintShooterBox(TEAM(R_A2)); }
static void run_PrintShooterNames(const Record *r) { PrintShooterNames(TEAM(R_A2)); }
static void run_FormatPlayerNameShort(const Record *r) { expect_reg32(r, R_A1, FormatPlayerNameShort(R16(R_D0), TEAM(R_A2)), "a1"); }
static void run_FormatPlayerNameLast(const Record *r) { expect_reg32(r, R_A1, FormatPlayerNameLast(R16(R_D0), TEAM(R_A2)), "a1"); }
static void run_GetLeagueCrowdRecord(const Record *r) { expect_reg16(r, R_D0, GetLeagueCrowdRecord(), "d0"); }
static void run_ClearWinRecords(const Record *r) { ClearWinRecords(); }
static void run_GetTeamStruct(const Record *r) { expect_reg32(r, R_A2, rp_addr(team_place(GetTeamStruct(R16(R_D2)))), "a2"); }

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
    { SYM_randomd0s, "randomd0s", run_randomd0s },
    { SYM_sroot, "sroot", run_sroot },
    { SYM_sfx, "sfx", run_sfx },
    { SYM_song, "song", run_song },
    { SYM_UnpackNibbles, "UnpackNibbles", run_UnpackNibbles },
    { SYM_WeightedRandomSelect, "WeightedRandomSelect", run_WeightedRandomSelect },
    { SYM_remap, "remap", run_remap },
    { SYM_waitx, "waitx", run_waitx },
    { SYM_waitxsr, "waitxsr", run_waitxsr },
    { SYM_waitjoy, "waitjoy", run_waitjoy },
    { SYM_orjoy, "orjoy", run_orjoy },
    { SYM_nodiag, "nodiag", run_nodiag },
    { SYM_ProcessInputWithRepeat, "ProcessInputWithRepeat", run_ProcessInputWithRepeat },
    { SYM_ReadJoy, "ReadJoy", run_ReadJoy },
    { SYM_ReadJoy1, "ReadJoy1", run_ReadJoy1 },
    { SYM_ReadJoy2, "ReadJoy2", run_ReadJoy2 },
    { SYM_ReadJoy3, "ReadJoy3", run_ReadJoy3 },
    { SYM_ReadJoy4, "ReadJoy4", run_ReadJoy4 },
    { SYM_DoDMApro, "DoDMApro", run_DoDMApro },
    { SYM_DoDMA, "DoDMA", run_DoDMA },
    { SYM_DoDMA_nd2, "DoDMA_nd2", run_DoDMA_nd2 },
    { SYM_DoFill, "DoFill", run_DoFill },
    { SYM_setVram, "setVram", run_setVram },
    { SYM_Vmaddr, "Vmaddr", run_Vmaddr },
    { SYM_dobitmap, "dobitmap", run_dobitmap },
    { SYM_DecompressGraphicsWithCallback, "DecompressGraphicsWithCallback", run_DecompressGraphicsWithCallback },
    { SYM_DecompressGraphicsPlain, "DecompressGraphicsPlain", run_DecompressGraphicsPlain },
    { SYM_DecompressGraphics, "DecompressGraphics", run_DecompressGraphics },
    { SYM_DecompressBytecode, "DecompressBytecode", run_DecompressBytecode },
    { SYM_Opcode_CopyLiteral, "Opcode_CopyLiteral", run_Opcode_CopyLiteral },
    { SYM_Opcode_ClearBytes, "Opcode_ClearBytes", run_Opcode_ClearBytes },
    { SYM_Opcode_Fillbytes, "Opcode_Fillbytes", run_Opcode_Fillbytes },
    { SYM_Opcode_CopyBackwardShort, "Opcode_CopyBackwardShort", run_Opcode_CopyBackwardShort },
    { SYM_Opcode_CopyBackwardMedium, "Opcode_CopyBackwardMedium", run_Opcode_CopyBackwardMedium },
    { SYM_Opcode_CopyBackwardLong, "Opcode_CopyBackwardLong", run_Opcode_CopyBackwardLong },
    { SYM_Opcode_CopyBackwardExtended1, "Opcode_CopyBackwardExtended1", run_Opcode_CopyBackwardExtended1 },
    { SYM_Opcode_CopyBackwardExtended2, "Opcode_CopyBackwardExtended2", run_Opcode_CopyBackwardExtended2 },
    { SYM_Opcode_CopyBackwardReverseShort, "Opcode_CopyBackwardReverseShort", run_Opcode_CopyBackwardReverseShort },
    { SYM_Opcode_CopyBackwardReverseMedium, "Opcode_CopyBackwardReverseMedium", run_Opcode_CopyBackwardReverseMedium },
    { SYM_Opcode_CopyBackwardReverseLong, "Opcode_CopyBackwardReverseLong", run_Opcode_CopyBackwardReverseLong },
    { SYM_CopyBackwardRun, "CopyBackwardRun", run_CopyBackwardRun },
    { SYM_CopyBackwardReverseRun, "CopyBackwardReverseRun", run_CopyBackwardReverseRun },
    { SYM_FlushOutputBuffer, "FlushOutputBuffer", run_FlushOutputBuffer },
    { SYM_eraser, "eraser", run_eraser },
    { SYM_Framer, "Framer", run_Framer },
    { SYM_printsmallz, "printsmallz", run_printsmallz },
    { SYM_printz, "printz", run_printz },
    { SYM_printbigz, "printbigz", run_printbigz },
    { SYM_appendz, "appendz", run_appendz },
    { SYM_printsmall, "printsmall", run_printsmall },
    { SYM_print, "print", run_print },
    { SYM_printbig, "printbig", run_printbig },
    { SYM_appstring, "appstring", run_appstring },
    { SYM_ControlCode_SetMapAndPosition, "ControlCode_SetMapAndPosition", run_ControlCode_SetMapAndPosition },
    { SYM_ControlCode_SetMap, "ControlCode_SetMap", run_ControlCode_SetMap },
    { SYM_ControlCode_SetAttribute, "ControlCode_SetAttribute", run_ControlCode_SetAttribute },
    { SYM_ControlCode_SetX, "ControlCode_SetX", run_ControlCode_SetX },
    { SYM_ControlCode_SetY, "ControlCode_SetY", run_ControlCode_SetY },
    { SYM_ControlCode_AddX, "ControlCode_AddX", run_ControlCode_AddX },
    { SYM_ControlCode_AddY, "ControlCode_AddY", run_ControlCode_AddY },
    { SYM_ControlCode_SetFont, "ControlCode_SetFont", run_ControlCode_SetFont },
    { SYM_FormatAndPrintTime, "FormatAndPrintTime", run_FormatAndPrintTime },
    { SYM_PushTime, "PushTime", run_PushTime },
    { SYM_PushNumber, "PushNumber", run_PushNumber },
    { SYM_PushNumberWidth, "PushNumberWidth", run_PushNumberWidth },
    { SYM_PrintBigChar, "PrintBigChar", run_PrintBigChar },
    { SYM_PutBigTile, "PutBigTile", run_PutBigTile },
    { SYM_AddSmallFont, "AddSmallFont", run_AddSmallFont },
    { SYM_AddFramer, "AddFramer", run_AddFramer },
    { SYM_AddTeamBlock, "AddTeamBlock", run_AddTeamBlock },
    { SYM_forceblack, "forceblack", run_forceblack },
    { SYM_forceblack2, "forceblack2", run_forceblack2 },
    { SYM_forcefade, "forcefade", run_forcefade },
    { SYM_cramfade, "cramfade", run_cramfade },
    { SYM_CopyPaletteToCRAM, "CopyPaletteToCRAM", run_CopyPaletteToCRAM },
    { SYM_WaitDMA, "WaitDMA", run_WaitDMA },
    { SYM_setVram_0, "setVram_0", run_setVram_0 },
    { SYM_xyVmMap, "xyVmMap", run_xyVmMap },
    { SYM_EndOneTimer, "EndOneTimer", run_EndOneTimer },
    { SYM_getFgtbyte, "getFgtbyte", run_getFgtbyte },
    { SYM_chkFgtBit1, "chkFgtBit1", run_chkFgtBit1 },
    { SYM_ReadGoaliePulled, "ReadGoaliePulled", run_ReadGoaliePulled },
    { SYM_ShootoutShootCheck, "ShootoutShootCheck", run_ShootoutShootCheck },
    { SYM_SkatePath, "SkatePath", run_SkatePath },
    { SYM_NextPathPoint, "NextPathPoint", run_NextPathPoint },
    { SYM_StartShootoutPath, "StartShootoutPath", run_StartShootoutPath },
    { SYM_RandomFaceoffAnim, "RandomFaceoffAnim", run_RandomFaceoffAnim },
    { SYM_LeadSongExit, "LeadSongExit", run_LeadSongExit },
    { SYM_chgplayer, "chgplayer", run_chgplayer },
    { SYM_swpchk, "swpchk", run_swpchk },
    { SYM_j_setc1player, "j_setc1player", run_j_setc1player },
    { SYM_j_setc2player, "j_setc2player", run_j_setc2player },
    { SYM_CountButtonPress, "CountButtonPress", run_CountButtonPress },
    { SYM_PSandSOpassdir, "PSandSOpassdir", run_PSandSOpassdir },
    { SYM_LoadHomeTeamGfx, "LoadHomeTeamGfx", run_LoadHomeTeamGfx },
    { SYM_HotColdIcon, "HotColdIcon", run_HotColdIcon },
    { SYM_clrTmPdst, "clrTmPdst", run_clrTmPdst },
    { SYM_PlaceBoardFall, "PlaceBoardFall", run_PlaceBoardFall },
    { SYM_PrintParenNumber, "PrintParenNumber", run_PrintParenNumber },
    { SYM_PrintPlayerGoals, "PrintPlayerGoals", run_PrintPlayerGoals },
    { SYM_PrintPlayerAssists, "PrintPlayerAssists", run_PrintPlayerAssists },
    { SYM_CalcStarScores, "CalcStarScores", run_CalcStarScores },
    { SYM_CalcTeamStarScores, "CalcTeamStarScores", run_CalcTeamStarScores },
    { SYM_TakeTopStarScore, "TakeTopStarScore", run_TakeTopStarScore },
    { SYM_GetPlayerNamePointer, "GetPlayerNamePointer", run_GetPlayerNamePointer },
    { SYM_ConvertByteToDigits, "ConvertByteToDigits", run_ConvertByteToDigits },
    { SYM_FinalizeTextBuffer, "FinalizeTextBuffer", run_FinalizeTextBuffer },
    { SYM_FormatPlayerName, "FormatPlayerName", run_FormatPlayerName },
    { SYM_FormatPlayerNumInitial, "FormatPlayerNumInitial", run_FormatPlayerNumInitial },
    { SYM_getname, "getname", run_getname },
    { SYM_GetPlayerName, "GetPlayerName", run_GetPlayerName },
    { SYM_GetPlayerNumInitial, "GetPlayerNumInitial", run_GetPlayerNumInitial },
    { SYM_GetShifter, "GetShifter", run_GetShifter },
    { SYM_wallcollduringcheck, "wallcollduringcheck", run_wallcollduringcheck },
    { SYM_CrowdRecordIO, "CrowdRecordIO", run_CrowdRecordIO },
    { SYM_ReadCrowdRecord, "ReadCrowdRecord", run_ReadCrowdRecord },
    { SYM_ReadSRAM, "ReadSRAM", run_ReadSRAM },
    { SYM_WriteSRAM, "WriteSRAM", run_WriteSRAM },
    { SYM_GetPlayerCount, "GetPlayerCount", run_GetPlayerCount },
    { SYM_GetGoalieCount, "GetGoalieCount", run_GetGoalieCount },
    { SYM_ReloadEnergyBarTiles, "ReloadEnergyBarTiles", run_ReloadEnergyBarTiles },
    { SYM_ReloadCrowdTiles, "ReloadCrowdTiles", run_ReloadCrowdTiles },
    { SYM_NextShooter, "NextShooter", run_NextShooter },
    { SYM_Set4WayPlayerStub, "Set4WayPlayerStub", run_Set4WayPlayerStub },
    { SYM_PrintMenuItem, "PrintMenuItem", run_PrintMenuItem },
    { SYM_UpdateMenuSelection, "UpdateMenuSelection", run_UpdateMenuSelection },
    { SYM_DrawMenuScreen, "DrawMenuScreen", run_DrawMenuScreen },
    { SYM_HandleMenuInput, "HandleMenuInput", run_HandleMenuInput },
    { SYM_PutTeamBlock, "PutTeamBlock", run_PutTeamBlock },
    { SYM_ChooseSong, "ChooseSong", run_ChooseSong },
    { SYM_CheckScoreLeader, "CheckScoreLeader", run_CheckScoreLeader },
    { SYM_LeadSong, "LeadSong", run_LeadSong },
    { SYM_ClearLeadSong, "ClearLeadSong", run_ClearLeadSong },
    { SYM_rtsChgplayer, "rtsChgplayer", run_rtsChgplayer },
    { SYM_ClearPenalties, "ClearPenalties", run_ClearPenalties },
    { SYM_SetSflags8Bit1, "SetSflags8Bit1", run_SetSflags8Bit1 },
    { SYM_box, "box", run_box },
    { SYM_ShowGoalBox, "ShowGoalBox", run_ShowGoalBox },
    { SYM_PenaltyShotBox, "PenaltyShotBox", run_PenaltyShotBox },
    { SYM_ShowThreeStars, "ShowThreeStars", run_ShowThreeStars },
    { SYM_ShowInjuryBox, "ShowInjuryBox", run_ShowInjuryBox },
    { SYM_CountGoalies, "CountGoalies", run_CountGoalies },
    { SYM_ReloadRinkGraphics, "ReloadRinkGraphics", run_ReloadRinkGraphics },
    { SYM_PeriodOver, "PeriodOver", run_PeriodOver },
    { SYM_resetplstuff, "resetplstuff", run_resetplstuff },
    { SYM_SprSort, "SprSort", run_SprSort },
    { SYM_setupEASNmap, "setupEASNmap", run_setupEASNmap },
    { SYM_setupIceRinkPal, "setupIceRinkPal", run_setupIceRinkPal },
    { SYM_setplayercolors, "setplayercolors", run_setplayercolors },
    { SYM_ShootoutModeBox, "ShootoutModeBox", run_ShootoutModeBox },
    { SYM_CountShootoutGoals, "CountShootoutGoals", run_CountShootoutGoals },
    { SYM_ShootoutWonBy, "ShootoutWonBy", run_ShootoutWonBy },
    { SYM_SetMenuPrintX, "SetMenuPrintX", run_SetMenuPrintX },
    { SYM_vcountwait, "vcountwait", run_vcountwait },
    { SYM_play_sfx_or_music_track, "play_sfx_or_music_track", run_play_sfx_or_music_track },
    { SYM_ReadPad4Way1, "ReadPad4Way1", run_ReadPad4Way1 },
    { SYM_ReadPad4Way2, "ReadPad4Way2", run_ReadPad4Way2 },
    { SYM_ReadPad4Way3, "ReadPad4Way3", run_ReadPad4Way3 },
    { SYM_ReadPad4Way4, "ReadPad4Way4", run_ReadPad4Way4 },
    { SYM_ReadPad4WayPort1, "ReadPad4WayPort1", run_ReadPad4WayPort1 },
    { SYM_ReadPad1, "ReadPad1", run_ReadPad1 },
    { SYM_ReadPad2, "ReadPad2", run_ReadPad2 },
    { SYM_ReadPadA0, "ReadPadA0", run_ReadPadA0 },
    { SYM_ReadPad3Button, "ReadPad3Button", run_ReadPad3Button },
    { SYM_ProcessOneMusicTrack, "ProcessOneMusicTrack", run_ProcessOneMusicTrack },
    { SYM_handle_command_00, "handle_command_00", run_handle_command_00 },
    { SYM_handle_command_10, "handle_command_10", run_handle_command_10 },
    { SYM_handle_command_30, "handle_command_30", run_handle_command_30 },
    { SYM_handle_command_40, "handle_command_40", run_handle_command_40 },
    { SYM_handle_command_60, "handle_command_60", run_handle_command_60 },
    { SYM_handle_command_skip, "handle_command_skip", run_handle_command_skip },
    { SYM_ReleaseChannelAndNote, "ReleaseChannelAndNote", run_ReleaseChannelAndNote },
    { SYM_UpdateChannelFrequencyAndVolume, "UpdateChannelFrequencyAndVolume", run_UpdateChannelFrequencyAndVolume },
    { SYM_SetChannelVolume, "SetChannelVolume", run_SetChannelVolume },
    { SYM_p_turnoff, "p_turnoff", run_p_turnoff },
    { SYM_rtsfx, "rtsfx", run_rtsfx },
    { SYM_rtscmd10, "rtscmd10", run_rtscmd10 },
    { SYM_stop_current_song, "stop_current_song", run_stop_current_song },
    { SYM_ReadJoyData, "ReadJoyData", run_ReadJoyData },
    { SYM_ResetZ80Bus, "ResetZ80Bus", run_ResetZ80Bus },
    { SYM_p_music_vblank, "p_music_vblank", run_p_music_vblank },
    { SYM_z80_bus_release_delay, "z80_bus_release_delay", run_z80_bus_release_delay },
    { SYM_UploadCommandBufferToZ80, "UploadCommandBufferToZ80", run_UploadCommandBufferToZ80 },
    { SYM_StopPCMChannel, "StopPCMChannel", run_StopPCMChannel },
    { SYM_p_initialZ80, "p_initialZ80", run_p_initialZ80 },
    { SYM_ClearAllTrackAndSFXSlots, "ClearAllTrackAndSFXSlots", run_ClearAllTrackAndSFXSlots },
    { SYM_defaultsprites, "defaultsprites", run_defaultsprites },
    { SYM_defaultsprites2, "defaultsprites2", run_defaultsprites2 },
    { SYM_setupTeamBlocksMap, "setupTeamBlocksMap", run_setupTeamBlocksMap },
    { SYM_CopyTeamBlockMapData, "CopyTeamBlockMapData", run_CopyTeamBlockMapData },
    { SYM_ReloadRefHorTiles, "ReloadRefHorTiles", run_ReloadRefHorTiles },
    { SYM_InitMenuState, "InitMenuState", run_InitMenuState },
    { SYM_DoGameFrame, "DoGameFrame", run_DoGameFrame },
    { SYM_Pausemode, "Pausemode", run_Pausemode },
    { SYM_setupice, "setupice", run_setupice },
    { SYM_setSlotBit, "setSlotBit", run_setSlotBit },
    { SYM_Gameloop, "Gameloop", run_never },
    { SYM_StartPer, "StartPer", run_never },
    { SYM_IntermissionStart, "IntermissionStart", run_never },
    { SYM_StartHL, "StartHL", run_StartHL },
    { SYM_StartHL2, "StartHL2", run_StartHL2 },
    { SYM_InitTeamStructure, "InitTeamStructure", run_InitTeamStructure },
    { SYM_ReloadRefTiles, "ReloadRefTiles", run_ReloadRefTiles },
    { SYM_ReloadFaceOffTiles, "ReloadFaceOffTiles", run_ReloadFaceOffTiles },
    { SYM_ReloadFaceOffMap, "ReloadFaceOffMap", run_ReloadFaceOffMap },
    { SYM_SetupScreen, "SetupScreen", run_SetupScreen },
    { SYM_WaitVSyncAndReadInput, "WaitVSyncAndReadInput", run_WaitVSyncAndReadInput },
    { SYM_PlayerRecordIO, "PlayerRecordIO", run_PlayerRecordIO },
    { SYM_ReadPlayerRecord, "ReadPlayerRecord", run_ReadPlayerRecord },
    { SYM_WritePlayerRecord, "WritePlayerRecord", run_WritePlayerRecord },
    { SYM_TeamRecordIO, "TeamRecordIO", run_TeamRecordIO },
    { SYM_ReadTeamRecord, "ReadTeamRecord", run_ReadTeamRecord },
    { SYM_WriteTeamRecord, "WriteTeamRecord", run_WriteTeamRecord },
    { SYM_WriteCrowdRecord, "WriteCrowdRecord", run_WriteCrowdRecord },
    { SYM_UpdatePlayerRecords, "UpdatePlayerRecords", run_UpdatePlayerRecords },
    { SYM_UpdateTeamRecord, "UpdateTeamRecord", run_UpdateTeamRecord },
    { SYM_UpdateCrowdRecord, "UpdateCrowdRecord", run_UpdateCrowdRecord },
    { SYM_Intermission, "Intermission", run_Intermission },
    { SYM_DoHiLights, "DoHiLights", run_DoHiLights },
    { SYM_setupice_highlight, "setupice_highlight", run_setupice_highlight },
    { SYM_clearTeamStats, "clearTeamStats", run_clearTeamStats },
    { SYM_setteams, "setteams", run_setteams },
    { SYM_ReplayMode, "ReplayMode", run_ReplayMode },
    { SYM_PeriodStatsScreen, "PeriodStatsScreen", run_PeriodStatsScreen },
    { SYM_GameStatisticsScreen, "GameStatisticsScreen", run_GameStatisticsScreen },
    { SYM_rtsStatTables, "rtsStatTables", run_rtsStatTables },
    { SYM_ExitStatsScreen, "ExitStatsScreen", run_ExitStatsScreen },
    { SYM_AddPlayoffTeamStats, "AddPlayoffTeamStats", run_AddPlayoffTeamStats },
    { SYM_ReadTeamStats, "ReadTeamStats", run_ReadTeamStats },
    { SYM_CountPlayers, "CountPlayers", run_CountPlayers },
    { SYM_UpdateRecords, "UpdateRecords", run_UpdateRecords },
    { SYM_MakeSRAMChecksum, "MakeSRAMChecksum", run_MakeSRAMChecksum },
    { SYM_SegaInit, "SegaInit", run_SegaInit },
    { SYM_ValidationRoutine, "ValidationRoutine", run_ValidationRoutine },
    { SYM_EASportsScreen, "EASportsScreen", run_EASportsScreen },
    { SYM_Detect4WayPlay, "Detect4WayPlay", run_Detect4WayPlay },
    { SYM_DefaultMenus, "DefaultMenus", run_DefaultMenus },
    { SYM_InitSaveRAM, "InitSaveRAM", run_InitSaveRAM },
    { SYM_SRAMWaitVblank, "SRAMWaitVblank", run_SRAMWaitVblank },
    { SYM_VBcount, "VBcount", run_VBcount },
    { SYM_ValidateSRAM, "ValidateSRAM", run_ValidateSRAM },
    { SYM_ClearSRAM, "ClearSRAM", run_ClearSRAM },
    { SYM_ReadLineData, "ReadLineData", run_ReadLineData },
    { SYM_HiScoreScreen, "HiScoreScreen", run_HiScoreScreen },
    { SYM_newTitleScreen, "newTitleScreen", run_newTitleScreen },
    { SYM_CreditsPrintRow, "CreditsPrintRow", run_CreditsPrintRow },
    { SYM_CreditsWait, "CreditsWait", run_CreditsWait },
    { SYM_VBlank_TitleScreen, "VBlank_TitleScreen", run_VBlank_TitleScreen },
    { SYM_CreditsLineScroll, "CreditsLineScroll", run_CreditsLineScroll },
    { SYM_CreditsScrollStep, "CreditsScrollStep", run_CreditsScrollStep },
    { SYM_SuperDiv, "SuperDiv", run_SuperDiv },
    { SYM_ReadPassBits, "ReadPassBits", run_ReadPassBits },
    { SYM_GetRandomUnusedTeam, "GetRandomUnusedTeam", run_GetRandomUnusedTeam },
    { SYM_InitializeGameStructures, "InitializeGameStructures", run_InitializeGameStructures },
    { SYM_FigureJoy, "FigureJoy", run_FigureJoy },
    { SYM_maketree, "maketree", run_maketree },
    { SYM_NewPO, "NewPO", run_NewPO },
    { SYM_ContinuePlayoffs, "ContinuePlayoffs", run_ContinuePlayoffs },
    { SYM_InitShooters, "InitShooters", run_InitShooters },
    { SYM_ClearShootout, "ClearShootout", run_ClearShootout },
    { SYM_UpdatePlayoffLevel, "UpdatePlayoffLevel", run_UpdatePlayoffLevel },
    { SYM_DrawMatchupBitmaps, "DrawMatchupBitmaps", run_DrawMatchupBitmaps },
    { SYM_DrawTeamBitmap, "DrawTeamBitmap", run_DrawTeamBitmap },
    { SYM_VBlank_SetOptions, "VBlank_SetOptions", run_VBlank_SetOptions },
    { SYM_UnpackPicture, "UnpackPicture", run_UnpackPicture },
    { SYM_ResetCardTimer, "ResetCardTimer", run_ResetCardTimer },
    { SYM_PlayerCardTimer, "PlayerCardTimer", run_PlayerCardTimer },
    { SYM_PlayerCardScreen, "PlayerCardScreen", run_PlayerCardScreen },
    { SYM_PrintRecordValue, "PrintRecordValue", run_PrintRecordValue },
    { SYM_CopyString, "CopyString", run_CopyString },
    { SYM_AppendNumber, "AppendNumber", run_AppendNumber },
    { SYM_GetRecordValue, "GetRecordValue", run_GetRecordValue },
    { SYM_PrintRecordHolder, "PrintRecordHolder", run_PrintRecordHolder },
    { SYM_PrintRecordVs, "PrintRecordVs", run_PrintRecordVs },
    { SYM_AppendRecordHolder, "AppendRecordHolder", run_AppendRecordHolder },
    { SYM_AppendRecordVs, "AppendRecordVs", run_AppendRecordVs },
    { SYM_ReadNameLog, "ReadNameLog", run_ReadNameLog },
    { SYM_NameLogIO, "NameLogIO", run_NameLogIO },
    { SYM_AppendUserName, "AppendUserName", run_AppendUserName },
    { SYM_AppendTeamName, "AppendTeamName", run_AppendTeamName },
    { SYM_TrimSpaces, "TrimSpaces", run_TrimSpaces },
    { SYM_GameSetUp, "GameSetUp", run_GameSetUp },
    { SYM_GoNewPlayoffs, "GoNewPlayoffs", run_GoNewPlayoffs },
    { SYM_GoContinuePlayoffs, "GoContinuePlayoffs", run_GoContinuePlayoffs },
    { SYM_MoveMenuFrame, "MoveMenuFrame", run_MoveMenuFrame },
    { SYM_SetMenuArea, "SetMenuArea", run_SetMenuArea },
    { SYM_SetFrameRect, "SetFrameRect", run_SetFrameRect },
    { SYM_rtsSetup, "rtsSetup", run_rtsSetup },
    { SYM_FixModeOptions, "FixModeOptions", run_FixModeOptions },
    { SYM_ChangeOption, "ChangeOption", run_ChangeOption },
    { SYM_WrapOption, "WrapOption", run_WrapOption },
    { SYM_PrintOptions, "PrintOptions", run_PrintOptions },
    { SYM_PrintOptionValue, "PrintOptionValue", run_PrintOptionValue },
    { SYM_PrintScrollMarks, "PrintScrollMarks", run_PrintScrollMarks },
    { SYM_PrintOptTeamName, "PrintOptTeamName", run_PrintOptTeamName },
    { SYM_PrintOptionNames, "PrintOptionNames", run_PrintOptionNames },
    { SYM_setoptions, "setoptions", run_setoptions },
    { SYM_ReadSetupPads, "ReadSetupPads", run_ReadSetupPads },
    { SYM_SetupDemo, "SetupDemo", run_SetupDemo },
    { SYM_SetupStart, "SetupStart", run_SetupStart },
    { SYM_LoadSetupTiles, "LoadSetupTiles", run_LoadSetupTiles },
    { SYM_UpdateRosterPlayers, "UpdateRosterPlayers", run_UpdateRosterPlayers },
    { SYM_EraseCard, "EraseCard", run_EraseCard },
    { SYM_DrawHomeBlock, "DrawHomeBlock", run_DrawHomeBlock },
    { SYM_DrawVisBlock, "DrawVisBlock", run_DrawVisBlock },
    { SYM_DrawTeamLogo, "DrawTeamLogo", run_DrawTeamLogo },
    { SYM_LogoBoxRight, "LogoBoxRight", run_LogoBoxRight },
    { SYM_LogoBoxHome, "LogoBoxHome", run_LogoBoxHome },
    { SYM_LogoBoxLeft, "LogoBoxLeft", run_LogoBoxLeft },
    { SYM_LogoBoxVis, "LogoBoxVis", run_LogoBoxVis },
    { SYM_DrawLogoBox, "DrawLogoBox", run_DrawLogoBox },
    { SYM_UserNameEntry, "UserNameEntry", run_UserNameEntry },
    { SYM_NameEntryScreen, "NameEntryScreen", run_NameEntryScreen },
    { SYM_NameLetterGrid, "NameLetterGrid", run_NameLetterGrid },
    { SYM_NameEntryHelp, "NameEntryHelp", run_NameEntryHelp },
    { SYM_PrintNamePrompt, "PrintNamePrompt", run_PrintNamePrompt },
    { SYM_SyncLetterCursor, "SyncLetterCursor", run_SyncLetterCursor },
    { SYM_FindLetter, "FindLetter", run_FindLetter },
    { SYM_PrintNameCursor, "PrintNameCursor", run_PrintNameCursor },
    { SYM_rtsNameCursor, "rtsNameCursor", run_rtsNameCursor },
    { SYM_PrintCurLetter, "PrintCurLetter", run_PrintCurLetter },
    { SYM_LetterGridPos, "LetterGridPos", run_LetterGridPos },
    { SYM_PrintNameLog, "PrintNameLog", run_PrintNameLog },
    { SYM_MoveLogArrows, "MoveLogArrows", run_MoveLogArrows },
    { SYM_PrintLogArrows, "PrintLogArrows", run_PrintLogArrows },
    { SYM_GetNameLength, "GetNameLength", run_GetNameLength },
    { SYM_PrintNameField, "PrintNameField", run_PrintNameField },
    { SYM_NameEntryFramer, "NameEntryFramer", run_NameEntryFramer },
    { SYM_GetLogName, "GetLogName", run_GetLogName },
    { SYM_StoreUserName, "StoreUserName", run_StoreUserName },
    { SYM_WriteNameRecord, "WriteNameRecord", run_WriteNameRecord },
    { SYM_SkipOtherUserName, "SkipOtherUserName", run_SkipOtherUserName },
    { SYM_WriteNameLog, "WriteNameLog", run_WriteNameLog },
    { SYM_PlayoffScreen, "PlayoffScreen", run_PlayoffScreen },
    { SYM_HandlePlayoffInput, "HandlePlayoffInput", run_HandlePlayoffInput },
    { SYM_UpdatePlayoffScroll, "UpdatePlayoffScroll", run_UpdatePlayoffScroll },
    { SYM_PlayoffScreen_waitvsync, "PlayoffScreen_waitvsync", run_PlayoffScreen_waitvsync },
    { SYM_PrintSeriesScore, "PrintSeriesScore", run_PrintSeriesScore },
    { SYM_DrawTreeArrow, "DrawTreeArrow", run_DrawTreeArrow },
    { SYM_DrawTeamBlocks, "DrawTeamBlocks", run_DrawTeamBlocks },
    { SYM_VBlank_Playoff, "VBlank_Playoff", run_VBlank_Playoff },
    { SYM_DrawPlayoffSprite, "DrawPlayoffSprite", run_DrawPlayoffSprite },
    { SYM_PlayoffScreenExit, "PlayoffScreenExit", run_PlayoffScreenExit },
    { SYM_ScoutingReport, "ScoutingReport", run_ScoutingReport },
    { SYM_ScoutCrowdRecord, "ScoutCrowdRecord", run_ScoutCrowdRecord },
    { SYM_PageMatchup, "PageMatchup", run_PageMatchup },
    { SYM_PrintMatchupRating, "PrintMatchupRating", run_PrintMatchupRating },
    { SYM_PrintTwoSpaces, "PrintTwoSpaces", run_PrintTwoSpaces },
    { SYM_DrawMatchupPictures, "DrawMatchupPictures", run_DrawMatchupPictures },
    { SYM_DrawMatchupPicture, "DrawMatchupPicture", run_DrawMatchupPicture },
    { SYM_DrawMatchupLogo, "DrawMatchupLogo", run_DrawMatchupLogo },
    { SYM_GetTeamLogo, "GetTeamLogo", run_GetTeamLogo },
    { SYM_PrintMatchupRatings, "PrintMatchupRatings", run_PrintMatchupRatings },
    { SYM_GetMatchupPlayers, "GetMatchupPlayers", run_GetMatchupPlayers },
    { SYM_GetMatchupPlayer, "GetMatchupPlayer", run_GetMatchupPlayer },
    { SYM_RestartAdvantageMarks, "RestartAdvantageMarks", run_RestartAdvantageMarks },
    { SYM_PrintAdvantageMarks, "PrintAdvantageMarks", run_PrintAdvantageMarks },
    { SYM_GetTeamNickname, "GetTeamNickname", run_GetTeamNickname },
    { SYM_GetTeamArena, "GetTeamArena", run_GetTeamArena },
    { SYM_InitScoutingDisplay, "InitScoutingDisplay", run_InitScoutingDisplay },
    { SYM_UpdateScoutingDisplay, "UpdateScoutingDisplay", run_UpdateScoutingDisplay },
    { SYM_ScrollDisplayUp, "ScrollDisplayUp", run_ScrollDisplayUp },
    { SYM_PlayedByHome, "PlayedByHome", run_PlayedByHome },
    { SYM_PlayedByAway, "PlayedByAway", run_PlayedByAway },
    { SYM_AttribAdjust, "AttribAdjust", run_AttribAdjust },
    { SYM_DrawPlayerPicture, "DrawPlayerPicture", run_DrawPlayerPicture },
    { SYM_CalcAttrib, "CalcAttrib", run_CalcAttrib },
    { SYM_DrawMatchupBitmap, "DrawMatchupBitmap", run_DrawMatchupBitmap },
    { SYM_PlayedByText, "PlayedByText", run_PlayedByText },
    { SYM_PlayedByNone, "PlayedByNone", run_PlayedByNone },
    { SYM_PlayedByExit, "PlayedByExit", run_PlayedByExit },
    { SYM_GameOver, "GameOver", run_GameOver },
    { SYM_ClrPassBits, "ClrPassBits", run_ClrPassBits },
    { SYM_SuperAdd, "SuperAdd", run_SuperAdd },
    { SYM_SuperMult, "SuperMult", run_SuperMult },
    { SYM_PushBits, "PushBits", run_PushBits },
    { SYM_WritePassBits, "WritePassBits", run_WritePassBits },
    { SYM_ResolveGames, "ResolveGames", run_ResolveGames },
    { SYM_EncodePW, "EncodePW", run_EncodePW },
    { SYM_WriteLineData, "WriteLineData", run_WriteLineData },
    { SYM_PlayoffStatsScreen, "PlayoffStatsScreen", run_PlayoffStatsScreen },
    { SYM_PlayerStatsScreen, "PlayerStatsScreen", run_PlayerStatsScreen },
    { SYM_PlayerStatsList, "PlayerStatsList", run_PlayerStatsList },
    { SYM_UpdateStatsListScroll, "UpdateStatsListScroll", run_UpdateStatsListScroll },
    { SYM_SetStatsListScrollReg, "SetStatsListScrollReg", run_SetStatsListScrollReg },
    { SYM_DrawStatsRowUp, "DrawStatsRowUp", run_DrawStatsRowUp },
    { SYM_DrawStatsRowDown, "DrawStatsRowDown", run_DrawStatsRowDown },
    { SYM_DrawStatsList, "DrawStatsList", run_DrawStatsList },
    { SYM_DrawStatsRow, "DrawStatsRow", run_DrawStatsRow },
    { SYM_GetStatColumn, "GetStatColumn", run_GetStatColumn },
    { SYM_ReadStatColumn, "ReadStatColumn", run_ReadStatColumn },
    { SYM_GetStatsValue, "GetStatsValue", run_GetStatsValue },
    { SYM_DrawStatsListArrows, "DrawStatsListArrows", run_DrawStatsListArrows },
    { SYM_ShowScores, "ShowScores", run_ShowScores },
    { SYM_UpdateScoresScroll, "UpdateScoresScroll", run_UpdateScoresScroll },
    { SYM_UpdateVertScrollReg, "UpdateVertScrollReg", run_UpdateVertScrollReg },
    { SYM_DisplayGameInfo, "DisplayGameInfo", run_DisplayGameInfo },
    { SYM_PrintGameInfo, "PrintGameInfo", run_PrintGameInfo },
    { SYM_DrawScrollArrows, "DrawScrollArrows", run_DrawScrollArrows },
    { SYM_LineEditor, "LineEditor", run_LineEditor },
    { SYM_DrawLineIcon, "DrawLineIcon", run_DrawLineIcon },
    { SYM_ClearLineArea, "ClearLineArea", run_ClearLineArea },
    { SYM_DrawTeamScreen, "DrawTeamScreen", run_DrawTeamScreen },
    { SYM_ClearLineEditorTop, "ClearLineEditorTop", run_ClearLineEditorTop },
    { SYM_ClearMenuFlags, "ClearMenuFlags", run_ClearMenuFlags },
    { SYM_SetLineSlot, "SetLineSlot", run_SetLineSlot },
    { SYM_DecodeTeamLines, "DecodeTeamLines", run_DecodeTeamLines },
    { SYM_EncodeTeamLines, "EncodeTeamLines", run_EncodeTeamLines },
    { SYM_TeamRosterScreen, "TeamRosterScreen", run_TeamRosterScreen },
    { SYM_CheckPlayerListScroll, "CheckPlayerListScroll", run_CheckPlayerListScroll },
    { SYM_UpdatePlayerListScroll, "UpdatePlayerListScroll", run_UpdatePlayerListScroll },
    { SYM_StopPlayerListScroll, "StopPlayerListScroll", run_StopPlayerListScroll },
    { SYM_DisplayPlayerListUp, "DisplayPlayerListUp", run_DisplayPlayerListUp },
    { SYM_DisplayPlayerListDown, "DisplayPlayerListDown", run_DisplayPlayerListDown },
    { SYM_DisplayPlayerList, "DisplayPlayerList", run_DisplayPlayerList },
    { SYM_GetNameandAttrib, "GetNameandAttrib", run_GetNameandAttrib },
    { SYM_AttribStatus, "AttribStatus", run_AttribStatus },
    { SYM_AttribEnergy, "AttribEnergy", run_AttribEnergy },
    { SYM_AttribFighting, "AttribFighting", run_AttribFighting },
    { SYM_AttribRating, "AttribRating", run_AttribRating },
    { SYM_AttribPrintPct, "AttribPrintPct", run_AttribPrintPct },
    { SYM_AttribHanded, "AttribHanded", run_AttribHanded },
    { SYM_AttribWeight, "AttribWeight", run_AttribWeight },
    { SYM_ScoringSummaryScreen, "ScoringSummaryScreen", run_ScoringSummaryScreen },
    { SYM_CheckScoringScroll, "CheckScoringScroll", run_CheckScoringScroll },
    { SYM_UpdateScoringScroll, "UpdateScoringScroll", run_UpdateScoringScroll },
    { SYM_DrawScoringRowUp, "DrawScoringRowUp", run_DrawScoringRowUp },
    { SYM_DrawScoringRowDown, "DrawScoringRowDown", run_DrawScoringRowDown },
    { SYM_DrawScoringRow, "DrawScoringRow", run_DrawScoringRow },
    { SYM_PrintScorerName, "PrintScorerName", run_PrintScorerName },
    { SYM_PenaltySummaryScreen, "PenaltySummaryScreen", run_PenaltySummaryScreen },
    { SYM_CheckPenaltyScroll, "CheckPenaltyScroll", run_CheckPenaltyScroll },
    { SYM_UpdatePenaltyScroll, "UpdatePenaltyScroll", run_UpdatePenaltyScroll },
    { SYM_DisplayPenaltyLineUp, "DisplayPenaltyLineUp", run_DisplayPenaltyLineUp },
    { SYM_DisplayPenaltyLineDown, "DisplayPenaltyLineDown", run_DisplayPenaltyLineDown },
    { SYM_DisplayPenaltyEntry, "DisplayPenaltyEntry", run_DisplayPenaltyEntry },
    { SYM_DrawScrollArrowsPenalty, "DrawScrollArrowsPenalty", run_DrawScrollArrowsPenalty },
    { SYM_CrowdMeterScreen, "CrowdMeterScreen", run_CrowdMeterScreen },
    { SYM_PrintCrowdMeter, "PrintCrowdMeter", run_PrintCrowdMeter },
    { SYM_PrintCrowdDB, "PrintCrowdDB", run_PrintCrowdDB },
    { SYM_TimeoutMenu, "TimeoutMenu", run_TimeoutMenu },
    { SYM_SelectGoalieMenu, "SelectGoalieMenu", run_SelectGoalieMenu },
    { SYM_DrawGoalieSelectList, "DrawGoalieSelectList", run_DrawGoalieSelectList },
    { SYM_GetDefenseStart, "GetDefenseStart", run_GetDefenseStart },
    { SYM_GetForwardCount, "GetForwardCount", run_GetForwardCount },
    { SYM_PlayerCards, "PlayerCards", run_PlayerCards },
    { SYM_WrapCardIndex, "WrapCardIndex", run_WrapCardIndex },
    { SYM_DrawPlayerCard, "DrawPlayerCard", run_DrawPlayerCard },
    { SYM_PrintCardRecords, "PrintCardRecords", run_PrintCardRecords },
    { SYM_ClearCardText, "ClearCardText", run_ClearCardText },
    { SYM_PrintOverallRating, "PrintOverallRating", run_PrintOverallRating },
    { SYM_PrintStartingLine, "PrintStartingLine", run_PrintStartingLine },
    { SYM_PrintCardPosition, "PrintCardPosition", run_PrintCardPosition },
    { SYM_PrintTeamRating, "PrintTeamRating", run_PrintTeamRating },
    { SYM_PrintCardTeam, "PrintCardTeam", run_PrintCardTeam },
    { SYM_PrintCentered, "PrintCentered", run_PrintCentered },
    { SYM_GetJerseyString, "GetJerseyString", run_GetJerseyString },
    { SYM_PrintGoalsAssists, "PrintGoalsAssists", run_PrintGoalsAssists },
    { SYM_PrintSaves, "PrintSaves", run_PrintSaves },
    { SYM_DrawPictureBox, "DrawPictureBox", run_DrawPictureBox },
    { SYM_GetCardTeam, "GetCardTeam", run_GetCardTeam },
    { SYM_GetCardPlayer, "GetCardPlayer", run_GetCardPlayer },
    { SYM_BuildCardPlayerLists, "BuildCardPlayerLists", run_BuildCardPlayerLists },
    { SYM_BuildCardPlayerList, "BuildCardPlayerList", run_BuildCardPlayerList },
    { SYM_RecordHoldersScreen, "RecordHoldersScreen", run_RecordHoldersScreen },
    { SYM_PrintRecordPage, "PrintRecordPage", run_PrintRecordPage },
    { SYM_PrintRecordTitles, "PrintRecordTitles", run_PrintRecordTitles },
    { SYM_PrintWinRecords, "PrintWinRecords", run_PrintWinRecords },
    { SYM_ClearRecordArea, "ClearRecordArea", run_ClearRecordArea },
    { SYM_PrintPlayerRecords, "PrintPlayerRecords", run_PrintPlayerRecords },
    { SYM_PrintRecordName, "PrintRecordName", run_PrintRecordName },
    { SYM_CalcWinPercents, "CalcWinPercents", run_CalcWinPercents },
    { SYM_ReadTeamRecords, "ReadTeamRecords", run_ReadTeamRecords },
    { SYM_ShootoutShooters, "ShootoutShooters", run_ShootoutShooters },
    { SYM_PrintShooterList, "PrintShooterList", run_PrintShooterList },
    { SYM_ShootersBackground, "ShootersBackground", run_ShootersBackground },
    { SYM_ClearShooterScreen, "ClearShooterScreen", run_ClearShooterScreen },
    { SYM_ResetShooterScroll, "ResetShooterScroll", run_ResetShooterScroll },
    { SYM_PrintShooterSlots, "PrintShooterSlots", run_PrintShooterSlots },
    { SYM_PrintShooterBox, "PrintShooterBox", run_PrintShooterBox },
    { SYM_PrintShooterNames, "PrintShooterNames", run_PrintShooterNames },
    { SYM_FormatPlayerNameShort, "FormatPlayerNameShort", run_FormatPlayerNameShort },
    { SYM_FormatPlayerNameLast, "FormatPlayerNameLast", run_FormatPlayerNameLast },
    { SYM_GetLeagueCrowdRecord, "GetLeagueCrowdRecord", run_GetLeagueCrowdRecord },
    { SYM_ClearWinRecords, "ClearWinRecords", run_ClearWinRecords },
    { SYM_GetTeamStruct, "GetTeamStruct", run_GetTeamStruct },
    { SYM_PrintCardRecordLine, "PrintCardRecordLine", run_PrintCardRecordLine },
    { SYM_PrintCrowdRecordLine, "PrintCrowdRecordLine", run_PrintCrowdRecordLine },
    { SYM_PrintSaveRecordLine, "PrintSaveRecordLine", run_PrintSaveRecordLine },
    { SYM_PrintGoalRecordLine, "PrintGoalRecordLine", run_PrintGoalRecordLine },
    { 0, NULL, NULL }
};

const uint32_t inner_labels[] = {
    SYM_ValidationLoop, SYM_SkipIncrement, SYM_ContinueValidation, SYM_VDPErrorSetup, SYM_VRAMWriteLoop, SYM_Halt,   /* ValidationRoutine */
    SYM_FormatLastName,                                   /* FormatPlayerNameShort / FormatPlayerNameLast branch into it */
    0
};
