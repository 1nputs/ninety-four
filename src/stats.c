/* stats94.asm: the statistics and menu screens (the player stats lists, Other Scores, the line
 * editor, Team Roster, the scoring and penalty summaries, Crowd Meter, Timeout, Change Goalie)
 * and their helpers. Complete. */
#include "game.h"
#include "hal.h"
#include "rom.h"
#include "symbols_gen.h"
#include "stats_strings_gen.h"

#define RAM_W(off) ram_ptr(RAM_BASE + (off))

/* stats94.asm:GetPlayerCount. a2 = team -> d0 = its players (records until a length word 2). */
uint16_t GetPlayerCount(uint8_t *tm)
{
    uint32_t a = rd32(tm + tmdata);
    a += (int16_t)rd16(rom_ptr(a));
    uint16_t n = 0;
    do {
        n++;
        a += (int16_t)rd16(rom_ptr(a));
        a += 8;
    } while (rd16(rom_ptr(a)) != 2);
    return n;
}

/* stats94.asm:GetGoalieCount. a2 = team -> d0 = its goalies (the nibbles of a team data word). */
uint16_t GetGoalieCount(const uint8_t *tm)
{
    uint32_t a = rd32(tm + tmdata);
    a += (int16_t)rd16(rom_ptr(a + 0xA));
    uint16_t w = rd16(rom_ptr(a)), n = 0;
    do { n++; w = (uint16_t)(w << 4); } while (w != 0);
    return n;
}

/* stats94.asm:GetDefenseStart. d0 = the roster index of team a2's first defenseman: its goalies
 * (GetGoalieCount) and forwards (GetForwardCount). */
uint16_t GetDefenseStart(const uint8_t *tm)
{
    return (uint16_t)(GetGoalieCount(tm) + GetForwardCount(tm));
}

/* stats94.asm:GetForwardCount. d0 = team a2's forwards: the high nibble of byte 3 of its team
 * data block at +8. */
uint16_t GetForwardCount(const uint8_t *tm)
{
    uint32_t a = rd32(tm + tmdata);
    a += (uint32_t)(int16_t)rd16(rom_ptr(a + 8));
    return (uint16_t)(rom_ptr(a)[3] >> 4);
}

/* stats94.asm:ReloadRinkGraphics. After a screen over the rink: back to 32 columns, then reload
 * the rink (reversed in a reverse angle replay), team, font, framer, EASN, energy bar and crowd
 * tiles and the palettes. */
void ReloadRinkGraphics(void)
{
    static const uint8_t table[8] = { 0x43, 0x43, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF };
    uint16_t saved = g.disflags;
    g.disflags |= HI(dfng);
    Vmaddr(g.VSCRLPM);
    vdp_data32(0);
    vdp_ctrl16((uint16_t)(0x8D00 | (g.VSCRLPM >> 8 >> 2)));
    vdp_ctrl16(SYM_VDPR_MODE4);
    g.Map3col1 = 5;
    g.disflags = saved;
    g.disflags |= HI(df32c);
    uint16_t c = DecompressGraphicsPlain(g.sflags4 & HI(4) ? SYM_RevRinkTiles : SYM_Rinktiles, g.rinkvrcset);
    LoadHomeTeamGfx(c);
    DecompressGraphicsWithCallback(SYM_SmallFontMap + 8, g.smallfontchars,
                                   rom_find(SYM_ReloadRinkGraphics, SYM_ReloadRinkGraphics + 0x80, table, 8));
    AddFramer(g.framercset);
    setupEASNmap();
    ReloadEnergyBarTiles();
    ReloadCrowdTiles();
    setupIceRinkPal();
    setplayercolors();
}

/* stats94.asm:SetupScreen. The common start of the stats screens: fade out, 40 columns, the
 * framer and small font tiles, the background (ScoutMap) once plain and once rows d0 to d1
 * with its palettes, a second small font, and plane A cleared -> d4 = the char after them. */
uint16_t SetupScreen(uint16_t row, uint16_t rows)
{
    static const uint8_t framer[8] = { 0x91, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF };
    static const uint8_t font2[8] = { 0xD1, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF };
    forceblack();
    g.disflags &= (uint16_t)~HI(dfok);
    uint16_t saved = g.disflags;
    g.disflags |= HI(dfng);
    Vmaddr(g.VSPRITES);
    vdp_data32(0);
    vdp_ctrl16(SYM_VDPR_MODE4 + 0x81);
    g.Map3col1 = 6;
    vdp_ctrl16(SYM_VDPR_HSCROLL);
    Vmaddr(0);
    vdp_data32(0);
    g.disflags = saved;
    g.disflags &= (uint16_t)~HI(df32c);
    DecompressGraphicsWithCallback(SYM_FramerMap + 8, g.framercset, rom_find(SYM_SetupScreen, SYM_SetupScreen + 0x100, framer, 8));
    AddSmallFont(g.smallfontchars);
    printz(str_SetupScreen_1, sizeof str_SetupScreen_1);
    uint32_t map = SYM_ScoutMap + rd32(rom_ptr(SYM_ScoutMap + 4));
    const uint8_t *m = rom_ptr(map);
    uint16_t c = dobitmap(0, map, SYM_NoTiles, 0, 0, rd16(m), rd16(m + 2), 1, 0);
    printz(str_SetupScreen_2, sizeof str_SetupScreen_2);
    g.printy += row;
    uint32_t pal = SYM_ScoutMap + rd32(rom_ptr(SYM_ScoutMap));
    c = dobitmap(pal, map, SYM_ScoutMap + 8, 0, row, 0x28, (uint16_t)(rows - row), c, 0xD);
    g.smallfont2chars = c;
    c = DecompressGraphicsWithCallback(SYM_SmallFontMap + 8, c, rom_find(SYM_SetupScreen, SYM_SetupScreen + 0x100, font2, 8));
    printz(str_SetupScreen_3, sizeof str_SetupScreen_3);
    eraser(0x40, 0x20, 0x7FF);
    g.palcount = 0x18;
    return c;
}

/* stats94.asm:ExitStatsScreen. Leave a stats screen: fade out, then ReloadRinkGraphics. */
void ExitStatsScreen(void)
{
    forceblack();
    ReloadRinkGraphics();
}

/* stats94.asm:WaitVSyncAndReadInput. Wait for the next vblank and a new button press -> d1 (and
 * the other pad words). */
uint16_t WaitVSyncAndReadInput(uint16_t *d0, uint16_t *d2, uint16_t *d3)
{
    uint16_t d1;
    do {
        uint16_t v = g.vcount;
        while (g.vcount == v) hal_wait_vblank();
        getpzjoy(d0, &d1, d2, d3);
        nodiag(&d1, *d3);
    } while ((d1 & 0xFF) == 0);
    return d1;
}

/* ---- the player stats list (Player Stats in the pause menu, Playoff Stats after a playoff game) */

#define SATT RAM_W(RAM_OFF_Satt)                         /* (the sorted player order) */

/* stats94.asm:PlayoffStatsScreen. Playoff Stats: the playoff totals (ReadTeamStats) of the user's
 * playoff team, as PlayerStatsList with d7 = 1. */
void PlayoffStatsScreen(void)
{
    ReadTeamStats();
    uint16_t team = (uint16_t)((g.potreeteam & 0xFF00) | g.potree[g.potreeteam]);
    uint8_t *tm = team_struct(0);
    if (team != rd16(tm + tmteam)) tm = team_struct(1);
    PlayerStatsList(tm, 1);
}

/* stats94.asm:PlayerStatsScreen. Player Stats (a menu item): PlayerStatsList with d7 = 0. */
void PlayerStatsScreen(uint8_t *tm)
{
    PlayerStatsList(tm, 0);
}

/* stats94.asm:PlayerStatsList. The stats of team a2's players, this game's (d7 = 0) or the
 * playoffs' (1): sorted by the column chosen with left / right, scrolled with up / down; A shows
 * the other team (game stats only), Start leaves (ExitStatsScreen). */
void PlayerStatsList(uint8_t *tm, int playoff)
{
    SetupScreen(0xD, 0x16);
    g.DispAttribCtr = 0;
    g.VertLineScrolling = 0;
    for (;;) {                                            /* .redraw */
        g.PlayerScrollCtr = 0;
        printz(str_PlayerStatsList_1, sizeof str_PlayerStatsList_1);
        Framer(0x1E, 6);
        if (playoff) {
            printbigz(str_PlayerStatsList_2, sizeof str_PlayerStatsList_2);
        } else {
            printsmallz(str_PlayerStatsList_3, sizeof str_PlayerStatsList_3);
            printbigz(str_PlayerStatsList_4, sizeof str_PlayerStatsList_4);
        }
        PutTeamBlock(tm == team_struct(0) ? 0 : 0x2C);
        DrawStatsList(tm, playoff);
        DrawStatsListArrows();
        for (;;) {
            vcountwait();
            uint16_t d0, d1, d2, d3;
            getpzjoy(&d0, &d1, &d2, &d3);
            if (d3 & 0x80) { ExitStatsScreen(); return; }
            if (d1 & 0x40) {                              /* A: the other team */
                if (playoff) { UpdateStatsListScroll(tm, playoff); continue; }
                tm = tm == team_struct(0) ? team_struct(1) : team_struct(0);
                break;
            }
            nodiag(&d1, d3);
            int16_t by = 0;
            if (d1 & 8) by = 1;
            else if (d1 & 4) by = -1;
            if (by) {                                     /* the sort column */
                by = (int16_t)(by + g.DispAttribCtr);
                if (by >= -1 && by <= 4) {
                    g.DispAttribCtr = (uint16_t)by;
                    DrawStatsList(tm, playoff);
                    DrawStatsListArrows();
                }
            } else if (d3 & 1) {
                g.PlayerScrollCtr = 0xFFFE;
            } else if (d3 & 2) {
                g.PlayerScrollCtr = 2;
            }
            UpdateStatsListScroll(tm, playoff);
        }
    }
}

/* stats94.asm:UpdateStatsListScroll. Each frame: the list scrolls by PlayerScrollCtr within 0 ...
 * SelectedPlayerIdx, stopping on a 16 line boundary (the arrows then); off a boundary the row
 * coming into view is drawn. */
void UpdateStatsListScroll(uint8_t *tm, int playoff)
{
    if (g.PlayerScrollCtr == 0) return;
    int16_t d0 = (int16_t)(g.PlayerScrollCtr + g.VertLineScrolling);
    if (d0 < 0 || d0 > (int16_t)g.SelectedPlayerIdx) return;
    uint16_t old = g.VertLineScrolling;
    g.VertLineScrolling = (uint16_t)d0;
    d0 &= 0xF;
    if (d0 == 0) {
        DrawStatsListArrows();
        g.PlayerScrollCtr = 0;
    }
    if ((old & 0xF) == 0) {
        if (d0 == 0xE) DrawStatsRowUp(tm, playoff);
        if (d0 == 2) DrawStatsRowDown(tm, playoff);
    }
    SetStatsListScrollReg();
}

/* stats94.asm:SetStatsListScrollReg. The list's vertical scroll: VSRAM word 1 = VertLineScrolling
 * - $68. */
void SetStatsListScrollReg(void)
{
    uint16_t saved = g.disflags;
    g.disflags |= HI(dfng);
    vdp_ctrl(VSRAM_W + (2u << 16));
    vdp_data((uint16_t)(0xFF98 + g.VertLineScrolling));
    g.disflags = saved;
}

/* stats94.asm:DrawStatsRowUp. The row at the top of the window. */
void DrawStatsRowUp(uint8_t *tm, int playoff) { DrawStatsRow((uint16_t)(g.VertLineScrolling >> 4), tm, playoff); }

/* stats94.asm:DrawStatsRowDown. The row below the window. */
void DrawStatsRowDown(uint8_t *tm, int playoff) { DrawStatsRow((uint16_t)((g.VertLineScrolling >> 4) + 6), tm, playoff); }

/* stats94.asm:DrawStatsList. The column headers (the sort column highlighted; the goalie columns
 * for DispAttribCtr -1), the sort column's title, then the players by that column (ties by
 * lower number; goalies only for the goalie column) into Satt, ended by $FF, and the window's six
 * rows. */
void DrawStatsList(uint8_t *tm, int playoff)
{
    printsmallz(str_DrawStatsList_1, sizeof str_DrawStatsList_1);
    uint32_t a1 = SYM_StatColumnHeaders;
    int16_t col = 5;
    if ((int16_t)g.DispAttribCtr >= 0) {
        a1 += (uint32_t)(int16_t)rd16(rom_ptr(a1));
        col = 0;
    }
    do {
        g.printa = (int16_t)g.DispAttribCtr == col ? 0xC000 : 0x8000;
        a1 = printsmall(a1);
        col++;
    } while (col < 5);
    printsmallz(str_DrawStatsList_2, sizeof str_DrawStatsList_2);
    Framer(0x16, 3);
    printz(str_DrawStatsList_3, sizeof str_DrawStatsList_3);
    PrintStringFromList(SYM_StatSortTitles, (uint16_t)(1 + g.DispAttribCtr));
    g.printfontset = 0;
    uint8_t *a3 = SATT;
    uint32_t done = 0;                                    /* d6: the players placed (or left out) */
    if ((int16_t)g.DispAttribCtr < 0) done = ~((1u << (GetGoalieCount(tm) & 31)) - 1);   /* the goalies only */
    uint8_t best;
    do {
        int32_t top = -1;
        best = 0xFF;
        uint32_t data = rd32(tm + tmdata);
        for (int16_t p = (int16_t)GetPlayerCount(tm) - 1; p >= 0; p--) {
            if (done & (1u << (p & 31))) continue;
            uint16_t v = 0;
            if ((int16_t)g.DispAttribCtr >= 0) v = GetStatColumn((uint16_t)p, g.DispAttribCtr, tm, playoff);
            uint32_t a = data + (uint32_t)(int16_t)rd16(rom_ptr(data)) - 8;   /* past his name: his number */
            for (int16_t i = p; i >= 0; i--) { a += 8; a += (uint32_t)(int16_t)rd16(rom_ptr(a)); }
            int32_t key = (int32_t)((uint32_t)(int32_t)(int16_t)v << 8 & 0xFFFFFF00u) | (uint8_t)(0xFF - *rom_ptr(a));
            if (key > top) { top = key; best = (uint8_t)p; }
        }
        done |= 1u << (best & 31);
        *a3++ = best;
    } while (!(best & 0x80));
    for (int i = 5; i >= 0; i--) a3[i] = 0xFF;
    int16_t max = (int16_t)(uint16_t)((uint16_t)(ram_addr(a3)) - 0xC01E);
    if (max < 0) max = 0;
    g.SelectedPlayerIdx = (uint16_t)(max << 4);
    if (g.SelectedPlayerIdx < g.VertLineScrolling) g.VertLineScrolling = g.SelectedPlayerIdx;
    for (int16_t r = 5; r >= 0; r--) DrawStatsRow((uint16_t)((g.VertLineScrolling >> 4) + r), tm, playoff);
    SetStatsListScrollReg();
}

/* stats94.asm:DrawStatsRow. Row d3 of the sorted list (Satt): its rank, the player's name, then his
 * five columns (the sort column highlighted) or, in the goalie column, his saves, shots and save
 * %; nothing past the list's end. */
void DrawStatsRow(uint16_t row, uint8_t *tm, int playoff)
{
    const uint8_t *a4 = SATT + row;
    printz(str_DrawStatsRow_1, sizeof str_DrawStatsRow_1);
    g.printy = (uint16_t)((row * 2) & 0x1F);
    eraser(0x28, 1, 0x7FF);
    if (*a4 & 0x80) return;
    g.printy--;
    g.printx = 1;
    g.printa = 0xE000;
    print(PushNumberWidth((uint16_t)((uint16_t)ram_addr(a4) - 0xC017), 2));   /* the rank */
    uint32_t name = FormatPlayerName(*a4, tm);
    g.printx++;
    g.printa = 0x8000;
    print(name);
    g.printx = 0x11;
    if ((int16_t)g.DispAttribCtr >= 0) {
        for (uint16_t c = 0; c < 5; c++) {
            uint32_t s = PushNumberWidth(GetStatColumn(*a4, c, tm, playoff), 4);
            g.printa = g.DispAttribCtr == c ? 0xC000 : 0x8000;
            print(s);
        }
        return;
    }
    uint16_t shots = ReadStatColumn(*a4, 3, tm, playoff), goals = ReadStatColumn(*a4, 0, tm, playoff);
    int16_t saves = (int16_t)(shots - goals);
    if (saves < 0) saves = 0;
    uint16_t v = (uint16_t)saves;
    uint32_t s = PushNumberWidth(v, 4);
    g.printx++;
    print(s);
    s = PushNumberWidth(shots, 4);
    g.printx += 2;
    print(s);
    if (shots != 0) v = (uint16_t)m68k_divu((uint32_t)v * 100u, shots);
    s = PushNumberWidth(v, 4);
    g.printx += 2;
    print(s);
    printz(str_DrawStatsRow_2, sizeof str_DrawStatsRow_2);
}

/* stats94.asm:GetStatColumn. -> d2 = stat column d1 of player d0 (ReadStatColumn); a goalie has
 * only columns 1 and 2, both his assists. */
uint16_t GetStatColumn(uint16_t player, uint16_t col, uint8_t *tm, int playoff)
{
    if ((int16_t)player >= (int16_t)GetGoalieCount(tm)) return ReadStatColumn(player, col, tm, playoff);
    if (!(6 & (1u << (col & 7)))) return 0;               /* (btst d1,#6) */
    return ReadStatColumn(player, 1, tm, playoff);
}

/* stats94.asm:ReadStatColumn. -> d2 = stat column d1 of player d0: this game's from the team
 * struct's byte arrays (StatColumnOffsets: one or two of them), the playoffs' (d7) from their RAM
 * tables (GetStatsValue). */
uint16_t ReadStatColumn(uint16_t player, uint16_t col, uint8_t *tm, int playoff)
{
    if (!playoff) {
        const uint8_t *a1 = rom_ptr(SYM_StatColumnOffsets) + (uint16_t)(col << 2);
        uint16_t v = tm[(int16_t)(uint16_t)(rd16(a1) + player)];
        int16_t second = (int16_t)rd16(a1 + 2);
        if (second < 0) return v;
        return (uint16_t)(v + tm[(int16_t)(uint16_t)(second + player)]);
    }
    uint16_t at = (uint16_t)(col << 2), off = (uint16_t)(player << 1), v = 0;
    v = GetStatsValue(at, off, v);
    return GetStatsValue((uint16_t)(at + 2), off, v);
}

/* stats94.asm:GetStatsValue. d2 + word d0 of the playoff RAM table at StatsOffsetTbl2 + d1 (0:
 * none). */
uint16_t GetStatsValue(uint16_t at, uint16_t off, uint16_t v)
{
    uint16_t table = rd16(rom_ptr(SYM_StatsOffsetTbl2) + (int16_t)at);
    if (table == 0) return v;
    return (uint16_t)(v + rd16(ram_ptr(RAM_BASE + (uint16_t)(table + off))));
}

/* stats94.asm:DrawStatsListArrows. The up / down arrows (ScrollArrowTbl2): up when scrolled, down
 * when more rows follow. */
void DrawStatsListArrows(void)
{
    printsmallz(str_DrawStatsListArrows_1, sizeof str_DrawStatsListArrows_1);
    uint16_t n = (int16_t)g.VertLineScrolling > 0;
    n = (uint16_t)(n + 2 * ((int16_t)g.VertLineScrolling < (int16_t)g.SelectedPlayerIdx));
    PrintStringFromList(SYM_ScrollArrowTbl2, n);
}

/* ---- the menu screens: Other Scores ------------------------------------------------------------ */

/* stats94.asm:ShowScores. The night's other games (gsstruct) over the Scores bitmap; up / down
 * scroll, Start leaves (ExitStatsScreen). A menu item. */
void ShowScores(void)
{
    uint16_t c = SetupScreen(6, 0x1A);
    printz(str_ShowScores_1, sizeof str_ShowScores_1);
    Framer(0x1E, 6);
    printbigz(str_ShowScores_2, sizeof str_ShowScores_2);
    uint32_t map = SYM_ScoresMap + rd32(rom_ptr(SYM_ScoresMap + 4));
    dobitmap(0, map, SYM_ScoresMap + 8, 0, 0, rd16(rom_ptr(map)), rd16(rom_ptr(map) + 2), c, 0);   /* (d4 as SetupScreen left it) */
    printsmallz(str_ShowScores_3, sizeof str_ShowScores_3);
    g.VertLineScrolling = 0;
    UpdateVertScrollReg();
    uint16_t first;
    int16_t games = (int16_t)GetShifter(&first);
    if (games > 0)
        for (int16_t n = games; n >= 0; n--) DisplayGameInfo((uint16_t)n);
    g.SelectedPlayerIdx = 0;
    int16_t over = (int16_t)(g.printy - 0x15);
    if (over >= 0) {
        g.SelectedPlayerIdx = (uint16_t)(over << 3);      /* the scroll limit */
        DrawScrollArrows();
    }
    for (;;) {
        vcountwait();
        uint16_t d0, d1, d2, d3;
        getpzjoy(&d0, &d1, &d2, &d3);
        if (d3 & 0x80) { ExitStatsScreen(); return; }
        if (d3 & 1) g.PlayerScrollCtr = 0xFFFE;
        else if (d3 & 2) g.PlayerScrollCtr = 2;
        UpdateScoresScroll();
    }
}

/* stats94.asm:UpdateScoresScroll. Each frame: the list scrolls by PlayerScrollCtr within 0 ...
 * SelectedPlayerIdx, stopping on a 24 line boundary (the arrows then); then UpdateVertScrollReg. */
void UpdateScoresScroll(void)
{
    if (g.PlayerScrollCtr == 0) return;
    int16_t d0 = (int16_t)(g.PlayerScrollCtr + g.VertLineScrolling);
    if (d0 < 0 || d0 > (int16_t)g.SelectedPlayerIdx) return;
    g.VertLineScrolling = (uint16_t)d0;
    if ((uint16_t)(m68k_divu((uint32_t)(int32_t)d0, 0x18) >> 16) == 0) {
        DrawScrollArrows();
        g.PlayerScrollCtr = 0;
    }
    UpdateVertScrollReg();
}

/* stats94.asm:UpdateVertScrollReg. VSRAM word 1 = VertLineScrolling - $30. */
void UpdateVertScrollReg(void)
{
    uint16_t saved = g.disflags;
    g.disflags |= HI(dfng);
    vdp_ctrl(VSRAM_W + (2u << 16));
    vdp_data((uint16_t)(0xFFD0 + g.VertLineScrolling));
    g.disflags = saved;
}

/* stats94.asm:DisplayGameInfo. Game d3 of gsstruct (not the user's, gamenum, nor one with flag
 * bit 1 or 2): its two teams and scores (PrintGameInfo) and its period (PerLabels). */
void DisplayGameInfo(uint16_t n)
{
    if (n == g.gamenum) return;
    const uint8_t *gs = g.gsstruct + (int16_t)(uint16_t)(0x10 * n);
    if (gs[0xE] & 6) return;
    PrintGameInfo(rd16(gs + 2), rd16(gs + 0xC));
    g.printy++;
    PrintGameInfo(rd16(gs), rd16(gs + 0xA));
    g.printx = 0x1C;
    PrintStringFromList(SYM_PerLabels, (uint16_t)(rd16(gs + 8) - 1));
    g.printy += 2;
}

/* stats94.asm:PrintGameInfo. Team d0's city at x 6 and score d1 (2 digits) at x $18. */
void PrintGameInfo(uint16_t team, uint16_t score)
{
    uint32_t a1 = rd32(rom_ptr(SYM_TeamList + (uint32_t)(int16_t)(uint16_t)(team << 2)));
    a1 += (uint32_t)(int16_t)rd16(rom_ptr(a1 + 4));
    g.printx = 6;
    print(a1);
    g.printx = 0x18;
    print(PushNumberWidth(score, 2));
}

/* stats94.asm:DrawScrollArrows. Other Scores' up / down arrows (ScrollArrowTable: none, up, down;
 * both reads past it, as on the cartridge). */
void DrawScrollArrows(void)
{
    printsmallz(str_DrawScrollArrows_1, sizeof str_DrawScrollArrows_1);
    uint16_t n = (int16_t)g.VertLineScrolling > 0;
    n = (uint16_t)(n + 2 * ((int16_t)g.VertLineScrolling < (int16_t)g.SelectedPlayerIdx));
    PrintStringFromList(SYM_ScrollArrowTable, n);
}

/* ---- the menu screens: the line editor ---------------------------------------------------------- */

/* the line editor's registers, kept across its routines as the 68k keeps them: d6, the player of
 * the slot under the cursor, and d7, the attribute he was printed with (DrawLineIcon) */
static uint16_t le_player, le_attr;
#define LE_SLOT() rd16((uint8_t *)RAM_W(RAM_OFF_TestList))    /* the cursor's line slot (TestList's first word) */
enum { LE_REDRAW, LE_MENU, LE_SELECT, LE_EXIT, LE_DONE };

/* stats94.asm:LineEditor. The Line Editor for team a2 (a menu item): the slot cursor, C picks a
 * player for the slot (SelectLinePlayer), Start the exit menu (LineEditorExit). The 68k branches
 * between its four routines; here each returns the next. */
void LineEditor(uint8_t *tm)
{
    tm[tmflags] |= 1;
    SetupScreen(0, 0x1C);
    g.DispAttribCtr = 0;
    g.PlayerScrollCtr = 0;
    wr16(RAM_W(RAM_OFF_TestList), 1);
    for (int state = LE_REDRAW;;) {
        switch (state) {
        case LE_REDRAW: state = LineEditorRedraw(tm); break;
        case LE_MENU: state = LineEditorMenu(tm); break;
        case LE_SELECT: state = SelectLinePlayer(tm); break;
        case LE_EXIT: state = LineEditorExit(tm); break;
        default: ExitStatsScreen(); return;
        }
    }
}

/* stats94.asm:LineEditorRedraw. Clear the screen, the line icons to be drawn again; on to
 * LineEditorMenu. */
int LineEditorRedraw(uint8_t *tm)
{
    printsmallz(str_LineEditorRedraw_1, sizeof str_LineEditorRedraw_1);
    eraser(0x28, 0x1C, 0x7FF);
    g.redrawicons = 0xFF;
    ClearMenuFlags();
    return LE_MENU;
}

/* stats94.asm:LineEditorMenu. The screen and the icons, then the slot cursor: Start -> the exit
 * menu, C -> a player for the slot, the pad moves the cursor (LineCursorTable; only the first
 * line's five slots with OptLine). */
int LineEditorMenu(uint8_t *tm)
{
    DrawTeamScreen(tm);
    DrawLineIcons(tm);
    for (;;) {
        vcountwait();
        uint16_t d0, d1, d2, d3;
        getpzjoy(&d0, &d1, &d2, &d3);
        ProcessInputWithRepeat(&d1, d2, d3);
        if (d1 & 0x80) return LE_EXIT;
        if (d1 & 0x20) return LE_SELECT;
        int16_t by;
        if (d1 & 2) by = 1;
        else if (d1 & 1) by = -1;
        else if (d1 & 8) by = 8;
        else if (d1 & 4) by = -8;
        else continue;
        by = (int16_t)(by + LE_SLOT());
        if (g.OptLine != 0 && (by < 1 || by > 5)) continue;
        RAM_W(RAM_OFF_TestList)[1] = rom_ptr(SYM_LineCursorTable)[8 + by];
        DrawLineIcons(tm);
    }
}

/* stats94.asm:SelectLinePlayer. The players who can take the slot (forwards, or the defensemen
 * for slots 1-2) in Satt, the list at the slot's player; up / down choose, left / right page the
 * attribute columns, C puts the choice in the slot (SetLineSlot), Start goes back. */
int SelectLinePlayer(uint8_t *tm)
{
    uint16_t first = GetGoalieCount(tm), n = GetForwardCount(tm);
    if ((LE_SLOT() & 7) <= 2) {                           /* the defense */
        first = (uint16_t)(first + n);
        n = (uint16_t)(GetPlayerCount(tm) - first);
    }
    n--;
    g.screentimer = n;                                    /* (the list's last row) */
    g.PlayerScrollCtr = 0;
    g.VertLineScrolling = 0;
    uint8_t *list = SATT;
    for (uint16_t i = 0; ; i++, first++) {
        list[i] = (uint8_t)first;
        if (first == le_player) g.VertLineScrolling = i;
        if (n-- == 0) break;
    }
    ClearLineEditorTop();
    printsmallz(str_SelectLinePlayer_1, sizeof str_SelectLinePlayer_1);
    Framer(0x12, 3);
    printsmallz(str_SelectLinePlayer_2, sizeof str_SelectLinePlayer_2);
    Framer(0x12, 3);
    printsmallz(str_SelectLinePlayer_3, sizeof str_SelectLinePlayer_3);
    int16_t d0 = 0;
    goto vert;
    for (;;) {
        vcountwait();
        uint16_t j0, d1, d2, d3;
        getpzjoy(&j0, &d1, &d2, &d3);
        ProcessInputWithRepeat(&d1, d2, d3);
        if (d1 & 0x80) return LE_MENU;
        if (d1 & 0x20) {                                  /* C: the pick */
            SetLineSlot((uint8_t)(SATT[g.VertLineScrolling] + 1), LE_SLOT(), tm);
            return LE_MENU;
        }
        if (d1 & 2) { d0 = 1; goto vert; }
        if (d1 & 8) { d0 = 1; goto horz; }
        if (d1 & 1) { d0 = -1; goto vert; }
        if (d1 & 4) { d0 = -1; goto horz; }
        continue;
    horz:
        d0 = (int16_t)(d0 + g.DispAttribCtr);
        if (d0 < 0) continue;
        g.DispAttribCtr = (uint16_t)d0;
        PrintAttribHeader(tm);
        continue;
    vert:
        d0 = (int16_t)(d0 + g.VertLineScrolling);
        if (d0 < 0 || d0 > (int16_t)g.screentimer) continue;
        g.VertLineScrolling = (uint16_t)d0;
        if (d0 <= (int16_t)g.PlayerScrollCtr) g.PlayerScrollCtr = (uint16_t)d0;   /* the window follows */
        d0 = (int16_t)(d0 - 5);
        if (d0 > (int16_t)g.PlayerScrollCtr) g.PlayerScrollCtr = (uint16_t)d0;
        PrintAttribHeader(tm);
    }
}

/* the attribute column entry n of a column list (PAttribColumns / GAttribColumns: a header String,
 * then its selector long), stepping DispAttribCtr back when the list is shorter */
static uint32_t attrib_column(uint32_t list)
{
    for (;;) {
        uint32_t a1 = list;
        int16_t n = (int16_t)g.DispAttribCtr;
        while ((int16_t)rd16(rom_ptr(a1)) >= 0 && n-- != 0) a1 += (uint32_t)(int16_t)rd16(rom_ptr(a1)) + 4;   /* (dbmi) */
        if ((int16_t)rd16(rom_ptr(a1)) >= 0) return a1;
        g.DispAttribCtr--;
    }
}

/* stats94.asm:PrintAttribHeader. The player list of the line editor: the header of attribute
 * column DispAttribCtr, then six rows of names and that attribute, the chosen one highlighted. */
void PrintAttribHeader(uint8_t *tm)
{
    printz(str_PrintAttribHeader_1, sizeof str_PrintAttribHeader_1);
    uint32_t a1 = print(attrib_column(SYM_PAttribColumns));
    uint32_t sel = rd32(rom_ptr(a1));
    uint16_t row = g.PlayerScrollCtr, rows = (uint16_t)(g.screentimer - row);
    if (rows > 5) rows = 5;
    g.printy = 2;
    for (;; row++) {
        printsmallz(str_PrintAttribHeader_2, sizeof str_PrintAttribHeader_2);
        if (row == g.VertLineScrolling) g.printa = le_attr;
        GetNameandAttrib(SATT[row], sel, tm);
        if (rows-- == 0) break;
    }
}

/* stats94.asm:DrawLineIcons. The lines drawn with the cursor's (LineIconGroups' masks; the area
 * cleared when they change), then the cursor slot's player in a box. */
void DrawLineIcons(uint8_t *tm)
{
    uint32_t a0 = SYM_LineIconGroups + (LE_SLOT() >> 3);
    if (g.OptLine != 0) a0--;
    uint8_t lines = *rom_ptr(a0);
    if (lines != g.redrawicons) {
        g.redrawicons = lines;
        ClearLineArea();
    }
    for (int16_t line = 6; line >= 0; line--)
        if (g.redrawicons & (1u << line)) DrawLineIcon((uint16_t)line, tm);
    printsmallz(str_DrawLineIcons_1, sizeof str_DrawLineIcons_1);
    Framer(0x18, 3);
    printsmallz(str_DrawLineIcons_2, sizeof str_DrawLineIcons_2);
    uint32_t name = getname(le_player, tm);
    g.printx -= (uint16_t)(rd16(mem_ptr(name)) >> 1);
    g.printa = le_attr;
    printsmall(name);
    g.printfontset = 0;
}

/* stats94.asm:DrawLineIcon. Line d5: its name (linelist) and " Line", then its players' last
 * names, at LineIconPosTable (the entry before with OptLine); the cursor's slot in the bold font,
 * its player and attribute kept (d6, d7). */
void DrawLineIcon(uint16_t line, uint8_t *tm)
{
    uint32_t a0 = SYM_LineIconPosTable + (uint16_t)(6 * line);
    if (g.OptLine != 0) a0 -= 6;
    const uint8_t *pos = rom_ptr(a0);
    g.printx = rd16(pos);
    g.printy = rd16(pos + 2);
    g.printa = 0x8000;
    PrintStringFromList(SYM_linelist, line);
    printsmallz(str_DrawLineIcon_1, sizeof str_DrawLineIcon_1);
    g.printx = rd16(pos);
    uint16_t slot = (uint16_t)((line << 3) + 1);
    g.printy = rd16(pos + 2);
    const uint8_t *slots = tm + tmlines;
    for (int16_t n = (int16_t)rd16(pos + 4); n >= 0; n--, slot++) {
        g.printx = rd16(pos);
        printsmallz(str_DrawLineIcon_2, sizeof str_DrawLineIcon_2);
        uint16_t player = (uint16_t)(slots[slot] - 1);
        uint32_t name = FormatPlayerNameShort(player, tm);
        if (slot == LE_SLOT()) {
            le_player = player;
            le_attr = g.printa;
            g.printfontset = 2;
        }
        printsmall(name);
        g.printfontset = 0;
    }
}

/* stats94.asm:ClearLineArea. Erase the lines' area and print the position header. */
void ClearLineArea(void)
{
    printsmallz(str_ClearLineArea_1, sizeof str_ClearLineArea_1);
    eraser(0x28, 0x12, 0x7FF);
    g.printy = rd16(rom_ptr(SYM_LineIconPosTable + 2));
    g.printx = rd16(rom_ptr(SYM_LineIconPosTable));
    if (g.OptLine != 0) g.printx = rd16(rom_ptr(SYM_LineIconPosTable - 6));
    printsmallz(str_ClearLineArea_2, sizeof str_ClearLineArea_2);
}

/* stats94.asm:DrawTeamScreen. The line editor's background (ScoutMap), frame and title, and the
 * team's logo map (PutTeamBlock). */
void DrawTeamScreen(uint8_t *tm)
{
    ClearLineEditorTop();
    printz(str_DrawTeamScreen_1, sizeof str_DrawTeamScreen_1);
    uint32_t map = SYM_ScoutMap + rd32(rom_ptr(SYM_ScoutMap + 4));
    dobitmap(0, map, SYM_NoTiles, 0, 0, rd16(rom_ptr(map)), rd16(rom_ptr(map) + 2), 1, 0);
    printz(str_DrawTeamScreen_2, sizeof str_DrawTeamScreen_2);
    Framer(0x1A, 6);
    printbigz(str_DrawTeamScreen_3, sizeof str_DrawTeamScreen_3);
    PutTeamBlock(tm == team_struct(0) ? 0 : 0x2C);
}

/* stats94.asm:ClearLineEditorTop. Erase 40 x 10 at the top of map 2. */
void ClearLineEditorTop(void)
{
    printz(str_ClearLineEditorTop_1, sizeof str_ClearLineEditorTop_1);
    eraser(0x28, 0xA, 0x7FF);
}

/* stats94.asm:ClearMenuFlags. Colours $2D and $3D of palfadenew cleared. */
void ClearMenuFlags(void)
{
    g.palfadenew[0x2D] = 0;
    g.palfadenew[0x3D] = 0;
}

/* stats94.asm:LineEditorExit. Start in the line editor: its menu (LineEditorMenuText, or
 * LineEditorMenuNoLoad when databuffer holds another team's lines), the pause menu's state kept;
 * its first item leaves (ExitStatsScreen), the others redraw the editor. */
int LineEditorExit(uint8_t *tm)
{
    ClearMenuFlags();
    g.palcount = 0x18;
    uint32_t item = g.menuitem, list = g.menulist, draw = g.menudraw;
    uint32_t items = SYM_LineEditorMenuText;
    if ((uint8_t)(rd16(tm + tmteam) + 1) != g.databuffer[0]) items = SYM_LineEditorMenuNoLoad;
    printsmallz(str_LineEditorExit_1, sizeof str_LineEditorExit_1);
    InitMenuState(items, SYM_rtss2);
    for (;;) {
        vcountwait();
        uint16_t d0, d1, d2, d3;
        getpzjoy(&d0, &d1, &d2, &d3);
        ProcessInputWithRepeat(&d1, d2, d3);
        uint16_t keep = d1;
        HandleMenuInput(&d1);
        if (keep & 0xA0) break;                           /* Start or C */
    }
    printsmallz(str_LineEditorExit_2, sizeof str_LineEditorExit_2);
    uint16_t chosen = (uint16_t)(g.menuitem >> 16);
    g.menudraw = draw;
    g.menulist = list;
    g.menuitem = item;
    return chosen != 0 ? LE_REDRAW : LE_DONE;
}

/* stats94.asm:SetLineSlot. Player d0 (1 based) to line slot d2 of team a2; when he is already in
 * another slot of that line, the slot's old player goes there (a swap). */
void SetLineSlot(uint8_t player, uint16_t slot, uint8_t *tm)
{
    uint8_t *lines = tm + tmlines, *line = lines + (int16_t)(slot & 0xFFF8);
    for (int16_t i = 5; i >= 0; i--)
        if (line[1 + i] == player) { line[1 + i] = lines[(int16_t)slot]; break; }
    lines[(int16_t)slot] = player;
}

/* stats94.asm:DecodeTeamLines. Team a2's lines from databuffer (byte 0 the team + 1, then a nibble
 * a slot in AttributeOffsetTbl order, relative to the first forward, or defenseman for slots
 * 1-2). A menu item. */
void DecodeTeamLines(uint8_t *tm)
{
    const uint8_t *a0 = g.databuffer + 1, *slots = rom_ptr(SYM_AttributeOffsetTbl);
    uint16_t goalies = GetGoalieCount(tm), forwards = GetForwardCount(tm);
    int odd = 0;
    for (; !(*slots & 0x80); slots++) {
        uint8_t v;
        odd ^= 1;
        if (odd) v = *a0;
        else v = (uint8_t)(*a0++ >> 4);
        v = (uint8_t)((v & 0xF) + goalies);
        if ((*slots & 7) <= 2) v = (uint8_t)(v + forwards);
        SetLineSlot((uint8_t)(v + 1), *slots, tm);
    }
}

/* stats94.asm:EncodeTeamLines. The reverse: team a2's lines packed into databuffer, saved
 * (WriteLineData). A menu item. */
void EncodeTeamLines(uint8_t *tm)
{
    uint8_t *a0 = g.databuffer;
    *a0++ = (uint8_t)(rd16(tm + tmteam) + 1);
    const uint8_t *lines = tm + tmlines, *slots = rom_ptr(SYM_AttributeOffsetTbl);
    uint16_t goalies = GetGoalieCount(tm), forwards = GetForwardCount(tm);
    int odd = 0;
    for (; !(*slots & 0x80); slots++) {
        uint8_t v = (uint8_t)(lines[*slots] - 1 - goalies);
        if ((*slots & 7) <= 2) v = (uint8_t)(v - forwards);
        odd ^= 1;
        if (odd) *a0 = v;
        else *a0++ |= (uint8_t)(v << 4);
    }
    WriteLineData();
}

/* ---- the menu screens: the team roster ---------------------------------------------------------- */

/* stats94.asm:TeamRosterScreen. Team a2's roster (a menu item): pages of players (the goalies,
 * then each line: DisplayPlayerList), left / right the attribute column, up / down the page, A
 * the other team, Start leaves. */
void TeamRosterScreen(uint8_t *tm)
{
    SetupScreen(0xD, 0x17);
    g.SelectedPlayerIdx = (uint16_t)(1 + rd16(tm + tmline));
    g.DispAttribCtr = 0;
    for (;;) {                                            /* .redraw */
        printz(str_TeamRosterScreen_1, sizeof str_TeamRosterScreen_1);
        Framer(0x1A, 6);
        printbigz(str_TeamRosterScreen_2, sizeof str_TeamRosterScreen_2);
        PutTeamBlock(tm == team_struct(0) ? 0 : 0x2C);
        printsmallz(str_TeamRosterScreen_3, sizeof str_TeamRosterScreen_3);
        printz(str_TeamRosterScreen_4, sizeof str_TeamRosterScreen_4);
        Framer(0x12, 3);
        printz(str_TeamRosterScreen_5, sizeof str_TeamRosterScreen_5);
        Framer(0x12, 3);
        DisplayPlayerList(tm);
        g.VertLineScrolling = (uint16_t)(g.SelectedPlayerIdx * 0x80);
        UpdatePlayerListScroll(g.VertLineScrolling, tm);
        g.PlayerScrollCtr = 0;
        for (;;) {
            vcountwait();
            uint16_t d0, d1, d2, d3;
            getpzjoy(&d0, &d1, &d2, &d3);
            ProcessInputWithRepeat(&d1, d2, d3);
            if (d3 & 0x80) { ExitStatsScreen(); return; }
            if (d1 & 0x40) break;                         /* A: the other team */
            nodiag(&d1, d3);
            int16_t by = 0;
            if (d1 & 8) by = 1;
            else if (d1 & 4) by = -1;
            if (by) {
                by = (int16_t)(by + g.DispAttribCtr);
                if (by >= 0) {
                    g.DispAttribCtr = (uint16_t)by;
                    DisplayPlayerList(tm);
                }
            } else if (g.PlayerScrollCtr == 0) {
                if (d3 & 1) g.PlayerScrollCtr = 0xFFFE;
                else if (d3 & 2) g.PlayerScrollCtr = 2;
            }
            CheckPlayerListScroll(tm);
        }
        tm = tm == team_struct(0) ? team_struct(1) : team_struct(0);
    }
}

/* stats94.asm:CheckPlayerListScroll. Each frame: VertLineScrolling + PlayerScrollCtr within 0 ...
 * $380 (UpdatePlayerListScroll), else the scroll stops. */
void CheckPlayerListScroll(uint8_t *tm)
{
    if (g.PlayerScrollCtr == 0) return;
    int16_t d0 = (int16_t)(g.PlayerScrollCtr + g.VertLineScrolling);
    if (d0 < 0 || d0 > 0x380) { StopPlayerListScroll(); return; }
    UpdatePlayerListScroll((uint16_t)d0, tm);
}

/* stats94.asm:UpdatePlayerListScroll. VertLineScrolling = d0; on a page boundary ($80) the scroll
 * stops; leaving one, the page coming into view is drawn; VSRAM word 1 = VertLineScrolling - $70. */
void UpdatePlayerListScroll(uint16_t v, uint8_t *tm)
{
    uint16_t old = g.VertLineScrolling;
    g.VertLineScrolling = v;
    uint32_t q = (uint32_t)m68k_divs((int32_t)(int16_t)v, 0x80);
    uint16_t rem = (uint16_t)(q >> 16), page = (uint16_t)q;
    if (rem == 0) g.PlayerScrollCtr = 0;
    if ((old & 0x7F) == 0) {
        if (rem == 0x7E) DisplayPlayerListUp(page, tm);
        if (rem == 2) DisplayPlayerListDown(page, tm);
    }
    uint16_t saved = g.disflags;
    g.disflags |= HI(dfng);
    vdp_ctrl(VSRAM_W + (2u << 16));
    vdp_data((uint16_t)(0xFF90 + g.VertLineScrolling));
    g.disflags = saved;
}

/* stats94.asm:StopPlayerListScroll. Out of range: the scroll stops. */
void StopPlayerListScroll(void)
{
    g.PlayerScrollCtr = 0;
}

/* stats94.asm:DisplayPlayerListUp. Roster page d0 (the quotient in d0's high word). */
void DisplayPlayerListUp(uint16_t page, uint8_t *tm)
{
    g.SelectedPlayerIdx = page;
    DisplayPlayerList(tm);
}

/* stats94.asm:DisplayPlayerListDown. The page after it. */
void DisplayPlayerListDown(uint16_t page, uint8_t *tm)
{
    g.SelectedPlayerIdx = (uint16_t)(page + 1);
    DisplayPlayerList(tm);
}

/* stats94.asm:DisplayPlayerList. Roster page SelectedPlayerIdx (0 the goalies, 1-7 the lines): its
 * title (PlayerStatMenuTxt), the attribute column's header, then its rows: the position
 * (GoalieRowText / PlayerPositionText), the name and the attribute (GetNameandAttrib). */
void DisplayPlayerList(uint8_t *tm)
{
    printsmallz(str_DisplayPlayerList_1, sizeof str_DisplayPlayerList_1);
    printsmall(AdvanceStringPtr(SYM_PlayerStatMenuTxt, g.SelectedPlayerIdx));
    printsmallz(str_DisplayPlayerList_2, sizeof str_DisplayPlayerList_2);
    uint32_t a1 = printsmall(attrib_column(g.SelectedPlayerIdx ? SYM_PAttribColumns : SYM_GAttribColumns));
    uint32_t sel = rd32(rom_ptr(a1));
    printsmallz(str_DisplayPlayerList_3, sizeof str_DisplayPlayerList_3);
    if (g.SelectedPlayerIdx & 1) g.printy += 0x10;
    uint16_t y = g.printy;
    eraser(0x28, 0x10, 0x7FF);
    g.printy = y;
    int16_t rows = (int16_t)(GetGoalieCount(tm) - 1);
    uint32_t text = SYM_GoalieRowText;
    uint8_t *who = SATT;
    wr32(who, 0x01020304);                                /* the goalies */
    if ((int16_t)(g.SelectedPlayerIdx - 1) >= 0) {
        who = tm + tmlines + (int16_t)(uint16_t)((g.SelectedPlayerIdx - 1) << 3) + 1;
        text = SYM_PlayerPositionText;
        rows = (int16_t)g.SelectedPlayerIdx > 5 ? 3 : 4;
    }
    for (; rows >= 0; rows--) {
        g.printx = 2;
        text = print(text);
        g.printx = 7;
        GetNameandAttrib((uint16_t)(*who++ - 1), sel, tm);
        g.printy += 2;
    }
}

/* stats94.asm:GetNameandAttrib. Player d0's name, then at x $1E the column d4's low word picks
 * (attribjmp: status, energy, handedness, weight, fighting, rating); for those past energy his
 * rating of the attributes in d4's high word (CalcAttrib) goes to it. */
void GetNameandAttrib(uint16_t player, uint32_t sel, uint8_t *tm)
{
    print(getname(player, tm));
    g.printx = 0x1E;
    uint16_t v = player, div = 0, which = (uint16_t)sel;
    if (which > 2) v = CalcAttrib(player, tm, sel, &div);
    uint32_t handler = SYM_attribjmp + (uint32_t)(int16_t)rd16(rom_ptr(SYM_attribjmp + (int16_t)which));
    if (handler == SYM_AttribStatus) AttribStatus(v, tm);
    else if (handler == SYM_AttribEnergy) AttribEnergy(v, tm);
    else if (handler == SYM_AttribHanded) AttribHanded(v);
    else if (handler == SYM_AttribWeight) AttribWeight(v);
    else if (handler == SYM_AttribFighting) AttribFighting(v, div);
    else AttribRating(v, div);
}

/* stats94.asm:AttribStatus. Player d0's status word ($66 of the team struct): Ice, Bench, an injury,
 * or his penalty time. */
void AttribStatus(uint16_t player, uint8_t *tm)
{
    uint16_t st = rd16(tm + tmpdst + (int16_t)(uint16_t)(player * 2)), n;
    if ((int16_t)st < 0) {
        n = (uint16_t)~st;
        if (n > 3) n = 1;
    } else if (st & 0x1000) {
        n = 3;                                            /* injured */
    } else {                                              /* in the box */
        g.printx--;
        print(PushTime(st & 0xFFF));
        n = (st & 0x4000) ? 5 : 4;
    }
    PrintStringFromList(SYM_StatusTextTbl, n);
}

/* stats94.asm:AttribEnergy. Energy: his word at $32 of the team struct / 40, at most 100. */
void AttribEnergy(uint16_t player, uint8_t *tm)
{
    uint16_t e = (uint16_t)m68k_divu((uint32_t)(int32_t)(int16_t)rd16(tm + tmpde + (int16_t)(uint16_t)(player * 2)), 0x28);
    if ((int16_t)e > 0x64) e = 0x64;
    AttribPrintPct(e);
}

/* stats94.asm:AttribFighting. Fighting: the nibble without its bit 0 (the handedness), a rating
 * out of d1 - 1. */
void AttribFighting(uint16_t v, uint16_t div)
{
    AttribRating((uint16_t)(v & 0xE), (uint16_t)(div - 1));
}

/* stats94.asm:AttribRating. d0 x 100 / d1, adjusted (AttribAdjust), printed (AttribPrintPct). */
void AttribRating(uint16_t v, uint16_t div)
{
    AttribPrintPct(AttribAdjust((uint16_t)m68k_divu((uint32_t)v * 100u, div)));
}

/* stats94.asm:AttribPrintPct. d0, 4 wide, then 4 blanks. */
void AttribPrintPct(uint16_t v)
{
    print(PushNumberWidth(v, 4));
    printz(str_AttribPrintPct_1, sizeof str_AttribPrintPct_1);
}

/* stats94.asm:AttribHanded. Bit 0 of the sum: Righty / Lefty. */
void AttribHanded(uint16_t v)
{
    PrintStringFromList(SYM_HandedTextTbl, (uint16_t)((v & 1) ^ 1));
}

/* stats94.asm:AttribWeight. Weight: 140 + 8 x the rating, in lb. */
void AttribWeight(uint16_t v)
{
    print(PushNumber((uint16_t)((v << 3) + 0x8C)));
    printz(str_AttribWeight_1, sizeof str_AttribWeight_1);
}

/* ---- the menu screens: the scoring and penalty summaries ---------------------------------------- */

/* the summaries' input: Start leaves (-> 1), up / down set the scroll */
static int summary_input(void)
{
    vcountwait();
    uint16_t d0, d1, d2, d3;
    getpzjoy(&d0, &d1, &d2, &d3);
    if (d3 & 0x80) return 1;
    if (d3 & 1) g.PlayerScrollCtr = 0xFFFE;
    else if (d3 & 2) g.PlayerScrollCtr = 2;
    return 0;
}

/* stats94.asm:ScoringSummaryScreen. The Scoring Summary (a menu item): a 4 row entry a goal
 * (ScoreSum), scrolled to the last, then with up / down; Start leaves. */
void ScoringSummaryScreen(void)
{
    SetupScreen(0xA, 0x1A);
    printz(str_ScoringSummaryScreen_1, sizeof str_ScoringSummaryScreen_1);
    Framer(0x20, 6);
    printbigz(str_ScoringSummaryScreen_2, sizeof str_ScoringSummaryScreen_2);
    PutTeamBlock(0x2C);
    g.printx += 4;
    PutTeamBlock(0);
    printsmallz(str_ScoringSummaryScreen_3, sizeof str_ScoringSummaryScreen_3);
    uint16_t v = (uint16_t)((uint16_t)m68k_divu(g.ScoreSumbytes, 6) << 5);
    g.VertLineScrolling = v;
    int16_t max = (int16_t)(v - 0x80);
    if (max < 0) max = 0;
    g.SelectedPlayerIdx = (uint16_t)max;
    v = g.VertLineScrolling;
    do {                                                  /* up 2 lines a frame to the last screenful */
        vcountwait();
        UpdateScoringScroll(v);
        v = (uint16_t)(g.VertLineScrolling - 2);
    } while ((int16_t)v >= (int16_t)g.SelectedPlayerIdx);
    g.PlayerScrollCtr = 0;
    while (!summary_input()) CheckScoringScroll();
    ExitStatsScreen();
}

/* stats94.asm:CheckScoringScroll. Each frame: VertLineScrolling + PlayerScrollCtr within 0 ...
 * SelectedPlayerIdx (UpdateScoringScroll). */
void CheckScoringScroll(void)
{
    if (g.PlayerScrollCtr == 0) return;
    int16_t d0 = (int16_t)(g.PlayerScrollCtr + g.VertLineScrolling);
    if (d0 < 0 || d0 > (int16_t)g.SelectedPlayerIdx) return;
    UpdateScoringScroll((uint16_t)d0);
}

/* stats94.asm:UpdateScoringScroll. VertLineScrolling = d0; on an entry boundary (32 lines) the
 * arrows and the scroll stops; leaving one, the entry coming into view is drawn; VSRAM word 1 =
 * VertLineScrolling - $50. */
void UpdateScoringScroll(uint16_t v)
{
    uint16_t old = g.VertLineScrolling;
    g.VertLineScrolling = v;
    uint32_t q = (uint32_t)m68k_divs((int32_t)(int16_t)v, 0x20);
    uint16_t rem = (uint16_t)(q >> 16), entry = (uint16_t)q;
    if (rem == 0) {
        DrawScrollArrowsPenalty();
        g.PlayerScrollCtr = 0;
    }
    if ((old & 0x1F) == 0) {
        if (rem == 0x1E) DrawScoringRowUp(entry);
        if (rem == 2) DrawScoringRowDown(entry);
    }
    uint16_t saved = g.disflags;
    g.disflags |= HI(dfng);
    vdp_ctrl(VSRAM_W + (2u << 16));
    vdp_data((uint16_t)(0xFFB0 + g.VertLineScrolling));
    g.disflags = saved;
}

/* stats94.asm:DrawScoringRowUp. Goal entry d0 (the quotient) at the top of the window. */
void DrawScoringRowUp(uint16_t entry)
{
    uint16_t at = (uint16_t)(6 * entry);
    printz(str_DrawScoringRowUp_1, sizeof str_DrawScoringRowUp_1);
    g.printy = (uint16_t)(((g.VertLineScrolling >> 3) - 3) & 0x1F);
    DrawScoringRow(at);
}

/* stats94.asm:DrawScoringRowDown. Goal entry d0 + 4 at the bottom of the window. */
void DrawScoringRowDown(uint16_t entry)
{
    uint16_t at = (uint16_t)(6 * (uint16_t)(entry + 4));
    printz(str_DrawScoringRowDown_1, sizeof str_DrawScoringRowDown_1);
    g.printy = (uint16_t)(((g.VertLineScrolling >> 3) + 0x10) & 0x1F);
    DrawScoringRow(at);
}

/* the abbreviation of the team of a summary entry (bit 7 of its byte 2: the visitors) at x $C */
static void summary_team(uint8_t flags)
{
    const uint8_t *tm = team_struct(flags & 0x80);
    uint32_t a1 = rd32(tm + tmdata);
    a1 += (uint32_t)(int16_t)rd16(rom_ptr(a1 + 4));
    a1 += (uint32_t)(int16_t)rd16(rom_ptr(a1));
    g.printx = 0xC;
    print(a1);
}

/* stats94.asm:DrawScoringRow. ScoreSum entry d3: the time, the team, the goal type (GoalTypeTbl),
 * the scorer and his two assists (PrintScorerName). */
void DrawScoringRow(uint16_t at)
{
    uint16_t y = g.printy;
    eraser(0x28, 4, 0x7FF);
    g.printy = y;
    const uint8_t *e = RAM_W(RAM_OFF_ScoreSum) + (int16_t)at;
    g.printx = 1;
    FormatAndPrintTime(rd16(e));
    summary_team(e[2]);
    g.printx = 0x23;
    PrintStringFromList(SYM_GoalTypeTbl, e[2] & 0x7F);
    uint8_t *tm = team_struct(e[2] & 0x80);
    g.printx = 0x10;
    PrintScorerName(e[3], tm);
    g.printa = 0xE000;
    g.printx = 0x12;
    PrintScorerName(e[4], tm);
    g.printx = 0x12;
    PrintScorerName(e[5], tm);
}

/* stats94.asm:PrintScorerName. Player d0 (a byte; negative: none) and down a row. */
void PrintScorerName(uint8_t player, uint8_t *tm)
{
    if (!(player & 0x80)) print(FormatPlayerNumInitial(player, tm));
    g.printy++;
}

/* stats94.asm:PenaltySummaryScreen. The Penalty Summary (a menu item): a 3 row entry a penalty
 * (PenSum), scrolled as the Scoring Summary. */
void PenaltySummaryScreen(void)
{
    SetupScreen(0xA, 0x19);
    printz(str_PenaltySummaryScreen_1, sizeof str_PenaltySummaryScreen_1);
    Framer(0x20, 6);
    printbigz(str_PenaltySummaryScreen_2, sizeof str_PenaltySummaryScreen_2);
    PutTeamBlock(0x2C);
    g.printx += 4;
    PutTeamBlock(0);
    printsmallz(str_PenaltySummaryScreen_3, sizeof str_PenaltySummaryScreen_3);
    uint16_t v = (uint16_t)((uint16_t)(g.PenSumLength >> 2) * 0x18);
    g.VertLineScrolling = v;
    int16_t max = (int16_t)(v - 0x78);
    if (max < 0) max = 0;
    g.SelectedPlayerIdx = (uint16_t)max;
    v = g.VertLineScrolling;
    do {                                                  /* up 2 lines a frame to the last screenful */
        vcountwait();
        UpdatePenaltyScroll(v);
        v = (uint16_t)(g.VertLineScrolling - 2);
    } while ((int16_t)v >= (int16_t)g.SelectedPlayerIdx);
    g.PlayerScrollCtr = 0;
    while (!summary_input()) CheckPenaltyScroll();
    ExitStatsScreen();
}

/* stats94.asm:CheckPenaltyScroll. Each frame: VertLineScrolling + PlayerScrollCtr within 0 ...
 * SelectedPlayerIdx (UpdatePenaltyScroll). */
void CheckPenaltyScroll(void)
{
    if (g.PlayerScrollCtr == 0) return;
    int16_t d0 = (int16_t)(g.PlayerScrollCtr + g.VertLineScrolling);
    if (d0 < 0 || d0 > (int16_t)g.SelectedPlayerIdx) return;
    UpdatePenaltyScroll((uint16_t)d0);
}

/* stats94.asm:UpdatePenaltyScroll. As UpdateScoringScroll with 24 line entries. */
void UpdatePenaltyScroll(uint16_t v)
{
    uint16_t old = g.VertLineScrolling;
    g.VertLineScrolling = v;
    uint32_t q = (uint32_t)m68k_divs((int32_t)(int16_t)v, 0x18);
    uint16_t rem = (uint16_t)(q >> 16), entry = (uint16_t)q;
    if (rem == 0) {
        DrawScrollArrowsPenalty();
        g.PlayerScrollCtr = 0;
    }
    if ((uint16_t)(m68k_divs((int32_t)(int16_t)old, 0x18) >> 16) == 0) {
        if (rem == 0x16) DisplayPenaltyLineUp(entry);
        if (rem == 2) DisplayPenaltyLineDown(entry);
    }
    uint16_t saved = g.disflags;
    g.disflags |= HI(dfng);
    vdp_ctrl(VSRAM_W + (2u << 16));
    vdp_data((uint16_t)(0xFFB0 + g.VertLineScrolling));
    g.disflags = saved;
}

/* stats94.asm:DisplayPenaltyLineUp. Penalty entry d0 (the quotient) at the top of the window. */
void DisplayPenaltyLineUp(uint16_t entry)
{
    uint16_t at = (uint16_t)(entry << 2);
    printz(str_DisplayPenaltyLineUp_1, sizeof str_DisplayPenaltyLineUp_1);
    g.printy = (uint16_t)(((g.VertLineScrolling >> 3) - 2) & 0x1F);
    DisplayPenaltyEntry(at);
}

/* stats94.asm:DisplayPenaltyLineDown. Penalty entry d0 + 5 at the bottom of the window. */
void DisplayPenaltyLineDown(uint16_t entry)
{
    uint16_t at = (uint16_t)((uint16_t)(entry + 5) << 2);
    printz(str_DisplayPenaltyLineDown_1, sizeof str_DisplayPenaltyLineDown_1);
    g.printy = (uint16_t)(((g.VertLineScrolling >> 3) + 0xF) & 0x1F);
    DisplayPenaltyEntry(at);
}

/* stats94.asm:DisplayPenaltyEntry. PenSum entry d3: the time, the team, the minutes and the
 * penalty's name (Penaltylist), the player. */
void DisplayPenaltyEntry(uint16_t at)
{
    uint16_t y = g.printy;
    eraser(0x28, 3, 0x7FF);
    g.printy = y;
    const uint8_t *e = RAM_W(RAM_OFF_PenSum) + (int16_t)at;
    g.printx = 1;
    FormatAndPrintTime(rd16(e));
    summary_team(e[2]);
    uint8_t *tm = team_struct(e[2] & 0x80);
    uint16_t kind = e[2] & 0x7F;
    uint32_t pen = SYM_Penaltylist + (uint32_t)(int16_t)rd16(rom_ptr(SYM_Penaltylist + kind));
    uint32_t mins = PushNumber(rom_ptr(pen)[1]);
    g.printx = 0x23;
    print(mins);
    g.printx = 0x10;
    print(FormatPlayerNumInitial(e[3], tm));
    g.printy++;
    g.printx = 0x13;
    g.printa = 0xE000;
    print(pen + 2);
}

/* stats94.asm:DrawScrollArrowsPenalty. The summaries' up / down arrows (ScrollArrowTbl). */
void DrawScrollArrowsPenalty(void)
{
    printsmallz(str_DrawScrollArrowsPenalty_1, sizeof str_DrawScrollArrowsPenalty_1);
    uint16_t n = (int16_t)g.VertLineScrolling > 0;
    n = (uint16_t)(n + 2 * ((int16_t)g.VertLineScrolling < (int16_t)g.SelectedPlayerIdx));
    PrintStringFromList(SYM_ScrollArrowTbl, n);
}

/* ---- the menu screens: the crowd meter, the timeout, the goalie ------------------------------- */

/* stats94.asm:CrowdMeterScreen. The Crowd Meter (a menu item): the levels and records
 * (PrintCrowdMeter) until Start. */
void CrowdMeterScreen(void)
{
    SetupScreen(9, 0x1C);
    printz(str_CrowdMeterScreen_1, sizeof str_CrowdMeterScreen_1);
    Framer(0x1C, 6);
    printbigz(str_CrowdMeterScreen_2, sizeof str_CrowdMeterScreen_2);
    PutTeamBlock(0x2C);
    g.printx += 4;
    PutTeamBlock(0);
    PrintCrowdMeter();
    for (;;) {
        uint16_t d0, d2, d3;
        if (WaitVSyncAndReadInput(&d0, &d2, &d3) & 0x80) break;
    }
    ExitStatsScreen();
}

/* stats94.asm:PrintCrowdMeter. The current, average and peak crowd levels in dB; with save RAM the
 * home arena's record and the league's (ReadCrowdRecord; 0 counts as 80 dB). */
void PrintCrowdMeter(void)
{
    printsmallz(str_PrintCrowdMeter_1, sizeof str_PrintCrowdMeter_1);
    PrintCrowdDB(g.CwdExciteLvl);
    printsmallz(str_PrintCrowdMeter_2, sizeof str_PrintCrowdMeter_2);
    PrintCrowdDB((uint16_t)m68k_divu(g.SumCwdExciteLvl, g.NumCwdExciteLvl));
    printsmallz(str_PrintCrowdMeter_3, sizeof str_PrintCrowdMeter_3);
    PrintCrowdDB(g.MaxCwdExciteLvl);
    if ((int16_t)g.ValidSRAM < 0) return;
    ReadCrowdRecord((uint32_t)(int32_t)(int16_t)g.HomeTeam, RAM_BASE + RAM_OFF_ThreeStars);
    uint16_t db = g.ThreeStars[8] ? g.ThreeStars[8] : 0x50;
    printsmallz(str_PrintCrowdMeter_4, sizeof str_PrintCrowdMeter_4);
    print(PushNumberWidth(db, 3));
    printz(str_PrintCrowdMeter_5, sizeof str_PrintCrowdMeter_5);
    ReadCrowdRecord((uint32_t)(int32_t)(int16_t)GetLeagueCrowdRecord(), RAM_BASE + RAM_OFF_ThreeStars);
    db = g.ThreeStars[8] ? g.ThreeStars[8] : 0x50;
    printsmallz(str_PrintCrowdMeter_6, sizeof str_PrintCrowdMeter_6);
    print(PushNumberWidth(db, 3));
    printz(str_PrintCrowdMeter_7, sizeof str_PrintCrowdMeter_7);
}

/* stats94.asm:PrintCrowdDB. Crowd level d0 as sqrt(d0 x 4) + 65 dB. */
void PrintCrowdDB(uint16_t level)
{
    uint32_t v = (uint32_t)(int32_t)(int16_t)level;
    v = (v & 0xFFFF0000u) | (uint16_t)(v << 2);           /* (ext.l, asl.w) */
    print(PushNumberWidth((uint16_t)(sroot(v) + 0x41), 3));
    printz(str_PrintCrowdDB_1, sizeof str_PrintCrowdDB_1);
}

/* stats94.asm:TimeoutMenu. Timeout for team a2 (a pause menu item): the menu now PauseText2 (no
 * second timeout), the team's name, both teams rested (RestoreTeamEnergy), then $78 frames. */
void TimeoutMenu(uint8_t *tm)
{
    uint8_t *mi = RAM_W(RAM_OFF_menuitem);
    wr16(mi, (uint16_t)(rd16(mi) - 1));
    wr16(mi + 2, (uint16_t)(rd16(mi + 2) - 1));
    g.menulist = SYM_PauseText2;
    tm[tmflags] |= 4;
    printz(str_TimeoutMenu_1, sizeof str_TimeoutMenu_1);
    Framer(0x16, 6);
    printz(str_TimeoutMenu_2, sizeof str_TimeoutMenu_2);
    uint32_t city = rd32(tm + tmdata);
    city += (uint32_t)(int16_t)rd16(rom_ptr(city + 4));
    g.printx -= (uint16_t)(rd16(rom_ptr(city)) >> 1);
    print(city);
    RestoreTeamEnergy(team_struct(0));
    RestoreTeamEnergy(team_struct(1));
    uint16_t d0, d1, d2, d3;
    waitx(0x78, &d0, &d1, &d2, &d3);
}

/* stats94.asm:SelectGoalieMenu. Change Goalie for team a2 (a menu item): its goalies (or none)
 * listed (DrawGoalieSelectList); up / down, then C or Start: the goalie ($26 of the team struct,
 * SetPersonel). The menu's state kept. */
void SelectGoalieMenu(uint8_t *tm)
{
    uint8_t *mi = RAM_W(RAM_OFF_menuitem);
    uint16_t item = rd16(mi), top = rd16(mi + 2);
    wr16(mi + 2, GetGoalieCount(tm));
    printz(str_SelectGoalieMenu_1, sizeof str_SelectGoalieMenu_1);
    Framer(0x18, (uint16_t)(3 + rd16(mi + 2)));
    int16_t cur = (int16_t)rd16(tm + tmgoalie);
    if (cur < 0) cur = -1;
    wr16(mi, (uint16_t)(cur + 1));
    for (;;) {
        DrawGoalieSelectList(tm);
        uint16_t d0, d2, d3, d1;
        d1 = WaitVSyncAndReadInput(&d0, &d2, &d3);
        if (d1 & 0xA0) break;
        if (d1 & 2) {
            uint16_t n = (uint16_t)(rd16(mi) + 1);
            if ((int16_t)n > (int16_t)rd16(mi + 2)) continue;
            wr16(mi, n);
        }
        if (d1 & 1) {
            wr16(mi, (uint16_t)(rd16(mi) - 1));
            if ((int16_t)rd16(mi) < 0) wr16(mi, 0);
        }
    }
    int16_t g0 = (int16_t)(rd16(mi) - 1);
    if (g0 >= 0 || (int16_t)rd16(tm + tmgoalie) >= -1) {
        wr16(tm + tmgoalie, (uint16_t)g0);
        setpersonel(tm);
    }
    wr16(mi + 2, top);
    wr16(mi, item);
}

/* stats94.asm:DrawGoalieSelectList. The goalie list from row $D: "no goalie", then each goalie's
 * number and name, row menuitem highlighted. */
void DrawGoalieSelectList(uint8_t *tm)
{
    const uint8_t *mi = RAM_W(RAM_OFF_menuitem);
    g.printy = 0xD;
    int16_t rows = (int16_t)rd16(mi + 2);
    for (uint16_t n = 0; rows >= 0; rows--, n++) {
        g.printx = 5;
        g.printa = n == rd16(mi) ? 0x8000 : 0xA000;
        printz(str_DrawGoalieSelectList_1, sizeof str_DrawGoalieSelectList_1);
        if (n == 0) {
            g.printx = 0xB;
            printz(str_DrawGoalieSelectList_2, sizeof str_DrawGoalieSelectList_2);
        } else {
            uint32_t a1 = rd32(tm + tmdata);
            a1 += (uint32_t)(int16_t)rd16(rom_ptr(a1));
            for (uint16_t i = (uint16_t)(n - 1); i-- != 0;) a1 += (uint32_t)(int16_t)rd16(rom_ptr(a1)) + 8;
            g.printx = 8;
            a1 = print(a1);
            uint8_t num = *rom_ptr(a1);
            uint8_t *buf = RAM_W(RAM_OFF_TextBuffer);
            buf[0] = (uint8_t)((num >> 4) + '0');
            buf[1] = (uint8_t)((num & 0xF) + '0');
            wr16(RAM_W(RAM_OFF_mesarea), 4);
            g.printx = 5;
            print(RAM_BASE + RAM_OFF_mesarea);
        }
        g.printy++;
    }
}
