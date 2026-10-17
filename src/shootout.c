/* shootout94.asm: the shootout: its play (the shooters in turn, the skate paths, the score) and
 * the Shootout SetUp menu screen. Complete. */
#include <string.h>
#include "game.h"
#include "rom.h"
#include "symbols_gen.h"
#include "shootout_strings_gen.h"

#define RAM_W(off) ram_ptr(RAM_BASE + (off))

/* shootout94.asm:ShootoutModeBox. "SHOOTOUT MODE": the shooter vs. the goalie, both teams'
 * shootout goals and the round. */
void ShootoutModeBox(void)
{
    prefmes(-1);
    printz(str_ShootoutModeBox_1, sizeof str_ShootoutModeBox_1);
    Framer(0x1B, 0xC);
    printbig(SYM_ShootoutModeBigTxt);
    uint16_t x = g.printx;
    print(FormatPlayerNumInitial(g.BA_Skater_Offset, team_struct(g.BA_Team != 0)));
    g.printx = x;
    g.printy++;
    g.printx += 6;
    printz(str_ShootoutModeBox_2, sizeof str_ShootoutModeBox_2);
    print(FormatPlayerNumInitial(sortobj((int16_t)g.BA_Goalie_SCnum)->pnum, team_struct(g.BA_Team == 0)));
    for (int away = 0; away < 2; away++) {
        printz(away ? str_ShootoutModeBox_4 : str_ShootoutModeBox_3, away ? sizeof str_ShootoutModeBox_4 : sizeof str_ShootoutModeBox_3);
        uint32_t data = rd32(team_struct(away) + tmdata);
        print(data + (int16_t)rd16(rom_ptr(data + 4)));
        g.printx = 0x15;
        print(PushNumberWidth(away ? g.awayshootgoals : g.homeshootgoals, 3));
    }
    printz(str_ShootoutModeBox_5, sizeof str_ShootoutModeBox_5);
    print(PushNumberWidth(g.shootoutround, 2));
}

/* shootout94.asm:NextShooter. The next shootout shooter: BA_Team, the goalie he faces, his
 * roster slot from the team's shooter list, and a new skate path (for a3). After the end of the
 * shootout (gmode2 bit 3), back to the front end. */
void NextShooter(struct SortObj *o)
{
    if (g.gmode2 & HI(3)) { ExitToOpening(); return; }
    StartShootoutPath(o);
    g.BA_Team = g.shootoutteam;
    g.BA_Goalie_SCnum = 0xB;
    const uint8_t *list = RAM_W(RAM_OFF_homeshooters);
    if (g.shootoutteam) { g.BA_Goalie_SCnum = 5; list = RAM_W(RAM_OFF_awayshooters); }
    g.BA_Skater_Offset = rd16(list + (int16_t)(g.homeshootnum << 1));
    sortobj(puckscnum)->pflags &= (uint8_t)~4;
}

/* shootout94.asm:CountShootoutGoals. After each shot: once both teams have shot in round 5 or
 * later with the goals not level, the shootout ends (gmode2 bit 3, EndShootout); else the other
 * team shoots, and after both, the next shooter and round. */
void CountShootoutGoals(void)
{
    if ((g.shootoutteam & 1) && (int16_t)g.shootoutround >= 5 && g.homeshootgoals != g.awayshootgoals) {
        g.gmode2 |= HI(3);
        EndShootout();
        return;
    }
    g.shootoutteam ^= 1;
    if (g.shootoutteam) return;
    if ((int16_t)++g.homeshootnum >= 5) g.homeshootnum = 0;
    g.shootoutround++;
}

/* shootout94.asm:ShootoutWonBy. "SHOOTOUT WON BY" and the winning team's name, in the big font. */
void ShootoutWonBy(void)
{
    printz(str_ShootoutWonBy_1, sizeof str_ShootoutWonBy_1);
    Framer(0x1E, 6);
    printbigz(str_ShootoutWonBy_2, sizeof str_ShootoutWonBy_2);
    g.printy += 2;
    uint16_t team = (int16_t)g.homeshootgoals > (int16_t)g.awayshootgoals ? g.HomeTeam : g.VisTeam;
    uint32_t data = rd32(rom_ptr(LG(TeamList) + (int16_t)(team << 2)));
    g.printx = 2;
    printbig(data + (int16_t)rd16(rom_ptr(data + 4)));
}

/* shootout94.asm:InitShooters. The six starters of the home team (shootoutteam 0: the list
 * below homeshootnum) or the away team (below shootoutteam), roster index - 1 each. */
void InitShooters(void)
{
    uint16_t team = g.HomeTeam, at = RAM_OFF_homeshootnum;
    if (g.shootoutteam) { team = g.VisTeam; at = RAM_OFF_shootoutteam; }
    uint32_t data = rd32(rom_ptr(LG(TeamList) + (uint16_t)(team << 2)));
    const uint8_t *a1 = rom_ptr(data + (int16_t)rd16(rom_ptr(data + 6)));
    for (int i = 0; i < 6; i++) {
        at -= 2;
        wr16(ram_ptr(RAM_BASE + at), (uint16_t)(*a1++ - 1));
    }
}

/* shootout94.asm:ClearShootout. Clear the player structs ($800 bytes from SortCords) and the
 * shootout state: round 1, no goals, both shooter lists. */
void ClearShootout(void)
{
    memset(ram_ptr(RAM_BASE + RAM_OFF_SortCords), 0, 0x800);
    g.shootoutdelay = 0;
    g.shootoutround = 1;
    g.gmode2 &= (uint16_t)~HI(3);
    g.homeshootgoals = 0;
    g.awayshootgoals = 0;
    g.homeshootnum = 0;
    g.shootoutteam = 0;
    InitShooters();
    g.shootoutteam = 1;
    InitShooters();
    g.shootoutteam = 0;
}

/* ---- the Shootout Shooters menu item ------------------------------------------------------------- */

/* the shooter list of team a2 (6 words: 5 shooters, then the goalie) */
static uint8_t *shooters(const uint8_t *tm) { return tm == team_struct(0) ? g.homeshooters : g.awayshooters; }

/* shootout94.asm:ShootoutShooters. Shootout Shooters for team a2 (a menu item, in a shootout
 * only): the 5 shooter slots and the goalie's; up / down a shooter slot, left / right the first
 * shooter or the goalie, C a player for the slot from a list (ShooterSelectList), Start leaves. */
void ShootoutShooters(uint8_t *tm)
{
    if (!(g.gmode2 & HI(0))) { ExitStatsScreen(); return; }
    SetupScreen(0, 0x1C);
    g.DispAttribCtr = 0;
    g.PlayerScrollCtr = 0;
    printsmallz(str_ShootoutShooters_1, sizeof str_ShootoutShooters_1);
    eraser(0x28, 0x1C, 0x7FF);
    ResetShooterScroll();
    PrintShooterSlots();
    g.TestList[0] = 0;
    for (;;) {
        ShootersBackground(tm);                           /* ShootersRedraw */
        PrintShooterNames(tm);
        PrintShooterBox(tm);
        for (;;) {                                        /* ShootersLoop */
            vcountwait();
            uint16_t d0, d1, d2, d3;
            getpzjoy(&d0, &d1, &d2, &d3);
            ProcessInputWithRepeat(&d1, d2, d3);
            if (d1 & 0x80) { ExitStatsScreen(); return; }
            int16_t to;
            if (d1 & 3) {                                 /* ShootersMove */
                if (g.TestList[0] == 5) continue;
                to = (int16_t)(((d1 & 2) ? 1 : -1) + (int16_t)g.TestList[0]);
                if (to < 0) to = 4;
                else if (to >= 5) to = 0;
            } else if (d1 & 0xC) {                        /* ShootersPick */
                to = (d1 & 4) ? 0 : 5;
                if (to == (int16_t)g.TestList[0]) continue;
            } else if (d1 & 0x20) {
                ShooterSelectList(tm);
                break;
            } else {
                continue;
            }
            g.TestList[0] = (uint16_t)to;                 /* ShootersSelect */
            PrintShooterNames(tm);
            PrintShooterBox(tm);
        }
    }
}

/* shootout94.asm:ShooterSelectList. C on slot TestList: "{Select Player}", the team's skaters
 * (the goalies for the goalie slot) in Satt, the slot's player highlighted ($C000); up / down
 * the player, left / right the attribute column, C takes him, Start goes back. */
void ShooterSelectList(uint8_t *tm)
{
    uint8_t *list = shooters(tm);
    uint16_t cur = rd16(list + (int16_t)(uint16_t)(g.TestList[0] << 1));
    uint16_t first = GetGoalieCount(tm);
    uint16_t n = (uint16_t)(GetPlayerCount(tm) - first - 1);
    if (g.TestList[0] == 5) {
        n = (uint16_t)(first - 1);
        first = 0;
    }
    g.screentimer = n;
    g.PlayerScrollCtr = 0;
    g.VertLineScrolling = 0;
    uint8_t *satt = RAM_W(RAM_OFF_Satt);
    uint16_t p = first, i = 0;
    do {
        satt[(int16_t)i] = (uint8_t)p;
        if (p == cur) g.VertLineScrolling = i;
        p++;
        i++;
    } while (n-- != 0);
    ClearShooterScreen();
    printsmallz(str_ShooterSelectList_1, sizeof str_ShooterSelectList_1);
    Framer(0x12, 3);
    printsmallz(str_ShooterSelectList_2, sizeof str_ShooterSelectList_2);
    Framer(0x12, 3);
    printsmallz(str_ShooterSelectList_3, sizeof str_ShooterSelectList_3);
    int16_t by = 0;
    int scroll = 1;
    for (;;) {
        if (scroll) {                                     /* .scroll */
            int16_t to = (int16_t)(by + (int16_t)g.VertLineScrolling);
            if (to >= 0 && to <= (int16_t)g.screentimer) {
                g.VertLineScrolling = (uint16_t)to;
                if (to <= (int16_t)g.PlayerScrollCtr) g.PlayerScrollCtr = (uint16_t)to;
                to = (int16_t)(to - 5);
                if (to > (int16_t)g.PlayerScrollCtr) g.PlayerScrollCtr = (uint16_t)to;
                PrintShooterList(tm);
            }
        } else {                                          /* .page */
            int16_t to = (int16_t)(by + (int16_t)g.DispAttribCtr);
            if (to >= 0) {
                g.DispAttribCtr = (uint16_t)to;
                PrintShooterList(tm);
            }
        }
        for (;;) {
            vcountwait();
            uint16_t d0, d1, d2, d3;
            getpzjoy(&d0, &d1, &d2, &d3);
            ProcessInputWithRepeat(&d1, d2, d3);
            if (d1 & 0x80) return;
            if (d1 & 0x20) {                              /* .pick */
                wr16(list + (int16_t)(uint16_t)(g.TestList[0] << 1), satt[(int16_t)g.VertLineScrolling]);
                return;
            }
            if (d1 & 2) { by = 1; scroll = 1; break; }
            if (d1 & 8) { by = 1; scroll = 0; break; }
            if (d1 & 1) { by = -1; scroll = 1; break; }
            if (d1 & 4) { by = -1; scroll = 0; break; }
        }
    }
}

/* the attribute column DispAttribCtr of a column list (each a String and a long): past the end
 * DispAttribCtr goes back one */
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

/* shootout94.asm:PrintShooterList. The column header (the overall column with the shooters' own
 * mask: ShooterOverallTxt / ShooterOverallTxt2), then six rows of Satt from PlayerScrollCtr: name
 * and attribute (GetNameandAttrib), row VertLineScrolling highlighted. */
void PrintShooterList(uint8_t *tm)
{
    printz(str_PrintShooterList_1, sizeof str_PrintShooterList_1);
    uint32_t base = g.TestList[0] == 5 ? SYM_GAttribOverall : SYM_PAttribOverall;
    uint32_t a1 = attrib_column(base);
    if (a1 == SYM_PAttribOverall) a1 = SYM_ShooterOverallTxt;
    if (a1 == SYM_GAttribOverall) a1 = SYM_ShooterOverallTxt2;
    a1 = print(a1);
    uint32_t sel = rd32(rom_ptr(a1));
    const uint8_t *satt = RAM_W(RAM_OFF_Satt);
    uint16_t row = g.PlayerScrollCtr, rows = (uint16_t)(g.screentimer - row);
    if (rows > 5) rows = 5;
    g.printy = 2;
    do {
        printsmallz(str_PrintShooterList_2, sizeof str_PrintShooterList_2);
        if (row == g.VertLineScrolling) g.printa = 0xC000;
        GetNameandAttrib(satt[(int16_t)row], sel, tm);
        row++;
    } while (rows-- != 0);
}

/* shootout94.asm:ShootersBackground. The screen cleared (ClearShooterScreen), the background
 * (ScoutMap, no tiles), the "Shootout" title box and team a2's block. */
void ShootersBackground(uint8_t *tm)
{
    ClearShooterScreen();
    printz(str_ShootersBackground_1, sizeof str_ShootersBackground_1);
    uint32_t map = SYM_ScoutMap + rd32(rom_ptr(SYM_ScoutMap + 4));
    const uint8_t *m = rom_ptr(map);
    dobitmap(0, map, SYM_NoTiles, 0, 0, rd16(m), rd16(m + 2), 1, 0);
    printz(str_ShootersBackground_2, sizeof str_ShootersBackground_2);
    Framer(0x1A, 6);
    printbigz(str_ShootersBackground_3, sizeof str_ShootersBackground_3);
    PutTeamBlock(tm == team_struct(0) ? 0 : 0x2C);
}

/* shootout94.asm:ClearShooterScreen. Plane B's top $A rows erased. */
void ClearShooterScreen(void)
{
    printz(str_ClearShooterScreen_1, sizeof str_ClearShooterScreen_1);
    eraser(0x28, 0xA, 0x7FF);
}

/* shootout94.asm:ResetShooterScroll. The list scroll words (palfadenew + $5A, + $7A) cleared. */
void ResetShooterScroll(void)
{
    g.palfadenew[0x2D] = 0;
    g.palfadenew[0x3D] = 0;
}

/* shootout94.asm:PrintShooterSlots. "Shooters", slots 1. to 5. and "Goalie". */
void PrintShooterSlots(void)
{
    printsmallz(str_PrintShooterSlots_1, sizeof str_PrintShooterSlots_1);
    printsmallz(str_PrintShooterSlots_2, sizeof str_PrintShooterSlots_2);
    printsmallz(str_PrintShooterSlots_3, sizeof str_PrintShooterSlots_3);
    printsmallz(str_PrintShooterSlots_4, sizeof str_PrintShooterSlots_4);
    printsmallz(str_PrintShooterSlots_5, sizeof str_PrintShooterSlots_5);
    printsmallz(str_PrintShooterSlots_6, sizeof str_PrintShooterSlots_6);
    printsmallz(str_PrintShooterSlots_7, sizeof str_PrintShooterSlots_7);
    printsmallz(str_PrintShooterSlots_8, sizeof str_PrintShooterSlots_8);
    printsmallz(str_PrintShooterSlots_9, sizeof str_PrintShooterSlots_9);
}

/* shootout94.asm:PrintShooterBox. The box with the full name of slot TestList's player (getname),
 * centred on column $15. */
void PrintShooterBox(uint8_t *tm)
{
    printsmallz(str_PrintShooterBox_1, sizeof str_PrintShooterBox_1);
    Framer(0x18, 3);
    printsmallz(str_PrintShooterBox_2, sizeof str_PrintShooterBox_2);
    uint16_t p = rd16(shooters(tm) + (int16_t)(uint16_t)(g.TestList[0] << 1));
    uint32_t a1 = getname(p, tm);
    g.printx -= (uint16_t)(rd16(mem_ptr(a1)) >> 1);
    printsmall(a1);
    g.printfontset = 0;
}

/* shootout94.asm:PrintShooterNames. The six slots' last names (FormatPlayerNameShort) from row
 * $E, two rows apart, the goalie's at ($19, $E); slot TestList in font set 2. */
void PrintShooterNames(uint8_t *tm)
{
    printsmallz(str_PrintShooterNames_1, sizeof str_PrintShooterNames_1);
    const uint8_t *list = shooters(tm);
    for (uint16_t n = 0; n < 6; n++) {
        uint32_t a1 = FormatPlayerNameShort(rd16(list + n * 2), tm);
        uint16_t x = g.printx;
        if (n == g.TestList[0]) g.printfontset = 2;
        if (n == 5) {
            g.printx = 0x19;
            g.printy = 0xE;
        }
        printsmall(a1);
        g.printfontset = 0;
        g.printx = x;
        g.printy += 2;
    }
}
