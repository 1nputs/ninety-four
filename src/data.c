/* data94.asm: the boxes shown over the rink during play (goal, penalty shot, injury, the three
 * stars), the player name formatting, the playoff tree and its password. Not ported: the 68k's
 * exception handlers (AddError, Illinst, ZeroDiv, crash: the crash screen; the C cannot fault). */
#include <string.h>
#include "game.h"
#include "hal.h"
#include "rom.h"
#include "symbols_gen.h"
#include "data_strings_gen.h"

#define PRINTZ(s) printz(s, sizeof s)

/* the goal entry of the last goal: ChkCnt + ScoreSumbytes (byte 2 bit 7 away, 3 scorer, 4 / 5
 * assists, negative when none) */
static rp last_goal(void) { return rp_add(RAM(ChkCnt), g.ScoreSumbytes); }

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
    if (g.BA_PS_flags & HI(2)) g.sflags2 |= HI(sf2drec);
    lcfound2(team_struct(0));
    lcfound2(team_struct(1));
    rp e = last_goal();
    PRINTZ(str_ShowGoalBox_1);
    uint16_t rows = 5;
    if (!(r8(e, 4) & 0x80)) {
        rows += 2;
        if (!(r8(e, 5) & 0x80)) rows++;
    }
    Framer(0x13, rows);
    CountGoalies();
    struct Team *tm = team_struct(0);
    g.scorergoalies = g.homegoalies;
    if (r8(e, 2) & 0x80) { tm = team_struct(1); g.scorergoalies = g.awaygoalies; }
    uint32_t text = SYM_GoalBigTxt;
    if (g.DelayedPen & 1) { g.DelayedPen &= (uint8_t)~1; text = SYM_PPGoalBigTxt; }
    if (tm->pgoals[r8(e, 3)] == 3 && r8(e, 3) >= g.scorergoalies) {          /* third goal of a skater */
        text += rd16(rom_ptr(text));                               /* HAT TRICK! */
        if (tm->team == g.HomeTeam) StartArenaAnim(0);
    }
    printbig(text);
    uint16_t scorer = r8(e, 3);
    g.printx = 0xC;
    print(FormatPlayerName(scorer, tm));
    if (scorer >= g.scorergoalies) PrintPlayerGoals(scorer, tm);
    if (!(r8(e, 4) & 0x80)) {
        int none = (g.sflags4 & HI(5)) != 0;
        g.sflags4 &= (uint16_t)~HI(5);
        if (!none) {
            PRINTZ(str_ShowGoalBox_2);
            print(FormatPlayerName(r8(e, 4), tm));
            PrintPlayerAssists(r8(e, 4), tm);
            if (!(r8(e, 5) & 0x80)) {
                PRINTZ(str_ShowGoalBox_3);
                print(FormatPlayerName(r8(e, 5), tm));
                PrintPlayerAssists(r8(e, 5), tm);
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
    uint32_t map = ART(Rinktilelist) + rd32(rom_ptr(ART(Rinktilelist) + 4));
    dobitmap(0, map, SYM_NoTiles, 0xD, 0x5B, 6, 4, g.rinkvrcset, 0);
    CalcStarScores(6);                                   /* d2 still 6 from dobitmap */
    g.printy = 0x12;
    for (int i = 0; i < 3; i++) {
        struct Team *tm;
        uint16_t player = TakeTopStarScore(&tm);
        g.printx = 0x1A;
        g.printy++;
        uint32_t data = tm->data;
        uint32_t abbr = data + (int16_t)rd16(rom_ptr(data + 4));
        abbr += (int16_t)rd16(rom_ptr(abbr));
        print(abbr);
        uint32_t name = getname(player, tm);
        g.printx = 3;
        print(name);
    }
    if ((int16_t)(team_struct(0)->score - team_struct(1)->score) > 0) song(SYM_SFXhomewin);
}

/* the star scores: a long per roster slot of both teams from ThreeStars (52, past it into
 * teamrecbuf), big-endian as the 68000 keeps them in that scratch buffer */
static uint8_t *star_scores(void) { return ram_bytes(RAM(ThreeStars), 0x34 * 4); }

/* data94.asm:CalcStarScores. The star score of every roster slot of both teams to ThreeStars
 * (26 longs each, home first); goalies need d5 = period time + d2 on the ice. In overtime the
 * scorer of the winning goal is first star. */
void CalcStarScores(uint16_t d2)
{
    uint16_t mintime = (uint16_t)(GetPeriodTime() + d2);
    uint8_t *out = star_scores();
    CalcTeamStarScores(team_struct(0), team_struct(1), &out, mintime);
    int16_t diff = CalcTeamStarScores(team_struct(1), team_struct(0), &out, mintime);
    if (g.gsp != 3 || diff == 0) return;
    rp e = last_goal();
    uint8_t slot = (uint8_t)((r8(e, 2) & 0x80 ? SYM_RosterSlots : 0) + r8(e, 3));
    wr32(star_scores() + (uint16_t)(slot << 2), 0x7FFFFFFF);
}

/* data94.asm:CalcTeamStarScores. Team a2 against a3: a long per roster slot to (a4)+, each from
 * the goal difference. Skaters: goals * 11000 + assists * 10100 + shots * 10, in a tied game
 * shots * 1000 + time on ice. Goalies on the ice at least d5 who faced shots: +32000 for at most
 * 4 goals per 100 shots, +75000 more for a shutout. -> d3 = the goal difference. */
int16_t CalcTeamStarScores(struct Team *tm, struct Team *other, uint8_t **out, uint16_t mintime)
{
    int16_t diff = (int16_t)(tm->score - other->score);
    uint16_t goalies = (uint16_t)(0x19 - GetGoalieCount(tm));
    for (int16_t slot = 0x19, k = 0; slot >= 0; slot--, k++, *out += 4) {                /* (k: the roster slot) */
        uint32_t score = (uint32_t)(int32_t)diff;
        if ((uint16_t)slot > goalies) {                  /* goalie slot */
            if (mintime > tm->ptoi[k]) { wr32(*out, score); continue; }
            uint16_t shots = tm->pshots[k];
            if (shots == 0) { wr32(*out, score); continue; }
            uint16_t pct = (uint16_t)m68k_divu((uint32_t)tm->pgoals[k] * 100, shots);
            if (pct <= 4) {
                score += 32000;
                if (pct == 0) score += 75000;
            }
            wr32(*out, score);
            continue;
        }
        score += tm->pgoals[k] * 11000u + tm->passists[k] * 10100u;
        uint32_t shots = tm->pshots[k] * 10u;
        if (diff == 0) score += (uint16_t)shots * 100u + tm->ptoi[k];
        else score += shots;
        wr32(*out, score);
    }
    return diff;
}

/* data94.asm:TakeTopStarScore. Take the highest of the 52 star scores (and clear it) -> d0 = the
 * player slot, a2 = his team. (When no score is above 0 the 68k clears through a1 as the caller
 * left it; the scores always include time on ice or shots, so the C takes slot 0.) */
uint16_t TakeTopStarScore(struct Team **tm)
{
    uint8_t *s0 = star_scores(), *s = s0, *best = s;
    int32_t top = 0;
    for (int i = 0; i < 0x34; i++, s += 4)
        if (!(top >= (int32_t)rd32(s))) { top = (int32_t)rd32(s); best = s; }
    wr32(best, 0);
    uint16_t n = (uint16_t)((best - s0) >> 2);
    *tm = team_struct(0);
    if (n >= SYM_RosterSlots) { n -= SYM_RosterSlots; *tm = team_struct(1); }
    return n;
}

/* data94.asm:GetPlayerNamePointer. d0 = roster slot, a2 = team -> a0 = the player's name
 * String in the team data (each player: a String, then 8 bytes). */
uint32_t GetPlayerNamePointer(uint16_t player, struct Team *tm)
{
    uint32_t a = tm->data;
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
    uint8_t *mes = g.mesarea;
    uint16_t len = (uint16_t)(end - mes);
    if (len & 1) { *end = 0; len++; }
    wr16(mes, len);
    return ram_addr(mes);
}

/* FormatPlayerName / FormatPlayerNumInitial: "NN " then the name from after the first space
 * (the first name), optionally after the first initial and ". " */
static uint32_t format_name(uint16_t player, struct Team *tm, int initial)
{
    uint32_t name = GetPlayerNamePointer(player, tm);
    const uint8_t *p = rom_ptr(name);
    uint8_t *out = text_buffer();
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
uint32_t FormatPlayerName(uint16_t player, struct Team *tm) { return format_name(player, tm, 0); }

/* data94.asm:FormatPlayerNumInitial. d0 = roster slot, a2 = team -> a1 = mesarea "NN F. Last". */
uint32_t FormatPlayerNumInitial(uint16_t player, struct Team *tm) { return format_name(player, tm, 1); }

/* data94.asm:FormatLastName. Name string a0 (a length word String): from after the first space
 * (the first name) to its end or a 0 byte, to a1, space padded to TextBuffer + 12 -> a1 =
 * mesarea (FinalizeTextBuffer). */
uint32_t FormatLastName(uint32_t name, uint8_t *out)
{
    const uint8_t *p = rom_ptr(name);
    const uint8_t *end = p + rd16(p);
    p += 2;
    while (*p++ != ' ') {}
    do *out++ = *p++; while (p != end && *p != 0);
    for (const uint8_t *lim = text_buffer() + 12; out < lim;) *out++ = ' ';
    return FinalizeTextBuffer(out);
}

/* data94.asm:FormatPlayerNameShort. d0 = roster slot, a2 = team -> a1 = mesarea " Last". */
uint32_t FormatPlayerNameShort(uint16_t player, struct Team *tm)
{
    uint8_t *out = text_buffer();
    *out++ = ' ';
    return FormatLastName(GetPlayerNamePointer(player, tm), out);
}

/* data94.asm:FormatPlayerNameLast. d0 = roster slot, a2 = team -> a1 = mesarea "Last". */
uint32_t FormatPlayerNameLast(uint16_t player, struct Team *tm)
{
    return FormatLastName(GetPlayerNamePointer(player, tm), text_buffer());
}

/* data94.asm:getname. d0 = roster slot, a2 = team -> a1 = mesarea "NN First Last" (the
 * number, then appendz / appstring). */
uint32_t getname(uint16_t player, struct Team *tm)
{
    uint32_t name = GetPlayerNamePointer(player, tm);
    const uint8_t *p = rom_ptr(name);
    uint8_t *mes = g.mesarea;
    uint32_t mes_addr = ram_addr(mes);
    wr16(mes, 4);
    ConvertByteToDigits(p[rd16(p)], mes + 2);
    appendz(str_getname_1, sizeof str_getname_1, mes_addr);
    appstring(name, mes_addr);
    return mes_addr;
}

/* the player of TempPlOffset (bit 15 set: the away team) */
static struct Team *temp_player(uint16_t *player)
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
    struct Team *tm = temp_player(&player);
    return getname(player, tm);
}

/* data94.asm:GetPlayerNumInitial. -> a1 = mesarea "NN F. Last" for player TempPlOffset. */
uint32_t GetPlayerNumInitial(void)
{
    uint16_t player;
    struct Team *tm = temp_player(&player);
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
static rp stat_long(uint16_t pos) { return rp_add(RAM(outputbuffer), 0x100 - 0x22 - (uint16_t)((pos >> 4) << 1)); }

/* data94.asm:ReadTeamStats. Unpack the $68 playoff stat totals of the bit stream (widths from
 * BitWidthTable by stat & 3) into statsbuffer words. */
void ReadTeamStats(void)
{
    uint8_t *out = g.statsbuffer;
    const uint8_t *widths = rom_ptr(SYM_BitWidthTable);
    uint16_t pos = 0;
    for (int16_t n = 0x67; n >= 0; n--, out += 2) {
        uint32_t v = r32(stat_long(pos), 0) >> (pos & 0xF);
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
    uint16_t team = r8(RAM(potree), (int16_t)g.potreeteam);
    struct Team *tm = team_struct(0);
    if (tm->team != team) tm = team_struct(1);
    rp st = rp_add(team_place(tm), offsetof(struct Team, pgoals));   /* (goals, assists, shots, penalty minutes: four tables of 26) */
    uint8_t *tot = g.statsbuffer;
    for (int i = 0; i < 0x68; i++) wr16(tot + 2 * i, (uint16_t)(rd16(tot + 2 * i) + r8(st, i)));
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
        rp p = stat_long(pos);
        w32(p, 0, (r32(p, 0) & keep) | v);
        pos += w;
    }
}

/* data94.asm:DefaultMenus. The options at power on (defmenuoptions: regular season, one
 * player, Montreal at Los Angeles, 10 minute periods ...); no demo has run yet. */
void DefaultMenus(void)
{
    ST_W(g.demoflag);
    for (int i = 0; i < 9; i++)
        w16(RAM(OptPlayMode), 2 * i, rd16(rom_ptr(SYM_defmenuoptions) + 2 * i));
}

/* ---- the playoff tree (game setup) ---------------------------------------------------------- */


/* data94.asm:SuperDiv. Divide the 5-word number at a3 by d0 in place -> the remainder. */
uint16_t SuperDiv(rp bits, uint16_t div)
{
    uint32_t d2 = 0;
    for (int i = 0; i < 5; i++) {
        d2 = m68k_divu((d2 & 0xFFFF0000) | r16(bits, 2 * i), div);     /* the remainder rides in the high word */
        w16(bits, 2 * i, (uint16_t)d2);
    }
    return (uint16_t)(d2 >> 16);
}

/* data94.asm:ReadPassBits. The playoff state from the 5 saved words at a3 (kept): each game's
 * series wins (bit 2 of its flags: a series won), WinBits, pojoy (1 / 2 become 4 without
 * FourWayPlay), potreeteam, gamelevel, bosgames, postarts. */
void ReadPassBits(rp bits)
{
    uint16_t saved[5];
    for (int i = 0; i < 5; i++) saved[i] = r16(bits, 2 * i);
    for (int n = 7; n >= 0; n--) {
        struct Matchup *gs = &g.matchups[n];
        gs->flags &= (uint8_t)~MU_SERIES_OVER;
        uint16_t w = SuperDiv(bits, 5);
        gs->botwins = w;
        if (w == 4) gs->flags |= MU_SERIES_OVER;
        w = SuperDiv(bits, 5);
        gs->topwins = w;
        if (w == 4) gs->flags |= MU_SERIES_OVER;
    }
    g.WinBits = SuperDiv(bits, 0x4000);
    g.pojoy = SuperDiv(bits, 8);
    if (!g.FourWayPlay && (g.pojoy == 1 || g.pojoy == 2)) g.pojoy = 4;
    g.potreeteam = SuperDiv(bits, 0x10);
    g.gamelevel = SuperDiv(bits, 4);
    g.bosgames = SuperDiv(bits, 8);
    g.postarts = SuperDiv(bits, 0x20);
    for (int i = 0; i < 5; i++) w16(bits, 2 * i, saved[i]);
}

/* data94.asm:GetRandomUnusedTeam. -> d0 = a random playoff team whose bit in d3 is clear; set it. */
uint16_t GetRandomUnusedTeam(uint32_t *used)
{
    for (;;) {
        uint16_t t = randomd0(league.playoff_teams);
        uint32_t bit = 1u << (t & 31);
        int was = (*used & bit) != 0;
        *used |= bit;
        if (!was) return t;
    }
}

/* data94.asm:InitializeGameStructures. Random team pairs for the 8 gsstruct games, none of
 * them HomeTeam or VisTeam. */
void InitializeGameStructures(void)
{
    ST_W(g.gamenum);
    uint32_t used = 1u << (g.HomeTeam & 31);
    used |= 1u << (g.VisTeam & 31);
    for (int n = 0; n < 8; n++) {
        g.matchups[n].team1 = GetRandomUnusedTeam(&used);
        g.matchups[n].team2 = GetRandomUnusedTeam(&used);
    }
}

/* data94.asm:FigureJoy. The teams of the pads (cont1team ... cont4team). In the playoffs (not
 * a shootout) find this round's game of the playoff team and take its teams and the pads from
 * .pojoylist (.pojoylist2 with FourWayPlay) by pojoy; otherwise from .noplist by OptNOP, and
 * random matchups for the other games (InitializeGameStructures), as after a demo. */
void FigureJoy(void)
{
    static const uint8_t pojoylist[8] = { 0, 1, 0, 0, 0, 1, 0, 1 };      /* its first two pairs */
    g.cont1team = g.cont2team = g.cont3team = g.cont4team = 0;
    if (g.demoflag != 0) {
        if (g.OptPlayMode != 0 && !(g.gmode2 & HI(0))) {
            uint16_t first, n = GetShifter(&first);       /* (games this round - 1) */
            uint16_t team = g.potree[g.potreeteam];
            struct Matchup *gs = &g.matchups[n];
            uint16_t d0;
            ST_W(g.gamenum);
            for (;;) {
                if (gs->team1 == team) {
                    d0 = 0;
                    g.Opt2Team = gs->team2;
                    g.Opt1Team = gs->team1;
                    break;
                }
                if (gs->team2 == team) {
                    d0 = g.FourWayPlay ? 5 : 4;
                    g.Opt2Team = gs->team1;
                    g.Opt1Team = gs->team2;
                    break;
                }
                if (n-- == 0) return;                     /* the playoff team is out */
                gs = &g.matchups[n];
            }
            d0 = (uint16_t)((d0 + g.pojoy) << 2);
            g.gamenum = n;
            g.HomeTeam = gs->team1;
            g.VisTeam = gs->team2;
            uint32_t list = rom_find(SYM_FigureJoy, SYM_InitializeGameStructures, pojoylist, 8);
            if (g.FourWayPlay) list += 0x20;              /* .pojoylist2 follows */
            g.cont1team = rd16(rom_ptr(list + d0));
            g.cont2team = rd16(rom_ptr(list + d0 + 2));
            if (g.FourWayPlay) {
                if (d0 == 4 || d0 == 0x18) {
                    g.cont3team = g.cont1team;
                    g.cont4team = g.cont2team;
                } else if (d0 == 8 || d0 == 0x1C) {
                    g.cont3team = g.cont1team;
                    g.cont4team = 0;
                }
            }
            return;
        }
        uint16_t d0 = (uint16_t)(g.OptNOP << 2);          /* (past OptNOP 6: the code after the table) */
        uint32_t list = SYM_InitializeGameStructures - 7 * 4;   /* .noplist, just before it */
        g.cont1team = rd16(rom_ptr(list + d0));
        g.cont2team = rd16(rom_ptr(list + d0 + 2));
        if (d0 == 0x14) {                                 /* Three */
            g.cont3team = g.cont1team;
            g.cont4team = 0;
        } else if (d0 == 0x18) {                          /* Four */
            g.cont3team = g.cont1team;
            g.cont4team = g.cont2team;
        }
    }
    InitializeGameStructures();
}

/* data94.asm:maketree. The playoff tree (potree): the first round's 16 teams from playoffseats
 * row postarts, each later round's winners by WinBits; then this round's games: scores
 * cleared, their teams (in tree order for series games 2, 3 and 5, else flipped), FigureJoy. */
void maketree(void)
{
    uint8_t *tree = g.potree;
    const uint8_t *seats = rom_ptr(LG(playoffseats)) + (uint16_t)(g.postarts << 4);
    memcpy(tree, seats, 16);
    uint8_t *a0 = tree, *a1 = tree + 16;
    uint16_t bits = g.WinBits;
    for (int i = 0; i < 15; i++, a0 += 2, bits >>= 1) *a1++ = a0[bits & 1];
    uint16_t first;
    int16_t n = (int16_t)GetShifter(&first);
    if (n < 0) return;                                    /* the playoffs are over */
    a0 = tree + (int16_t)(first * 2);
    for (struct Matchup *gs = g.matchups; n >= 0; n--, gs++) {
        gs->score1 = 0;
        gs->score2 = 0;
        gs->per = 0;
        gs->flags &= (uint8_t)~MU_HIGHLIGHT;
        if (g.bosgames == 2 || g.bosgames == 3 || g.bosgames == 5) {
            gs->flags &= (uint8_t)~MU_FLIPPED;
            gs->team1 = (uint16_t)((gs->team1 & 0xFF00) | *a0++);
            gs->team2 = (uint16_t)((gs->team2 & 0xFF00) | *a0++);
        } else {
            gs->flags |= MU_FLIPPED;
            gs->team2 = (uint16_t)((gs->team2 & 0xFF00) | *a0++);
            gs->team1 = (uint16_t)((gs->team1 & 0xFF00) | *a0++);
        }
    }
    FigureJoy();
}

/* data94.asm:NewPO. New playoffs: a random tree row (from the one after a random one) that holds
 * Team 1 (the all-stars count as the last playoff team), series wins cleared unless
 * best-of-one; falls into maketree. */
void NewPO(void)
{
    uint16_t row = randomd0(league.playoff_rows), d1;
    uint16_t team = g.Opt1Team > league.playoff_teams - 1 ? league.playoff_teams - 1 : g.Opt1Team;
    for (;;) {
        row = (row + 1) & (league.playoff_rows - 1);
        const uint8_t *a0 = rom_ptr(LG(playoffseats)) + (row << 4);
        for (d1 = SYM_PlayoffRowTeams - 1; *a0++ != (uint8_t)team; )   /* dbeq */
            if (d1-- == 0) break;
        if (d1 != 0xFFFF) break;
    }
    g.potreeteam = d1 ^ (SYM_PlayoffRowTeams - 1);
    g.postarts = row;
    g.gamelevel = 0;
    g.bosgames = 7;
    if (g.OptPlayMode != 2) {
        g.bosgames = 0;
        for (int n = 0; n < 8; n++) {
            g.matchups[n].topwins = 0;
            g.matchups[n].botwins = 0;
        }
    }
    maketree();
}

/* data94.asm:ContinuePlayoffs. The playoff state saved at pwddatabuffer (ReadPassBits); when a
 * game has been played, the tree (maketree) and OptNOP from pojoy (7 or $B - pojoy, pojoy
 * moved to the pad set-ups). */
void ContinuePlayoffs(void)
{
    ReadPassBits(RAM(pwddatabuffer));
    if ((g.gamelevel | g.bosgames) == 0) return;          /* nothing played yet */
    maketree();
    uint16_t d0 = g.FourWayPlay ? 0xB : 7;
    if (!g.FourWayPlay) {
        if ((int16_t)g.pojoy >= 2) g.pojoy -= 2;
    } else if (g.pojoy == 1) {
        g.pojoy = 3;
    } else if (g.pojoy == 2) {
        g.pojoy = 4;
    }
    g.OptNOP = (uint16_t)(d0 - g.pojoy);
}

/* ---- the playoff password (after a playoff game) --------------------------------------------- */

/* the long at p (two words of a 5-word number) plus v, the carry run up through at most n words
 * above it (dbcc: n counts down to -1) */
static void add_long_carry(rp p, uint32_t v, int16_t n)
{
    uint32_t old = r32(p, 0), sum = old + v;
    w32(p, 0, sum);
    int carry = sum < old;
    while (carry && --n != -1) {
        p = rp_add(p, -2);
        uint16_t w = (uint16_t)(r16(p, 0) + 1);
        w16(p, 0, w);
        carry = w == 0;
    }
}

/* data94.asm:ClrPassBits. Clear the 5 words at a3. */
void ClrPassBits(rp bits)
{
    for (int i = 0; i < 10; i++) w8(bits, i, 0);
}

/* data94.asm:SuperAdd. The 5-word number at a3 plus d0 (a long). */
void SuperAdd(rp bits, uint32_t v)
{
    add_long_carry(rp_add(bits, 6), v, 3);
}

/* data94.asm:SuperMult. The 5-word number at a3 times d0 (a word): each word's product added in
 * from the lowest (the highest's spills into the word below a3, which the game's numbers never
 * reach). */
void SuperMult(rp bits, uint16_t m)
{
    uint16_t w[5];
    for (int i = 0; i < 5; i++) { w[i] = r16(bits, 2 * i); w16(bits, 2 * i, 0); }
    for (int16_t i = 4; i >= 0; i--) add_long_carry(rp_add(bits, 2 * i - 2), (uint32_t)m * w[i], i);
}

/* data94.asm:PushBits. bits = bits x d1 + d0 (d1 the value's range, 2-$4000). */
void PushBits(rp bits, uint16_t v, uint16_t range)
{
    SuperMult(bits, range);
    SuperAdd(bits, v);
}

/* data94.asm:WritePassBits. The playoff state into the 5 words at a3 (as ReadPassBits reads it):
 * postarts, bosgames, gamelevel, potreeteam, pojoy, WinBits, then each game's series wins. */
void WritePassBits(rp bits)
{
    ClrPassBits(bits);
    PushBits(bits, g.postarts, 0x20);
    PushBits(bits, g.bosgames, 8);
    PushBits(bits, g.gamelevel, 4);
    PushBits(bits, g.potreeteam, 0x10);
    PushBits(bits, g.pojoy, 8);
    PushBits(bits, g.WinBits, 0x4000);
    for (int n = 0; n < 8; n++) {
        PushBits(bits, g.matchups[n].topwins, 5);
        PushBits(bits, g.matchups[n].botwins, 5);
    }
}

/* data94.asm:ResolveGames. After a playoff game: its score into its game struct, then this
 * round's results. Best of seven: a win to each unfinished series' winner; past game seven, or
 * when the user's series is over (the rest of the round decided at random, the bits before kept
 * at tpassbits, sf3alttree), the next round: WinBits from the series won, gamelevel + 1. Best of
 * one: WinBits from the scores (by the games' flipped flags). */
void ResolveGames(void)
{
    struct Matchup *gs = &g.matchups[g.gamenum];
    gs->score1 = g.teams[0].score;
    gs->score2 = g.teams[1].score;
    uint16_t first;
    int16_t n = (int16_t)GetShifter(&first);
    uint16_t won = 0;
    if (g.bosgames == 7) {                                /* best of one */
        for (; n >= 0; n--) {
            const struct Matchup *a0 = &g.matchups[n];
            if (!(a0->score1 > a0->score2)) won |= (uint16_t)(1u << n);
            if (a0->flags & MU_FLIPPED) won ^= (uint16_t)(1u << n);
        }
        goto winbits;
    }
    for (; n >= 0; n--) {                                 /* the series' wins */
        struct Matchup *a0 = &g.matchups[n];
        if (a0->topwins == 4 || a0->botwins == 4) continue;
        int bottom = (a0->flags & MU_FLIPPED) != 0;          /* (team1 the tree's bottom team) */
        if ((int16_t)(a0->score1 - a0->score2) < 0) bottom = !bottom;
        if (bottom) a0->botwins++; else a0->topwins++;
    }
    if (++g.bosgames != 7) {
        gs = &g.matchups[g.gamenum];
        if (gs->topwins != 4 && gs->botwins != 4) return;
        if ((int16_t)g.gamelevel < 3) {                   /* the user's series is over: the round's others */
            n = (int16_t)GetShifter(&first);
            for (; n >= 0; n--) {
                struct Matchup *a0 = &g.matchups[n];
                if ((uint16_t)n == g.gamenum) continue;
                while (a0->topwins != 4 && a0->botwins != 4) {
                    a0->topwins++;
                    if (randomd0(0xC8) & 1) {
                        a0->topwins--;
                        a0->botwins++;
                    }
                }
            }
            WritePassBits(RAM(tpassbits));
            g.sflags3 |= 1 << sf3alttree;
        }
    }
    g.bosgames = 0;                                       /* the next round */
    n = (int16_t)GetShifter(&first);
    for (; n >= 0; n--) {
        struct Matchup *a0 = &g.matchups[n];
        if (a0->topwins != 4) won |= (uint16_t)(1u << n);
        a0->topwins = 0;
        a0->botwins = 0;
    }
winbits:
    g.WinBits &= (uint16_t)((uint16_t)(1u << first) - 1);
    g.WinBits |= (uint16_t)(won << first);
    g.gamelevel++;
}

/* data94.asm:EncodePW. After a playoff game (not Demo, Regular Season or a shootout): the results
 * (ResolveGames), the tree, and the state saved (WritePassBits to pwddatabuffer, WriteLineData);
 * the playoffs over (or the user's team out): cleared, and the mode back to new playoffs. */
void EncodePW(void)
{
    if (g.OptNOP == 0 || g.OptPlayMode == 0 || g.OptPlayMode == 4) return;
    g.OptPlayMode = 1;                                    /* continue playoffs */
    g.TempOptPlayMode = 1;
    ResolveGames();
    maketree();
    if (g.gamelevel != 4 && (int16_t)g.gamenum >= 0) {
        WritePassBits(RAM(pwddatabuffer));
        WriteLineData();
        if (!(g.sflags3 & (1 << sf3alttree))) return;
        ReadPassBits(RAM(tpassbits));                          /* the bits before the round was settled */
        maketree();
        return;
    }
    ClrPassBits(RAM(pwddatabuffer));
    WriteLineData();
    g.OptPlayMode = 2;                                    /* new playoffs */
    g.TempOptPlayMode = 2;
    if (g.bosgames == 7) return;
    g.OptPlayMode = 3;                                    /* new playoffs, best of 7 */
    g.TempOptPlayMode = 3;
}
