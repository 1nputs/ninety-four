/* period94.asm: period and game statistics. Ends a shootout, clears the game stats, animates
 * the arena sprites and shows the Period Stats and Game Statistics screens, with the power play
 * time and team ratings they report. */
#include <string.h>
#include "game.h"
#include "hal.h"
#include "rom.h"
#include "symbols_gen.h"
#include "period_strings_gen.h"

#define PFLAG(o, b) ((o)->pflags & (1 << (b)))

/* period94.asm:EndShootout. Shootout over: freeze the window, then the 5 skaters of the winning
 * side are set up above or below the view and celebrate (assscore). */
void EndShootout(void)
{
    g.sflags2 |= HI(sf2drec);
    freezewindow();
    g.shootoutdelay = 0x60;
    struct SortObj *a3 = sortobj((int16_t)g.homeshootgoals > (int16_t)g.awayshootgoals ? 0 : 6);
    for (int i = 0; i < 5; i++) {
        a3++;
        setplayer(6, a3);
        a3->position = 2;
        int16_t d0 = (int16_t)g.Vpos < 0 ? 0xF0 : -0xF0;
        a3->Ypos = (int16_t)(d0 + (int16_t)g.Vpos);
        a3->Xpos = 0;
        assinsert(7, a3);                                 /* assscore */
        a3->pflags &= (uint8_t)~(1 << pfalock);
        a3->pflags2 &= (uint8_t)~2;
        a3->pflags &= (uint8_t)~(1 << pfnc);
    }
}

/* period94.asm:ClampPositionBox. Unreferenced: clamp temp3 / temp4 of a3 to the box of his
 * position. */
void ClampPositionBox(struct SortObj *o)
{
    const uint8_t *box = rom_ptr(PFLAG(o, pfgoal) ? SYM_PositionBoxesTop : SYM_PositionBoxes) + (o->position - 1) * 2;
    int16_t d0 = (int16_t)rd16(box);
    if (!(d0 < o->temp3)) o->temp3 = d0;
    d0 = (int16_t)rd16(box + 2);
    if (!(d0 > o->temp3)) o->temp3 = d0;
    d0 = (int16_t)rd16(box + 4);
    if (!(d0 < o->temp4)) o->temp4 = d0;
    d0 = (int16_t)rd16(box + 6);
    if (!(d0 > o->temp4)) o->temp4 = d0;
}

/* period94.asm:ClearGameStats. Clear the shootout state, both team structs, PenBuf and the
 * game flags. */
void ClearGameStats(void)
{
    uint8_t *a0 = ram_ptr(RAM_BASE + RAM_OFF_shootoutstate);
    for (int i = 0; i < 0x13; i++) wr16(a0 + i * 2, 0);
    a0 = team_struct(0);
    for (int i = 0; i < 0x364; i++) wr16(a0 + i * 2, 0);
    a0 = ram_ptr(RAM_BASE + RAM_OFF_PenBuf);
    for (int i = 0; i < 0x25; i++) wr16(a0 + i * 2, 0);
    g.BA_PS_flags = 0;
    g.sflags4 = 0;
    g.sflags6 = 0;
    g.gmode2 = 0;
    g.setupcardflags = 0;
    g.featuredplayer = 0;
}

/* period94.asm:AddArenaAnimSprite. Add the frame arenaframe of the arena animation to the
 * sprite table (64 sprites at most) for the pieces inside the view. Not in a reverse angle
 * replay, nor on the horizontal rink. */
void AddArenaAnimSprite(uint8_t **sat, uint16_t *link)
{
    if ((int16_t)g.arenaanim < 0) return;
    if (g.arenaspritelist == 0) return;
    if (g.sflags4 & HI(4)) return;
    if (g.sflags2 & HI(sf2replay)) return;
    if (g.sflags & HI(sfhor)) return;
    const uint8_t *a1 = rom_ptr(g.arenaspritelist);
    a1 += rd32(a1 + 4);
    int16_t d0 = (int8_t)g.arenaframe;
    if (d0 == 0) return;
    if ((int16_t)*link >= 0x40) return;
    const uint8_t *a0 = a1;
    int16_t n = (int16_t)(((rd16(a0 + d0 * 2 + 2) - rd16(a0 + d0 * 2)) >> 3) - 1);
    a0 += rd16(a0 + d0 * 2);
    int16_t x0 = (int16_t)((int16_t)g.Hpos + 0xC0 - 0x90), x1 = (int16_t)((int16_t)g.Hpos + 0xC0 + 0x80);
    int16_t y0 = (int16_t)(0x170 - (int16_t)g.Vpos - 0x80), y1 = (int16_t)(0x170 - (int16_t)g.Vpos + 0x70);
    uint8_t *a6 = *sat;
    uint16_t d6 = *link;
    for (; n >= 0; n--, a0 += 8) {
        int16_t sx = (int16_t)rd16(a0), sy = (int16_t)rd16(a0 + 2);
        if (y0 > sy || y1 < sy || x0 > sx || x1 < sx) continue;
        wr16(a6, (uint16_t)(sy + 0x70 - y0));
        a6[2] = a0[7];
        a6[3] = (uint8_t)d6;
        wr16(a6 + 4, (uint16_t)((rd16(a0 + 6) & 0xF800) + rd16(a0 + 4) + g.arenaanimchars));
        wr16(a6 + 6, (uint16_t)(sx + 0x70 - x0));
        a6 += 8;
        d6++;
        if (d6 == 0x40) break;
    }
    *sat = a6;
    *link = d6;
}

/* period94.asm:PrintPlayerNameRight. The name of player d0 of team a2, moved left so it ends
 * before column $29 (a trailing pad byte or two not counted), then the hot / cold icon. */
void PrintPlayerNameRight(uint16_t player, uint8_t *tm)
{
    g.iconplayer = player;
    uint32_t a1 = FormatPlayerName(player, tm);
    const uint8_t *a0 = ram_ptr(a1);
    uint16_t d5 = rd16(a0);
    int16_t d0 = (int16_t)d5;
    if (a0[d5 - 1] == 0) {
        d0--;
        if (a0[d5 - 2] == 0) d0--;
    }
    d0 = (int16_t)(d0 + g.printx - 0x29);
    if (d0 >= 0) g.printx -= d0;
    print(a1);
    HotColdIcon(tm);
}

/* period94.asm:SkipNameNumber. Unreferenced: the "NN F. Last" string without its first 2
 * characters. */
uint32_t SkipNameNumber(uint16_t player, uint8_t *tm)
{
    uint32_t a1 = FormatPlayerNumInitial(player, tm);
    uint8_t *p = ram_ptr(a1);
    wr16(p + 2, (uint16_t)(rd16(p) - 2));
    return a1 + 2;
}

/* period94.asm:SetStatScroll. Plane A vertical scroll = VertLineScrolling - $50, with disflags
 * bit 2 set while it writes. */
void SetStatScroll(void)
{
    uint16_t saved = g.disflags;
    g.disflags |= HI(dfng);
    vdp_ctrl(VSRAM_W + (2u << 16));
    vdp_data((uint16_t)(0xFFB0 + g.VertLineScrolling));
    g.disflags = saved;
}

/* period94.asm:StatScrollRow / StatScrollRowEnd. The row of the scroll position. */
uint16_t StatScrollRow(void) { return g.VertLineScrolling >> 4; }
uint16_t StatScrollRowEnd(void) { return (uint16_t)((g.VertLineScrolling >> 4) + 6); }

/* period94.asm:PrintStatScrollArrows. The scroll arrows of the Game Statistics screen. */
void PrintStatScrollArrows(void)
{
    printsmallz(str_PrintStatScrollArrows_1, sizeof str_PrintStatScrollArrows_1);
    if (g.VertLineScrolling != 0) printsmallz(str_PrintStatScrollArrows_2, sizeof str_PrintStatScrollArrows_2);
    else printsmallz(str_PrintStatScrollArrows_3, sizeof str_PrintStatScrollArrows_3);
    if (g.matchupvis != g.VertLineScrolling) printsmallz(str_PrintStatScrollArrows_4, sizeof str_PrintStatScrollArrows_4);
    else printsmallz(str_PrintStatScrollArrows_5, sizeof str_PrintStatScrollArrows_5);
    printsmallz(str_PrintStatScrollArrows_6, sizeof str_PrintStatScrollArrows_6);
}

/* period94.asm:DisplayTeamStatsScreen. The team logos, then each stat row (without the power
 * play rows when penalties are off) centred, with the home value at x $22 and the visitors'
 * at x 9. */
void DisplayTeamStatsScreen(void)
{
    printz(str_DisplayTeamStatsScreen_1, sizeof str_DisplayTeamStatsScreen_1);
    PutTeamBlock(0x2C);
    printz(str_DisplayTeamStatsScreen_2, sizeof str_DisplayTeamStatsScreen_2);
    PutTeamBlock(0);
    SetStatScroll();
    printz(str_DisplayTeamStatsScreen_3, sizeof str_DisplayTeamStatsScreen_3);
    int rows = 0xE;
    uint32_t a1 = SYM_TeamStatTextTbl;
    if (g.OptPen == 0) { rows = 0xA; a1 = SYM_TeamStatTextTblNoPen; }
    for (; rows >= 0; rows--) {
        g.printx = (uint16_t)(0x15 - (rd16(rom_ptr(a1)) >> 1));
        uint32_t a0 = print(a1);                          /* print leaves a1 after the string */
        g.printx = 0x22;
        PrintStatValue(rom_ptr(a0), team_struct(0));
        g.printx = 9;
        PrintStatValue(rom_ptr(a0), team_struct(1));
        a1 = a0 + 4;
        g.printy += 2;
    }
}

/* period94.asm:PrintStatValue. Print the stat row a0 (two team struct offsets) for team a2
 * centred at printx: the value (offsets $A and $352 are times), "/second" if any, " (pct%)"
 * for passing; offset $FFFF is the shooting percentage. */
void PrintStatValue(const uint8_t *a0, const uint8_t *tm)
{
    uint32_t mes = RAM_BASE + RAM_OFF_mesarea;
    wr16(ram_ptr(mes), 2);
    uint16_t off = rd16(a0);
    if (off == 0xFFFF) {
        uint32_t d0 = (uint32_t)rd16(tm + tmscore) * 0x64;
        uint16_t d1 = rd16(tm);
        if (d1 != 0) d0 = m68k_divu(d0, d1);
        appstring(PushNumber((uint16_t)d0), mes);
        appendz(str_PrintStatValue_1, sizeof str_PrintStatValue_1, mes);
    } else {
        uint16_t d0 = rd16(tm + off);
        uint32_t a1;
        if (off == 0x352) a1 = PushTime(d0);
        else {
            if (off != 0xA) a1 = PushNumber(d0);
            else a1 = PushTime(d0);
        }
        appstring(a1, mes);
        int16_t off2 = (int16_t)rd16(a0 + 2);
        if (off2 >= 0) {
            appendz(str_PrintStatValue_2, sizeof str_PrintStatValue_2, mes);
            appstring(PushNumber(rd16(tm + off2)), mes);
            if (off == 0x14) {                            /* passing: the percentage */
                appendz(str_PrintStatValue_3, sizeof str_PrintStatValue_3, mes);
                uint32_t p = (uint32_t)rd16(tm + off) * 0x64;
                uint16_t d1 = rd16(tm + off2);
                if (d1 != 0) p = m68k_divu(p, d1);
                appstring(PushNumber((uint16_t)p), mes);
                appendz(str_PrintStatValue_4, sizeof str_PrintStatValue_4, mes);
            }
        }
    }
    g.printx -= rd16(ram_ptr(mes)) >> 1;
    print(mes);
}

/* period94.asm:updatePPTeamTime. One more second of power play time for the team on the power
 * play. */
void updatePPTeamTime(void)
{
    if (!(g.sflags2 & HI(sf2pwrplay))) return;
    uint8_t *tm = team_struct((g.sflags2 & HI(sf2pwrtm)) != 0);
    wr16(tm + tmpptime, (uint16_t)(rd16(tm + tmpptime) + 1));
}

/* period94.asm:GetTeamRating. The TeamRatings byte of team $28(a2). */
uint16_t GetTeamRating(const uint8_t *tm)
{
    return rom_ptr(LG(TeamRatings))[rd16(tm + tmteam)];
}

/* period94.asm:ShortenMsgTimer. Cap msgtimer at 2, unless a second pad is on and a3 is not the
 * puck carrier. */
void ShortenMsgTimer(struct SortObj *o)
{
    if (g.cont2team != 0 && (uint16_t)o->SCnum != g.puckc) return;
    if ((int16_t)g.msgtimer > 2) g.msgtimer = 2;
}

/* PeriodStatsScreen: team's logo bitmap (TeamLogoBitmaps; its palette is TeamLogoPalettes +
 * team * 32, loaded as palette 2) at printx / printy -> the next char */
static uint16_t team_logo(uint16_t team, uint16_t chars)
{
    uint32_t bmp = rd32(rom_ptr(LG(TeamLogoBitmaps) + (uint16_t)(team << 2)));
    uint32_t pal = LG(TeamLogoPalettes) + (int16_t)((uint16_t)(team << 5) - 0x40);
    uint32_t map = bmp + rd32(rom_ptr(bmp + 4));
    return dobitmap(pal, map, bmp + 8, 0, 0, 6, 6, chars, 4);
}

/* PeriodStatsScreen's .num: the next period total of a0, added to matchuptimer, in 2 or 3
 * columns */
static const uint8_t *period_num(const uint8_t *a0)
{
    uint16_t v = rd16(a0);
    g.matchuptimer += v;
    printz(str_PeriodStatsScreen_15, sizeof str_PeriodStatsScreen_15);
    g.printx -= 3;
    print(PushNumberWidth(v, (int16_t)v < 0x64 ? 2 : 3));
    return a0 + 2;
}

/* PeriodStatsScreen's .team: team a2's goals (or shots, matchup) by period, then the total */
static void period_team(const uint8_t *tm)
{
    const uint8_t *a0 = tm + (g.matchup ? 0x34A : 0x342);
    g.matchuptimer = 0;
    g.printx = 0xB;
    a0 = period_num(a0);
    if ((int16_t)g.gsp >= 1) {
        g.printx = 0x10;
        a0 = period_num(a0);
        if ((int16_t)g.gsp >= 2) {
            g.printx = 0x15;
            a0 = period_num(a0);
            if ((int16_t)g.gsp >= 3 && (g.sflags7 & 2)) {
                g.printx = 0x1A;
                period_num(a0);
            }
        }
    }
    g.printx = 0x21;
    printz(str_PeriodStatsScreen_14, sizeof str_PeriodStatsScreen_14);
    g.printx -= 3;
    print(PushNumberWidth(g.matchuptimer, (int16_t)g.matchuptimer < 0x64 ? 2 : 3));
}

/* PeriodStatsScreen's .title and .lines */
static void period_title(void)
{
    if (g.matchup) printz(str_PeriodStatsScreen_11, sizeof str_PeriodStatsScreen_11);
    else printz(str_PeriodStatsScreen_12, sizeof str_PeriodStatsScreen_12);
}
static void period_lines(void)
{
    printz(str_PeriodStatsScreen_13, sizeof str_PeriodStatsScreen_13);
    g.printy = 0xE;
    period_team(team_struct(1));
    g.printy = 0x16;
    period_team(team_struct(0));
}

/* period94.asm:PeriodStatsScreen. The "Period Stats" menu item: both team logos, then each
 * team's goals (or shots) by period with the overtime column when there was one, and the
 * total. Left shows goals, right shots, start leaves. */
void PeriodStatsScreen(void)
{
    g.matchup = 0;
    uint16_t c = SetupScreen(0, 0x1C);
    printz(str_PeriodStatsScreen_1, sizeof str_PeriodStatsScreen_1);
    Framer(8, 8);
    printz(str_PeriodStatsScreen_2, sizeof str_PeriodStatsScreen_2);
    Framer(8, 8);
    printz(str_PeriodStatsScreen_3, sizeof str_PeriodStatsScreen_3);
    c = team_logo(g.VisTeam, c);
    uint8_t *pal = ram_ptr(RAM_BASE + RAM_OFF_palfadenew);
    memcpy(pal + 0x60, pal + 0x40, 0x20);
    printz(str_PeriodStatsScreen_4, sizeof str_PeriodStatsScreen_4);
    team_logo(g.HomeTeam, c);
    printz(str_PeriodStatsScreen_5, sizeof str_PeriodStatsScreen_5);
    Framer(0x20, 6);
    printbigz(str_PeriodStatsScreen_6, sizeof str_PeriodStatsScreen_6);
    if (g.sflags7 & 2) {
        printz(str_PeriodStatsScreen_7, sizeof str_PeriodStatsScreen_7);
        printz(str_PeriodStatsScreen_8, sizeof str_PeriodStatsScreen_8);
    } else {
        printz(str_PeriodStatsScreen_9, sizeof str_PeriodStatsScreen_9);
        printz(str_PeriodStatsScreen_10, sizeof str_PeriodStatsScreen_10);
    }
    period_title();
    period_lines();
    g.palcount = 0x18;
    for (;;) {
        uint16_t d0, d2, d3;
        uint16_t d1 = WaitVSyncAndReadInput(&d0, &d2, &d3);
        if (d1 & 0x80) break;
        if (d1 & 4) {
            if (!g.matchup) continue;
            g.matchup = 0;
        } else if (d1 & 8) {
            if (g.matchup) continue;
            ST_W(g.matchup);
        } else {
            continue;
        }
        period_title();
        period_lines();
    }
    ExitStatsScreen();
}

/* period94.asm:rtsStatTables. The scroll step's rts. */
void rtsStatTables(void) {}

/* GameStatisticsScreen's .scroll: move the rows by PlayerScrollCtr within 0..matchupvis,
 * stopping at a row boundary; at the boundaries print the row that scrolls in */
static void stats_scroll(void)
{
    int16_t step = (int16_t)g.PlayerScrollCtr;
    if (step == 0) { rtsStatTables(); return; }
    int16_t to = (int16_t)(step + (int16_t)g.VertLineScrolling);
    if (to < 0 || to > (int16_t)g.matchupvis) { rtsStatTables(); return; }
    uint16_t from = g.VertLineScrolling;
    g.VertLineScrolling = (uint16_t)to;
    uint16_t sub = (uint16_t)to & 0xF;
    if (sub == 0) g.PlayerScrollCtr = 0;
    if ((from & 0xF) == 0) {
        if (sub == 0xE) StatScrollRow();
        if (sub == 2) StatScrollRowEnd();
    }
    SetStatScroll();
}

/* period94.asm:GameStatisticsScreen. The "Game Statistics" menu item: both teams' totals
 * (DisplayTeamStatsScreen) under the team blocks; up / down scroll the rows (to $70, or $30
 * without penalties), start leaves. */
void GameStatisticsScreen(void)
{
    SetupScreen(9, 0x1A);
    printz(str_GameStatisticsScreen_1, sizeof str_GameStatisticsScreen_1);
    Framer(0x20, 6);
    printbigz(str_GameStatisticsScreen_2, sizeof str_GameStatisticsScreen_2);
    PutTeamBlock(0x2C);
    g.printx += 4;
    PutTeamBlock(0);
    g.DispAttribCtr = 0;
    g.VertLineScrolling = 0;
    g.PlayerScrollCtr = 0;
    g.matchupvis = 0x70;
    if (g.OptPen == 0) g.matchupvis = 0x30;
    DisplayTeamStatsScreen();
    PrintStatScrollArrows();
    for (;;) {
        vcountwait();
        uint16_t d0, d1, d2, d3;
        getpzjoy(&d0, &d1, &d2, &d3);
        if (d3 & 0x80) { ExitStatsScreen(); return; }
        nodiag(&d1, d3);
        if (d3 & 1) g.PlayerScrollCtr = 0xFFFE;
        else if (d3 & 2) g.PlayerScrollCtr = 2;
        stats_scroll();
        PrintStatScrollArrows();
    }
}
