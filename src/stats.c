/* stats94.asm: the statistics screens. Only the parts the game play calls are here so far. */
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
    if (team != rd16(tm + 0x28)) tm = team_struct(1);
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
    vdp_ctrl(0x40000010 + (2u << 16));
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
        uint32_t data = rd32(tm + 0x1E);
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
