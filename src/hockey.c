/* hockey94.asm: main game code. Starts the game, runs the game loop from the opening faceoff
 * to the end of each period, handles the events that happen every so often and keeps the
 * clock. It also runs the pause mode and its screen. The frame drivers (StartPer, Gameloop,
 * DoGameFrame, Pausemode, Begin, StartGame) come with the standalone loop. */
#include "game.h"
#include "hal.h"
#include "rom.h"
#include "symbols_gen.h"
#include "hockey_tables_gen.h"
#include "hockey_strings_gen.h"

#define PFLAG(o, b) ((o)->pflags & (1 << (b)))

/* hockey94.asm:ClearShotData. Clear $E4 words at outputbuffer. */
void ClearShotData(void)
{
    for (int i = 0; i < 0xE4; i++) wr16(g.outputbuffer + i * 2, 0);
}

/* hockey94.asm:restoreteams. Put both teams' rosters on the bench. */
void restoreteams(void)
{
    for (int t = 0; t < 2; t++) {
        uint8_t *tm = team_struct(t);
        wr16(tm + tmap, 6);
        for (int16_t d0 = 0x32; d0 >= 0; d0 -= 2) wr16(tm + tmpdst + d0, 0xFFFE);
    }
}

/* hockey94.asm:ResetClock. Set the period length and stop the clock; the late game song
 * trigger time is a random moment in the second half. */
void ResetClock(void)
{
    uint16_t d0 = GetPeriodTime();
    if ((int16_t)g.gsp >= 3 && g.OptPlayMode == 0) d0 = 0x258;   /* overtime is always 10:00 */
    g.gameclock = d0;
    g.PerTimeTotal = d0;
    g.periodendtime = d0;
    d0 = (uint16_t)((int16_t)d0 >> 1);
    d0 = randomd0(d0);
    g.periodendtime -= d0;
    g.gmode |= 1 << gmclock;
}

/* hockey94.asm:GetPeriodTime. The period length in seconds for the option. */
uint16_t GetPeriodTime(void)
{
    return periodtimes[g.OptPerlen];
}

/* hockey94.asm:periodicevents. Called every game loop pass: penalties, the crowd, the arena
 * animation, the sound and the clock, then once a second the goalies, the excitement, the
 * late game song, the bench energy, injuries and the power play. */
void periodicevents(void)
{
    PenaltyManager();
    updatecrowdf();
    RunArenaAnim();
    updatesound();
    clockcont();
    if (g.sflags & HI(sfhor)) return;
    g.lldisp = (uint16_t)(g.lldisp - elapsed);
    if ((int16_t)g.lldisp >= 0) return;
    g.lldisp += 0x18;
    ChkGoalies();
    UpdateCwdExcite();
    CheckLateGameSong();
    RestoreBenchEnergy();
    CheckInjury();
    updatepwrplay();
}

/* hockey94.asm:CheckInjury. Once a second: count InjCntDown down and show the injury box at
 * zero. */
void CheckInjury(void)
{
    g.InjCntDown--;
    if (g.InjCntDown != 0) return;
    ShowInjuryBox();
}

/* hockey94.asm:RestoreBenchEnergy. Once a second: restore energy for the players on the
 * bench (line changes on). */
void RestoreBenchEnergy(void)
{
    if (g.OptLine != 0) return;
    for (int t = 0; t < 2; t++) {
        uint8_t *tm = team_struct(t);
        for (int16_t d0 = 0x32; d0 >= 0; d0 -= 2) {
            if (rd16(tm + tmpdst + d0) != 0xFFFE) continue;
            uint16_t e = (uint16_t)(rd16(tm + tmpde + d0) + 9);
            if ((int16_t)e >= 0x1000) e = 0x1000;
            wr16(tm + tmpde + d0, e);
        }
    }
}

/* hockey94.asm:CheckLateGameSong. Third period: choose and play a song once at the random
 * trigger time ResetClock set. */
void CheckLateGameSong(void)
{
    if (g.gsp != 2) return;
    if (g.gmode & (1 << gmhl)) return;
    if ((int16_t)g.gameclock > (int16_t)g.periodendtime) return;
    ST_W(g.periodendtime);                                /* fires once */
    g.HmTeam = g.HomeTeam;
    g.SongIndex = 5;
    ChooseSong();
    song((int16_t)g.SongNum);
}

/* hockey94.asm:UpdateCwdExcite. Once a second: track the peak and running total of the crowd
 * excitement, then decay the level by 1. */
void UpdateCwdExcite(void)
{
    uint16_t d0 = g.CwdExciteLvl;
    if (d0 > g.MaxCwdExciteLvl) g.MaxCwdExciteLvl = d0;
    g.SumCwdExciteLvl += (uint32_t)(int32_t)(int16_t)d0;
    g.NumCwdExciteLvl++;
    g.CwdExciteLvl--;
    if ((int16_t)g.CwdExciteLvl < 0) g.CwdExciteLvl = 0;
}

/* hockey94.asm:updatecrowdf. Every game loop pass: the crowd level calms down and the crowd
 * animation frame is picked from it (cd0: frame / time pairs by level and step). */
void updatecrowdf(void)
{
    if ((int16_t)g.crowdlevel >= 0x15E) g.crowdlevel -= 3;   /* a loud crowd calms down faster */
    g.crowdlevel = (uint16_t)(g.crowdlevel - elapsed);
    if ((int16_t)g.crowdlevel < 0) g.crowdlevel = 0;
    g.crowdcnt = (uint16_t)(g.crowdcnt - elapsed);
    if ((int16_t)g.crowdcnt < 0) {
        uint16_t d0 = g.crowdlevel >> 1;
        if (d0 > 0x7F) d0 = 0x7F;
        d0 &= 0x60;
        g.crowdstep = (uint16_t)((g.crowdstep + 2) & 0x1E);
        d0 += g.crowdstep;
        const uint8_t *tab = rom_ptr(SYM_cd0) + d0;
        g.crowdframe = (uint16_t)((g.crowdframe & 0xFF00) | tab[0]);
        uint16_t d1 = tab[1];
        uint16_t d2 = (uint16_t)(vdp_hv16() & d1);        /* random 0..time */
        g.crowdcnt = (uint16_t)(d1 + d2);
    }
    g.crowdframe &= 0xFF;
    if (g.crowdlevel <= 0x118) return;
    uint16_t d0 = (uint16_t)(vdp_hv16() & 0x7F);
    if ((int16_t)d0 < 0x13 || (int16_t)d0 > 0x19) return;
    g.crowdframe = (uint16_t)((d0 << 8) | (g.crowdframe & 0xFF));   /* a random extra frame when loud */
}

/* hockey94.asm:clockcont. Monitor the period clock: at zero stop everything and end the
 * period. */
void clockcont(void)
{
    if (g.gmode & (1 << gmclock)) return;
    if (g.gameclock != 0) return;
    g.disflags |= HI(3);
    stop_current_song();
    sfx(SYM_SFXhorn);
    freezewindow();
    clockcont_0();
}

/* hockey94.asm:clockcont_0. End of period. Periods 1-2 just end; at the end of the game the
 * winning team's skaters celebrate (the Stanley Cup after a series win) and the game over
 * penalty is added. */
void clockcont_0(void)
{
    struct SortObj *a3 = sortobj(puckscnum);
    assinsert(0x18, a3);                                  /* pucknothing */
    uint16_t pen;
    if ((int16_t)g.gsp < 2) {
        pen = 2;                                          /* PenEOP */
        AddPenalty2(pen, a3);
        return;
    }
    uint16_t d0 = 7;                                      /* score assignment */
    a3 = sortobj(0);
    int cup = 0;
    if (g.gamelevel == 3) {
        if (g.bosgames == 7) cup = 1;
        else {                                            /* the Stanley Cup after 3 series wins */
            const uint8_t *gs = ram_ptr(RAM_BASE + RAM_OFF_gsstruct) + 0x10 * g.gamenum;
            uint16_t d3 = 0;
            if (gs[0xE] & 1) d3 ^= 2;
            if ((int16_t)(g.HmGoals[0] - g.AwGoals[0]) < 0) d3 ^= 2;
            if (rd16(gs + 4 + d3) == 3) cup = 1;
        }
    }
    if (cup) {
        d0 = 8;                                           /* stanley cup assignment */
        for (int i = 0; i < 12; i++) sortobj(i)->pflags &= (uint8_t)~(1 << pfjoycon);
    }
    int16_t d1 = (int16_t)(g.HmGoals[0] - g.AwGoals[0]);
    if (d1 != 0) {
        if (d1 < 0) a3 = sortobj(6);                      /* the away team leads */
        for (int i = 0; i < 6; i++, a3++) {
            if (a3->position <= 0) continue;              /* skip the goalie */
            assinsert(d0, a3);
            d0 = 7;
        }
        goto eog;
    }
    if (g.gsp != 3 || g.OptPlayMode != 0) {
        AddPenalty2(2, a3);                               /* PenEOP */
        return;
    }
eog:
    ClearPenaltyBuffer();
    g.crowdlevel += 0x3E8;
    g.gmode |= 1 << gmclock;
    g.gmode |= 0x40;
    ClearPenalties();
    AddPenalty2(4, a3);                                   /* PenEOG */
}

/* hockey94.asm:EndDemoOnPress. Any button on the pad just read ends the demo. Returns 1 when
 * it did. */
int EndDemoOnPress(uint16_t d1)
{
    if (d1 == 0) return 0;
    ExitToOpening();
    return 1;
}

/* hockey94.asm:demoread. Monitor the pads in demo mode: start pauses, anything else ends the
 * demo. */
void demoread(void)
{
    if (g.cont1team != 0) return;
    if (g.cont2team != 0) return;
    uint16_t d0, d1, d2, d3;
    ReadJoy1(&d0, &d1, &d2, &d3);
    if (d1 & 0x80) { startpause1(); return; }
    if (EndDemoOnPress(d1)) return;
    ReadJoy2(&d0, &d1, &d2, &d3);
    if (d1 & 0x80) { startpause2(); return; }
    if (g.FourWayPlay == 0) { EndDemoOnPress(d1); return; }
    ReadJoy3(&d0, &d1, &d2, &d3);
    if (d1 & 0x80) { startpause3(); return; }
    if (EndDemoOnPress(d1)) return;
    ReadJoy4(&d0, &d1, &d2, &d3);
    if (d1 & 0x80) { startpause4(); return; }
    EndDemoOnPress(d1);
}

/* hockey94.asm:startpause / startpause1-4. Pause at the end of this frame, remembering the
 * pad that asked. */
void startpause(void) { g.sflags |= HI(sfpz); }
void startpause1(void) { g.pausepad = 0; g.sflags &= (uint16_t)~HI(sfpj); startpause(); }
void startpause2(void) { g.pausepad = 0; g.sflags |= HI(sfpj); startpause(); }
void startpause3(void) { g.pausepad = 3; g.sflags &= (uint16_t)~HI(sfpj); startpause(); }
void startpause4(void) { g.pausepad = 4; g.sflags |= HI(sfpj); startpause(); }

/* hockey94.asm:SetupPauseScreen. The draw routine of the pause menu: the window plane over
 * the horizontal rink, the crowd silenced, plane 3 erased. */
void SetupPauseScreen(void)
{
    vdp_ctrl16(SYM_VDPR_WINX);
    vdp_ctrl16(SYM_VDPR_WINY + 0x1C);
    SetHor();
    setvideo();
    KillCrowd();
    printsmallz(str_SetupPauseScreen_1, sizeof str_SetupPauseScreen_1);
    eraser(0x20, 0x1C, 0x7FF);
}

/* hockey94.asm:seta2. The team struct of the pause joystick. */
uint8_t *seta2(void)
{
    uint16_t t = (g.sflags & HI(sfpj)) ? g.cont2team : g.cont1team;
    return team_struct(t != 1);
}
