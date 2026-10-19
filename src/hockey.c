/* hockey94.asm: main game code. Starts the game, runs the game loop from the opening faceoff
 * to the end of each period, handles the events that happen every so often and keeps the
 * clock. It also runs the pause mode and its screen, and the start of the game from power on (Begin,
 * StartGame). */
#include <string.h>
#include "game.h"
#include "hal.h"
#include "rom.h"
#include "symbols_gen.h"
#include "view.h"
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
        rp tm = team_struct(t);
        w16(tm, tmap, 6);
        for (int16_t d0 = 0x32; d0 >= 0; d0 -= 2) w16(tm, tmpdst + d0, 0xFFFE);
    }
}

/* hockey94.asm:ResetClock. Set the period length and stop the clock; the late game song
 * trigger time is a random moment in the second half. */
void ResetClock(void)
{
    uint16_t d0 = GetPeriodTime();
    if ((int16_t)g.gsp >= 3 && g.OptPlayMode == 0) d0 = (uint16_t)(rules[RULE_OVERTIME] * 60);   /* overtime is always 10:00 (rules: its minutes) */
    g.gameclock = d0;
    g.PerTimeTotal = d0;
    g.periodendtime = d0;
    d0 = (uint16_t)((int16_t)d0 >> 1);
    d0 = randomd0(d0);
    g.periodendtime -= d0;
    g.gmode |= 1 << gmclock;
}

/* hockey94.asm:GetPeriodTime. The period length in seconds for the option. (Rules: more lengths,
 * in the order the setup menu steps through them from 5 minutes, the secret 30 seconds last.) */
static const uint16_t more_periodtimes[8] = { 5*60, 7*60, 10*60, 15*60, 20*60, 2*60, 3*60, 30 };
uint16_t GetPeriodTime(void)
{
    if (rules[RULE_PERIOD_LENGTHS]) return more_periodtimes[g.OptPerlen & 7];
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
        rp tm = team_struct(t);
        for (int16_t d0 = 0x32; d0 >= 0; d0 -= 2) {
            uint16_t at = r16(tm, tmpdst + d0);           /* (rules: the box rests too: penalty time, not injured) */
            int box = rules[RULE_BOX_REST] && (int16_t)at >= 0 && !(at & 0x1000);
            if (at != 0xFFFE && !box) continue;
            uint16_t e = (uint16_t)(r16(tm, tmpde + d0) + rule_scale(RULE_BENCH_REST, 9));     /* (rules: bench rest) */
            if ((int16_t)e >= 0x1000) e = 0x1000;
            w16(tm, tmpde + d0, e);
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
    g.disflags |= HI(dfclock);
    stop_current_song(0);                                /* periodicevents' d0 is not followed (a register leak) */
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
    if (view_wide()) vdp_ctrl16(SYM_VDPR_MODE4);         /* (the wide view: the window plane is 32 columns) */
    vdp_ctrl16(SYM_VDPR_WINX);
    vdp_ctrl16(SYM_VDPR_WINY + 0x1C);
    SetHor();
    setvideo();
    KillCrowd();
    printsmallz(str_SetupPauseScreen_1, sizeof str_SetupPauseScreen_1);
    eraser(0x20, 0x1C, 0x7FF);
}

/* hockey94.asm:seta2. The team struct of the pause joystick. */
rp seta2(void)
{
    uint16_t t = (g.sflags & HI(sfpj)) ? g.cont2team : g.cont1team;
    return team_struct(t != 1);
}

/* hockey94.asm:VBjsr. The vblank interrupt: jump to the handler in vbint. */
void VBjsr(void) { call68k(g.vbint); }

/* hockey94.asm:DoGameFrame. Wait for a vblank, then one frame of game logic: d7 (elapsed) =
 * the frames since the last pass. The clock and periodic events, the players, a delayed song,
 * the slot bit, the view, the replay recording, and the lists for the next vblank. */
void DoGameFrame(void)
{
    int16_t d7;
    while ((d7 = (int16_t)(g.vcount - g.oldvcount)) == 0) hal_wait_vblank();
    g.oldvcount = g.vcount;
    elapsed = d7;
    periodicevents();
    updateplayers();
    if ((int16_t)g.songdelay > 0 && --g.songdelay == 0) {
        stop_current_song(0);                             /* updateplayers' d0 is not followed (a register leak) */
        song((int16_t)g.delayedsong);
    }
    setSlotBit();
    checkwindow();
    updatereplay();
    setvideo();
}

/* hockey94.asm:Gameloop. The game: a frame, the demo check, the pause when it is asked for. */
void Gameloop(void)
{
    for (;;) {
        DoGameFrame();
        demoread();
        if (g.sflags & HI(sfpz)) Pausemode();
    }
}

/* StartPer after the stack reset */
static void start_period(void)
{
    p_turnoff();
    setupice();
    ResetClock();
    g.PadControlBits |= 0xF000;
    ST_W(g.c1playernum);                                  /* no controlled player yet */
    ST_W(g.c2playernum);
    struct SortObj *puck = sortobj(puckscnum);
    g.fox = 0;                                            /* face off at center ice */
    g.foy = 0;
    uint16_t ass = 0x1B;                                  /* pfaceoff */
    if (g.gmode2 & HI(0)) {                               /* shootout */
        ass = 0x1E;
        g.BA_Skater_Offset = 8;
        g.BA_Goalie_SCnum = 0xB;
    }
    assreplace(ass, puck);
    g.sflags2 |= HI(sf2drec);                             /* don't record */
    g.sflags &= (uint16_t)~HI(sfwrap);                    /* reset the replay */
    g.lastsfx = 0xFFFF;
    g.recbpr = RAM_BASE;                                  /* the replay starts at M68K_RAM */
    g.oldvcount = g.vcount;
    DoGameFrame();                                        /* two frames before the loop */
    g.palcount = 0xFFFF;
    DoGameFrame();
    g.CwdExciteLvl = (uint16_t)(g.gamelevel << 4);
    g.CurCrowdMeter = 0;
    g.HmTeam = g.HomeTeam;
    g.SongIndex = 0;
    ChooseSong();
    song((int16_t)g.SongNum);                             /* the period start tune */
    if ((int16_t)g.gsp < 2) g.sflags3 |= 0x80;            /* periods 1 and 2 */
    Gameloop();
}

/* hockey94.asm:StartPer. Start a period: overtime flag, the faceoff animation, then with a fresh
 * stack: sound off, the rink, the clock, the opening faceoff, two frames, the crowd and the
 * period tune, and the game loop. */
void StartPer(void)
{
    if (g.gsp == 3) g.sflags7 |= 2;                       /* overtime */
    ST_W(g.faceoffanim[0]);
    restart_at(start_period);                             /* movea.w #Stack,sp */
}

/* hockey94.asm:Pausemode. The pause menu: fade out, silence, the menu for the pausing pad's team
 * (shootout or alternate item lists), run it until it is left, then put the rink back (vertical
 * or horizontal as it was), redraw the scores and fade in. */
void Pausemode(void)
{
    forceblack();
    uint16_t v = g.vcount;
    while (g.vcount == v) hal_wait_vblank();
    p_turnoff();
    uint16_t saved = g.sflags;
    rp tm = seta2();
    uint32_t items = SYM_PauseText;
    if (g.gmode2 & HI(0)) items = SYM_PauseMenuItems;
    else if (r8(tm, tmflags) & 4) items = SYM_PauseText2;
    InitMenuState(items, SYM_SetupPauseScreen);
    for (;;) {
        vcountwait();
        uint16_t d0, d1, d2, d3;
        getpzjoy(&d0, &d1, &d2, &d3);
        struct Vid vid = { ram_ptr(g.DMAlistend), ram_ptr(RAM_BASE + RAM_OFF_Satt), 1 };   /* a5 as setvideo left it */
        showclock(&vid);
        ProcessInputWithRepeat(&d1, d2, d3);
        if (HandleMenuInput(&d1)) break;
    }
    forceblack();
    g.sflags = saved;
    if (g.sflags & HI(sfhor)) {
        ReloadRefHorTiles();
        SetHor();
    } else {
        ClrHor();
    }
    vdp_ctrl16(SYM_VDPR_WINX);
    vdp_ctrl16(SYM_VDPR_WINY);
    view_rink_mode();                                     /* (the wide view's 40 columns again) */
    g.disflags |= HI(dfclock);
    g.sflags &= (uint16_t)~HI(sfpz);
    printscores1();
    setvideo();
    g.palcount = 0x18;
    while ((int16_t)g.palcount >= 0) hal_wait_vblank();
    g.oldvcount = g.vcount;
}

/* hockey94.asm:Begin. The cold start from Start: clear the game's RAM ($B000-$DEF3), note PAL,
 * start the sound, look for the 4 Way Play adaptor, the EA Sports screen, save RAM, the line
 * data, the attract screen, the default options, then the title (Opening). */
void Begin(void)
{
    cpu_set_sr(0x2700);
    memset(ram_ptr(RAM_BASE + RAM_OFF_VSCRLPM), 0, 0xDEF4 - RAM_OFF_VSCRLPM);
    uint8_t *palflag = ram_ptr(RAM_BASE + RAM_OFF_PALflag);
    wr16(palflag, 0);
    g.PalMode = vdp_status() & 1;                         /* status bit 0: PAL (50 Hz) */
    if (g.PalMode) palflag[0] |= 1;
    p_initialZ80();
    p_turnoff();
    p_music_vblank();
    KillCrowd();
    Detect4WayPlay();
    g.PadControlBits34 = 0xFFFF;
    EASportsScreen();
    InitSaveRAM();
    ReadLineData();
    HiScoreScreen();
    DefaultMenus();
    g.TmpOptLine2 = g.OptLine;
    g.TempOptPlayMode = g.OptPlayMode;
    orjoy();                                              /* forget the presses so far */
    Opening();
}

/* hockey94.asm:StartGame. A new game: the presses counted, 30 second periods when pad 1 holds
 * Start+A+C, offsides, the shot buffer, the team stats (hot / cold tables in a shootout), the
 * scores, the rink, the crowd record, then the intermission into the first period. Does not
 * return. */
void StartGame(void)
{
    ST_W(g.arenaanim);
    g.homepresses = 0;
    g.awaypresses = 0;
    g.homedirpresses = 0;
    g.awaydirpresses = 0;
    uint16_t d0, d1, d2, d3;
    ReadJoy1(&d0, &d1, &d2, &d3);
    if ((uint8_t)d3 == 0xE0) g.OptPerlen = rules[RULE_PERIOD_LENGTHS] ? 7 : 3;   /* (the secret 30 seconds) */
    g.gmode = 0;
    if (g.OptPen == 1) g.gmode |= 1 << gmoffs;
    if ((int16_t)g.OptPlayMode > 1) ClearShotData();
    clearTeamStats();
    if (g.gmode2 & HI(0)) {
        Create_HotCold_Table(g.tmstruct);
        Create_HotCold_Table(g.AwShots);
    }
    g.ScoreSumbytes = 0;
    g.PenSumLength = 0;
    g.gsp = 0;
    g.ChkCnt = 0;
    restoreteams();
    InitScores();
    setupice();
    LoadCrowdRec();
    IntermissionStart();
}
