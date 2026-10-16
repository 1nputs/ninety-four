/* data94.asm: the boxes shown over the rink during play (goal, penalty shot, injury, the three
 * stars) and the player name formatting they use. The front-end parts of data94 (playoffs,
 * passwords) come with the front end. */
#include "game.h"
#include "hal.h"
#include "rom.h"
#include "symbols_gen.h"
#include "data_strings_gen.h"

#define RAM_W(off) ram_ptr(RAM_BASE + (off))
#define PRINTZ(s) printz(s, sizeof s)

/* the goal entry of the last goal: ChkCnt + ScoreSumbytes (byte 2 bit 7 away, 3 scorer, 4 / 5
 * assists, negative when none) */
static const uint8_t *last_goal(void) { return RAM_W(RAM_OFF_ChkCnt + g.ScoreSumbytes); }

/* data94.asm:box. Clear a $13 x 8 rectangle at x $B, y 2 of map 1. */
void box(void)
{
    PRINTZ(str_box_1);
    eraser(0x13, 8, 0x7FF);
}

/* data94.asm:ShowGoalBox. The goal box: close both line change boxes, frame it (more rows per
 * assist), "GOAL!" ("PP GOAL!" after a delayed penalty, "HAT TRICK!" on a skater's third goal,
 * which starts the home crowd animation for the home team), the scorer with his goals and the
 * assists with theirs. */
void ShowGoalBox(void)
{
    if (g.BA_PS_flags & HI(2)) g.sflags2 |= HI(2);
    lcfound2(team_struct(0));
    lcfound2(team_struct(1));
    const uint8_t *e = last_goal();
    PRINTZ(str_ShowGoalBox_1);
    uint16_t rows = 5;
    if (!(e[4] & 0x80)) {
        rows += 2;
        if (!(e[5] & 0x80)) rows++;
    }
    Framer(0x13, rows);
    CountGoalies();
    uint8_t *tm = team_struct(0);
    g.scorergoalies = g.homegoalies;
    if (e[2] & 0x80) { tm = team_struct(1); g.scorergoalies = g.awaygoalies; }
    uint32_t text = SYM_GoalBigTxt;
    if (g.DelayedPen & 1) { g.DelayedPen &= (uint8_t)~1; text = SYM_PPGoalBigTxt; }
    if (tm[0xB4 + e[3]] == 3 && e[3] >= g.scorergoalies) {      /* third goal of a skater */
        text += rd16(rom_ptr(text));                               /* HAT TRICK! */
        if (rd16(tm + 0x28) == g.HomeTeam) StartArenaAnim(0);
    }
    printbig(text);
    uint16_t scorer = e[3];
    g.printx = 0xC;
    print(FormatPlayerName(scorer, tm));
    if (scorer >= g.scorergoalies) PrintPlayerGoals(scorer, tm);
    if (!(e[4] & 0x80)) {
        int none = (g.sflags4 & HI(5)) != 0;
        g.sflags4 &= (uint16_t)~HI(5);
        if (!none) {
            PRINTZ(str_ShowGoalBox_2);
            print(FormatPlayerName(e[4], tm));
            PrintPlayerAssists(e[4], tm);
            if (!(e[5] & 0x80)) {
                PRINTZ(str_ShowGoalBox_3);
                print(FormatPlayerName(e[5], tm));
                PrintPlayerAssists(e[5], tm);
            }
        }
    }
    g.sflags4 &= (uint16_t)~HI(5);
}

/* data94.asm:PenaltyShotBox. Penalty shot box (gmode2 bit 7): "PENALTY SHOT!", the shooter, the
 * penalty and who committed it. In a shootout the shootout box instead. */
void PenaltyShotBox(void)
{
    g.gmode2 |= HI(7);
    if (g.gmode2 & HI(0)) { ShootoutModeBox(); return; }
    prefmes(-1);
    PRINTZ(str_PenaltyShotBox_1);
    Framer(0x1B, 8);
    printbig(SYM_PenShotBigTxt);
    uint16_t shooter = sortobj((int16_t)g.BA_Sktr_SCnum)->pnum;
    print(FormatPlayerNumInitial(shooter, team_struct(g.BA_Team != 0)));
    PRINTZ(str_PenaltyShotBox_2);
    const uint8_t *list = rom_ptr(SYM_Penaltylist);
    print(SYM_Penaltylist + (int16_t)rd16(list + (int16_t)g.pspenalty) + 2);
    PRINTZ(str_PenaltyShotBox_3);
    print(FormatPlayerNumInitial(g.BA_Checker_Offset, team_struct(g.BA_Team == 0)));
}

/* data94.asm:ShowThreeStars. Game over, once the referee animation is far enough (RefCnt <= $40)
 * and once only (gmode bit 7): the "Stars of the Game" box with the three best star scores, and
 * the home win song. */
void ShowThreeStars(void)
{
    if ((int16_t)g.RefCnt > 0x40) return;
    if (g.gmode & 0x80) return;
    g.gmode |= 0x80;
    g.disflags |= HI(dfclock);
    uint16_t v = g.vcount;
    while (g.vcount == v) hal_wait_vblank();
    PRINTZ(str_ShowThreeStars_1);
    Framer(0x1C, 9);
    PRINTZ(str_ShowThreeStars_2);
    uint32_t map = SYM_Rinktilelist + rd32(rom_ptr(SYM_Rinktilelist + 4));
    dobitmap(0, map, SYM_NoTiles, 0xD, 0x5B, 6, 4, g.rinkvrcset, 0);
    CalcStarScores(6);                                   /* d2 still 6 from dobitmap */
    g.printy = 0x12;
    for (int i = 0; i < 3; i++) {
        uint8_t *tm;
        uint16_t player = TakeTopStarScore(&tm);
        g.printx = 0x1A;
        g.printy++;
        uint32_t data = rd32(tm + tmdata);
        uint32_t abbr = data + (int16_t)rd16(rom_ptr(data + 4));
        abbr += (int16_t)rd16(rom_ptr(abbr));
        print(abbr);
        uint32_t name = getname(player, tm);
        g.printx = 3;
        print(name);
    }
    if ((int16_t)(rd16(team_struct(0) + tmscore) - rd16(team_struct(1) + tmscore)) > 0) song(SYM_SFXhomewin);
}

/* data94.asm:CalcStarScores. The star score of every roster slot of both teams to ThreeStars
 * (26 longs each, home first); goalies need d5 = period time + d2 on the ice. In overtime the
 * scorer of the winning goal is first star. */
void CalcStarScores(uint16_t d2)
{
    uint16_t mintime = (uint16_t)(GetPeriodTime() + d2);
    uint8_t *out = RAM_W(RAM_OFF_ThreeStars);
    CalcTeamStarScores(team_struct(0), team_struct(1), &out, mintime);
    int16_t diff = CalcTeamStarScores(team_struct(1), team_struct(0), &out, mintime);
    if (g.gsp != 3 || diff == 0) return;
    const uint8_t *e = last_goal();
    uint8_t slot = (uint8_t)((e[2] & 0x80 ? SYM_RosterSlots : 0) + e[3]);
    wr32(RAM_W(RAM_OFF_ThreeStars) + (uint16_t)(slot << 2), 0x7FFFFFFF);
}

/* data94.asm:CalcTeamStarScores. Team a2 against a3: a long per roster slot to (a4)+, each from
 * the goal difference. Skaters: goals * 11000 + assists * 10100 + shots * 10, in a tied game
 * shots * 1000 + time on ice. Goalies on the ice at least d5 who faced shots: +32000 for at most
 * 4 goals per 100 shots, +75000 more for a shutout. -> d3 = the goal difference. */
int16_t CalcTeamStarScores(uint8_t *tm, const uint8_t *other, uint8_t **out, uint16_t mintime)
{
    int16_t diff = (int16_t)(rd16(tm + tmscore) - rd16(other + tmscore));
    uint16_t goalies = (uint16_t)(0x19 - GetGoalieCount(tm));
    const uint8_t *stats = tm, *time = tm;
    for (int16_t slot = 0x19; slot >= 0; slot--, stats++, time += 2, *out += 4) {
        uint32_t score = (uint32_t)(int32_t)diff;
        if ((uint16_t)slot > goalies) {                  /* goalie slot */
            if (mintime > rd16(time + 0x136)) { wr32(*out, score); continue; }
            uint16_t shots = stats[0xE8];
            if (shots == 0) { wr32(*out, score); continue; }
            uint16_t pct = (uint16_t)m68k_divu((uint32_t)stats[0xB4] * 100, shots);
            if (pct <= 4) {
                score += 32000;
                if (pct == 0) score += 75000;
            }
            wr32(*out, score);
            continue;
        }
        score += stats[0xB4] * 11000u + stats[0xCE] * 10100u;
        uint32_t shots = stats[0xE8] * 10u;
        if (diff == 0) score += (uint16_t)shots * 100u + rd16(time + 0x136);
        else score += shots;
        wr32(*out, score);
    }
    return diff;
}

/* data94.asm:TakeTopStarScore. Take the highest of the 52 star scores (and clear it) -> d0 = the
 * player slot, a2 = his team. (When no score is above 0 the 68k clears through a1 as the caller
 * left it; the scores always include time on ice or shots, so the C takes slot 0.) */
uint16_t TakeTopStarScore(uint8_t **tm)
{
    uint8_t *s = RAM_W(RAM_OFF_ThreeStars), *best = s;
    int32_t top = 0;
    for (int i = 0; i < 0x34; i++, s += 4)
        if (!(top >= (int32_t)rd32(s))) { top = (int32_t)rd32(s); best = s; }
    wr32(best, 0);
    uint16_t n = (uint16_t)((best - RAM_W(RAM_OFF_ThreeStars)) >> 2);
    *tm = team_struct(0);
    if (n >= SYM_RosterSlots) { n -= SYM_RosterSlots; *tm = team_struct(1); }
    return n;
}

/* data94.asm:GetPlayerNamePointer. d0 = roster slot, a2 = team -> a0 = the player's name
 * String in the team data (each player: a String, then 8 bytes). */
uint32_t GetPlayerNamePointer(uint16_t player, const uint8_t *tm)
{
    uint32_t a = rd32(tm + tmdata);
    a += rd16(rom_ptr(a));
    while (player--) a += rd16(rom_ptr(a)) + 8;
    return a;
}

/* data94.asm:ConvertByteToDigits. d0 = BCD byte: two ASCII digits to (a1)+, a leading 0 as a
 * space. -> a1 */
uint8_t *ConvertByteToDigits(uint16_t bcd, uint8_t *dst)
{
    uint8_t hi = (uint8_t)((uint8_t)bcd >> 4);
    if (hi == 0) hi = 0xF0;
    *dst++ = (uint8_t)(hi + '0');
    *dst++ = (uint8_t)((bcd & 0xF) + '0');
    return dst;
}

/* data94.asm:FinalizeTextBuffer. a1 = the end of the text after mesarea's length word: set the
 * length (with a pad byte when odd) -> a1 = mesarea. */
uint32_t FinalizeTextBuffer(uint8_t *end)
{
    uint8_t *mes = RAM_W(RAM_OFF_mesarea);
    uint16_t len = (uint16_t)(end - mes);
    if (len & 1) { *end = 0; len++; }
    wr16(mes, len);
    return ram_addr(mes);
}

/* FormatPlayerName / FormatPlayerNumInitial: "NN " then the name from after the first space
 * (the first name), optionally after the first initial and ". " */
static uint32_t format_name(uint16_t player, const uint8_t *tm, int initial)
{
    uint32_t name = GetPlayerNamePointer(player, tm);
    const uint8_t *p = rom_ptr(name);
    uint8_t *out = RAM_W(RAM_OFF_TextBuffer);
    out = ConvertByteToDigits(p[rd16(p)], out);          /* the jersey number after the name */
    *out++ = ' ';
    const uint8_t *end = p + rd16(p);
    p += 2;
    if (initial) { *out++ = *p++; *out++ = '.'; *out++ = ' '; }
    while (*p++ != ' ') {}
    do *out++ = *p++; while (p != end);
    return FinalizeTextBuffer(out);
}

/* data94.asm:FormatPlayerName. d0 = roster slot, a2 = team -> a1 = mesarea "NN Last". */
uint32_t FormatPlayerName(uint16_t player, uint8_t *tm) { return format_name(player, tm, 0); }

/* data94.asm:FormatPlayerNumInitial. d0 = roster slot, a2 = team -> a1 = mesarea "NN F. Last". */
uint32_t FormatPlayerNumInitial(uint16_t player, uint8_t *tm) { return format_name(player, tm, 1); }

/* data94.asm:getname. d0 = roster slot, a2 = team -> a1 = mesarea "NN First Last" (the
 * number, then appendz / appstring). */
uint32_t getname(uint16_t player, const uint8_t *tm)
{
    uint32_t name = GetPlayerNamePointer(player, tm);
    const uint8_t *p = rom_ptr(name);
    uint8_t *mes = RAM_W(RAM_OFF_mesarea);
    uint32_t mes_addr = ram_addr(mes);
    wr16(mes, 4);
    ConvertByteToDigits(p[rd16(p)], mes + 2);
    appendz(str_getname_1, sizeof str_getname_1, mes_addr);
    appstring(name, mes_addr);
    return mes_addr;
}

/* the player of TempPlOffset (bit 15 set: the away team) */
static uint8_t *temp_player(uint16_t *player)
{
    uint16_t p = g.TempPlOffset;
    if ((int16_t)p >= 0) { *player = p; return team_struct(0); }
    *player = p & 0xFF;
    return team_struct(1);
}

/* data94.asm:GetPlayerName. -> a1 = mesarea "NN First Last" for player TempPlOffset. */
uint32_t GetPlayerName(void)
{
    uint16_t player;
    uint8_t *tm = temp_player(&player);
    return getname(player, tm);
}

/* data94.asm:GetPlayerNumInitial. -> a1 = mesarea "NN F. Last" for player TempPlOffset. */
uint32_t GetPlayerNumInitial(void)
{
    uint16_t player;
    uint8_t *tm = temp_player(&player);
    return FormatPlayerNumInitial(player, tm);
}

/* data94.asm:ShowInjuryBox. "Injury to:" player TempPlOffset, "Out for period" ("Out for game"
 * with sflags7 bit 5, "the game" after a fight). */
void ShowInjuryBox(void)
{
    PRINTZ(str_ShowInjuryBox_1);
    Framer(0x12, 5);
    if (g.sflags7 & 0x20) PRINTZ(str_ShowInjuryBox_3);
    else PRINTZ(str_ShowInjuryBox_2);
    print(GetPlayerNumInitial());
    if (sortobj(puckscnum)->pflags2 & (1 << pf2fight)) PRINTZ(str_ShowInjuryBox_4);
}

/* data94.asm:GetShifter. -> d1 = games in a playoff round - 1, d2 = its first bit of WinBits
 * (by gamelevel). */
uint16_t GetShifter(uint16_t *first_bit)
{
    uint16_t d1 = 0x10, d2 = (uint16_t)-0x10;
    uint16_t n = g.gamelevel;
    do { d2 += d1; d1 >>= 1; } while (n-- != 0);        /* dbf: gamelevel + 1 times */
    if (first_bit) *first_bit = d2;
    return (uint16_t)(d1 - 1);
}

/* the playoff stat bit stream runs down in words from outputbuffer + $DE: the long holding bit
 * pos of it */
static uint8_t *stat_long(uint16_t pos) { return RAM_W(RAM_OFF_outputbuffer + 0x100 - 0x22 - (uint16_t)((pos >> 4) << 1)); }

/* data94.asm:ReadTeamStats. Unpack the $68 playoff stat totals of the bit stream (widths from
 * BitWidthTable by stat & 3) into statsbuffer words. */
void ReadTeamStats(void)
{
    uint8_t *out = RAM_W(RAM_OFF_statsbuffer);
    const uint8_t *widths = rom_ptr(SYM_BitWidthTable);
    uint16_t pos = 0;
    for (int16_t n = 0x67; n >= 0; n--, out += 2) {
        uint32_t v = rd32(stat_long(pos)) >> (pos & 0xF);
        uint8_t w = widths[n & 3];
        pos += w;
        wr16(out, (uint16_t)(v & (uint16_t)((1u << (w & 31)) - 1)));
    }
}

/* data94.asm:AddPlayoffTeamStats. In the playoffs: add the playoff team's game stats (the $68
 * bytes from team struct + $B4) to its unpacked totals and pack them back, each clamped to its
 * bit width. */
void AddPlayoffTeamStats(void)
{
    if ((int16_t)g.OptPlayMode < 1) return;
    ReadTeamStats();
    uint16_t team = RAM_W(RAM_OFF_potree)[(int16_t)g.potreeteam];
    const uint8_t *tm = team_struct(0);
    if (rd16(tm + 0x28) != team) tm = team_struct(1);
    tm += 0xB4;
    uint8_t *tot = RAM_W(RAM_OFF_statsbuffer);
    for (int i = 0; i < 0x68; i++) wr16(tot + 2 * i, (uint16_t)(rd16(tot + 2 * i) + tm[i]));
    const uint8_t *widths = rom_ptr(SYM_BitWidthTable);
    uint16_t pos = 0;
    for (int16_t n = 0x67; n >= 0; n--, tot += 2) {
        uint32_t v = rd16(tot);
        uint8_t w = widths[n & 3];
        uint32_t max = (uint16_t)((1u << (w & 31)) - 1);
        if ((int16_t)v > (int16_t)max) v = max;
        uint32_t keep = ~max;
        unsigned r = pos & 0xF;
        keep = r ? (keep << r | keep >> (32 - r)) : keep;   /* rol.l */
        v = r ? (v << r | v >> (32 - r)) : v;
        uint8_t *p = stat_long(pos);
        wr32(p, (rd32(p) & keep) | v);
        pos += w;
    }
}

/* data94.asm:DefaultMenus. The options at power on (defmenuoptions: regular season, one
 * player, Montreal at Los Angeles, 10 minute periods ...); no demo has run yet. */
void DefaultMenus(void)
{
    ST_W(g.demoflag);
    for (int i = 0; i < 9; i++)
        wr16(ram_ptr(RAM_BASE + RAM_OFF_OptPlayMode) + 2 * i, rd16(rom_ptr(SYM_defmenuoptions) + 2 * i));
}
