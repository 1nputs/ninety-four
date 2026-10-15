/* shootout94.asm: the shootout. Only the parts the game play calls are here so far; setting a
 * shootout up comes with the front end. */
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
    uint32_t data = rd32(rom_ptr(SYM_TeamList + (int16_t)(team << 2)));
    g.printx = 2;
    printbig(data + (int16_t)rd16(rom_ptr(data + 4)));
}
