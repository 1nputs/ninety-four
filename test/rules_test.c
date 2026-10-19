/* The game rules (src/rules.h) on the game's own routines, with RAM set up by hand and a hardware
 * layer that does nothing (hal_null.c): Classic plays as the original, and each rule changes only
 * what it says. make rulestest (the ROM: NHL94_ROM, or the repository's). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "game.h"
#include "rom.h"
#include "rules.h"
#include "symbols_gen.h"
#include "native.h"

static int checks, fails;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

static void classic_rules(void) { memcpy(rules, rule_classic, sizeof rules); }

/* the period lengths: GetPeriodTime for each option, and the setup menu's range for it */
static void periods(void)
{
    static const uint16_t classic[4] = { 300, 600, 1200, 30 }, more[8] = { 300, 420, 600, 900, 1200, 120, 180, 30 };
    classic_rules();
    for (int i = 0; i < 4; i++) { g.OptPerlen = (uint16_t)i; CHECK(GetPeriodTime() == classic[i]); }
    rules[RULE_PERIOD_LENGTHS] = 1;
    for (int i = 0; i < 8; i++) { g.OptPerlen = (uint16_t)i; CHECK(GetPeriodTime() == more[i]); }
    g.OptPlayMode = 0;
    g.FourWayPlay = 0;
    rules[RULE_PERIOD_LENGTHS] = 0;                       /* the original: three, round from 20 to 5 */
    g.OptPerlen = 2; ChangeOption(8, 1); CHECK(g.OptPerlen == 0);
    g.OptPerlen = 0; ChangeOption(8, -1); CHECK(g.OptPerlen == 2);
    g.OptPerlen = 6; ChangeOption(8, 0); CHECK(g.OptPerlen == 0);   /* (one the rules allowed, back in range) */
    rules[RULE_PERIOD_LENGTHS] = 1;                       /* more: seven, up from 5 and round from 20 to 2 */
    g.OptPerlen = 0; ChangeOption(8, 1); CHECK(g.OptPerlen == 1 && GetPeriodTime() == 420);
    g.OptPerlen = 4; ChangeOption(8, 1); CHECK(g.OptPerlen == 5 && GetPeriodTime() == 120);
    g.OptPerlen = 6; ChangeOption(8, 1); CHECK(g.OptPerlen == 0 && GetPeriodTime() == 300);
    g.OptPerlen = 0; ChangeOption(8, -1); CHECK(g.OptPerlen == 6 && GetPeriodTime() == 180);
    classic_rules();
}

/* overtime: ResetClock in the regular season's overtime (its minutes: the original's 10, the
 * page's others and some from the file), and elsewhere the period's length */
static void overtime(void)
{
    static const uint8_t minutes[5] = { 10, 5, 3, 7, 1 };
    g.OptPerlen = 0;
    for (int v = 0; v < 5; v++) {
        rules[RULE_OVERTIME] = minutes[v];
        g.gsp = 3; g.OptPlayMode = 0;
        ResetClock();
        CHECK(g.gameclock == minutes[v] * 60 && g.PerTimeTotal == minutes[v] * 60);
        g.gsp = 3; g.OptPlayMode = 2;                     /* (playoff overtime: a period, as the original) */
        ResetClock();
        CHECK(g.gameclock == 300);
        g.gsp = 1; g.OptPlayMode = 0;
        ResetClock();
        CHECK(g.gameclock == 300);
    }
    classic_rules();
}

/* the weights in a check: the original's byte wraps when a light pad player (base $F0) checks a
 * heavy skater; in full, the heavier the checked player the better his odds, always */
static void weights(void)
{
    struct SortObj a2, a3;
    memset(&a2, 0, sizeof a2);
    memset(&a3, 0, sizeof a3);
    a3.weight = 0;
    a2.weight = 120;
    CHECK(CheckWeightOdds(0xF0, &a2, &a3) == (uint8_t)(0xF0 + 120));   /* 104: wrapped */
    rules[RULE_WEIGHT_FIX] = 1;
    CHECK(CheckWeightOdds(0xF0, &a2, &a3) == 0xF0 + 120);
    int up = 1, wraps = 0;
    for (int w = 1; w < 16; w++) {
        struct SortObj lo = a2, hi = a2;
        lo.weight = (uint8_t)(8 * (w - 1));
        hi.weight = (uint8_t)(8 * w);
        rules[RULE_WEIGHT_FIX] = 1;
        up &= CheckWeightOdds(0xF0, &hi, &a3) > CheckWeightOdds(0xF0, &lo, &a3);
        rules[RULE_WEIGHT_FIX] = 0;
        wraps |= CheckWeightOdds(0xF0, &hi, &a3) < CheckWeightOdds(0xF0, &lo, &a3);
    }
    CHECK(up && wraps);
    a3.weight = 60; a2.weight = 60;                       /* (a computer's check, base $78: no wrap, the same) */
    rules[RULE_WEIGHT_FIX] = 0;
    uint16_t c = CheckWeightOdds(0x78, &a2, &a3);
    rules[RULE_WEIGHT_FIX] = 1;
    CHECK(CheckWeightOdds(0x78, &a2, &a3) == c && c == 0x78);
    classic_rules();
}

/* the assists: a2touchpuck for Z, B, A, then B again (a give-and-go) before B scores: the
 * original keeps A only; the rule keeps A and Z */
static void touch(struct SortObj *pl, uint8_t pnum, struct SortObj *puck)
{
    pl->pnum = pnum;
    a2touchpuck(pl, puck);
}

static void assists(void)
{
    struct SortObj *pl = sortobj(1), *puck = sortobj(puckscnum);
    for (int fix = 0; fix < 2; fix++) {
        rules[RULE_ASSIST_FIX] = (uint8_t)fix;
        memset(&g, 0, sizeof g);
        struct Team *tm = team_struct(0);
        tm->lasttouch = 0xFF00;
        tm->assist1 = 0xFF00;
        tm->assist2 = 0xFF00;
        touch(pl, 5, puck);                               /* Z */
        touch(pl, 6, puck);                               /* B */
        touch(pl, 7, puck);                               /* A */
        touch(pl, 6, puck);                               /* B again */
        uint16_t last = tm->lasttouch, a1 = tm->assist1, a2 = tm->assist2;
        printf("  assists %s: last %04X, assists %04X %04X\n", fix ? "with the rule" : "classic", last, a1, a2);
        CHECK(last == 6 && a1 == 7);
        if (fix) CHECK(a2 == 5);                          /* Z keeps his */
        else CHECK((a2 >> 8) == 0xFF);                    /* none */
    }
    classic_rules();
}

/* plus/minus: the skaters on the ice for a goal (objects 0-11: position > 0, a roster number),
 * plus for the scorers, minus for the others; none for a power play goal or with the rule off */
static void plusminus(void)
{
    memset(&g, 0, sizeof g);
    rules_clear_stats();
    for (int i = 0; i < 12; i++) {                        /* six a team: a goalie (0) and five skaters */
        struct SortObj *o = sortobj(i);
        o->position = (int16_t)(i % 6);
        o->pnum = (uint8_t)(i % 6 + 2);
        o->pflags = (uint16_t)(i >= 6 ? 1 << pfteam : 0);
    }
    sortobj(5)->position = -1;                            /* (home's last skater off the ice: in the box) */
    rules_count_goal(0, 2);                               /* rule off: nothing */
    CHECK(rule_plusminus[0][3] == 0 && rule_plusminus[1][3] == 0);
    rules[RULE_MORE_STATS] = 1;
    rules_count_goal(0, 2);                               /* home scores at even strength */
    CHECK(rule_plusminus[0][3] == 1 && rule_plusminus[0][6] == 1 && rule_plusminus[1][3] == -1 && rule_plusminus[1][7] == -1);
    CHECK(rule_plusminus[0][2] == 0 && rule_plusminus[1][2] == 0);   /* the goalies */
    CHECK(rule_plusminus[0][7] == 0);                     /* the one in the box */
    rules_count_goal(1, 3);                               /* away on the power play: nothing */
    CHECK(rule_plusminus[0][3] == 1 && rule_plusminus[1][3] == -1);
    rules_count_goal(1, 1);                               /* away short handed: counts */
    CHECK(rule_plusminus[0][3] == 0 && rule_plusminus[1][3] == 0);
    rules_clear_stats();
    CHECK(rule_plusminus[1][4] == 0);
    classic_rules();
}

/* the Player Stats list with more stats: the two columns' values, and sorted by plus/minus every
 * player listed, negative ones too (the list's key cannot be negative: the sort adds $80) */
static void stats_list(void)
{
    memset(&g, 0, sizeof g);
    rules_clear_stats();
    struct Team *tm = team_struct(0);
    tm->data = rd32(rom_ptr(SYM_TeamList));       /* Anaheim's players */
    uint16_t players = GetPlayerCount(tm), goalies = GetGoalieCount(tm);
    rules[RULE_MORE_STATS] = 1;
    for (uint16_t p = goalies; p < players; p++) rule_plusminus[0][p] = (int8_t)((int)p % 7 - 3);   /* -3 ... 3 */
    tm->pchecks[goalies] = 9;
    CHECK(ReadStatColumn(goalies, 6, tm, 0) == 9 && (int16_t)ReadStatColumn((uint16_t)(goalies + 1), 5, tm, 0) == (int)(goalies + 1) % 7 - 3);
    CHECK(GetStatColumn(0, 5, tm, 0) == 0 && GetStatColumn(0, 6, tm, 0) == 0);   /* (goalies: none) */
    g.DispAttribCtr = 5;
    DrawStatsList(tm, 0);
    rp order = RAM(Satt);
    int listed = 0, sorted = 1;
    for (int i = 0; i < 32 && !(r8(order, i) & 0x80); i++) {
        listed++;
        if (i && (int16_t)GetStatColumn(r8(order, i), 5, tm, 0) > (int16_t)GetStatColumn(r8(order, i - 1), 5, tm, 0)) sorted = 0;
    }
    printf("  stats list: %d of %d players by plus/minus, the first %+d, the last %+d\n", listed, players,
           (int16_t)GetStatColumn(r8(order, 0), 5, tm, 0), (int16_t)GetStatColumn(r8(order, listed - 1), 5, tm, 0));
    CHECK(listed == players && sorted);
    classic_rules();
}

/* the play rules through the game's own routines: Classic as the original, each rule its way */
static struct SortObj *skater(void)
{
    memset(&g, 0, sizeof g);
    struct SortObj *o = sortobj(1);
    o->position = 1;
    o->pnum = 3;
    o->weight = 64;
    o->legstr = 8;
    o->legspd = 20;
    o->endurance = 6;
    o->facedir = 2;                                       /* (right) */
    team_struct(0)->pde[6 / 2] = 0x1000;             /* full energy */
    return o;
}

static void play_rules(void)
{
    classic_rules();
    CHECK(rules[RULE_SKATING] == 100 && rule_scale(RULE_SKATING, -7) == -7 && rule_scale(RULE_BURST, 12345) == 12345);
    /* skating: from a standstill a step's acceleration, 110 % faster; the top speed, held */
    int16_t v[2];
    for (int r = 0; r < 2; r++) {
        rules[RULE_SKATING] = r ? 110 : 100;
        struct SortObj *o = skater();
        elapsed = 1;
        playeracc(2, o);
        v[r] = o->Xvel;
    }
    printf("  skating: a step's speed %d, faster %d\n", v[0], v[1]);
    CHECK(v[0] > 0 && v[1] > v[0] && v[1] <= v[0] * 11 / 10 + 2);
    int16_t top[2];
    for (int r = 0; r < 2; r++) {                         /* push for 400 steps: the speed it stops at */
        rules[RULE_SKATING] = r ? 125 : 100;
        struct SortObj *o = skater();
        elapsed = 1;
        for (int k = 0; k < 400; k++) playeracc(2, o);
        top[r] = o->Xvel;
    }
    printf("  skating: top speed %d, fastest %d\n", top[0], top[1]);
    CHECK(top[1] > top[0] * 115 / 100 && top[1] < top[0] * 135 / 100);
    classic_rules();
    /* the burst: its push, and its cost with line changes on */
    int16_t push[2], left[2];
    for (int r = 0; r < 2; r++) {
        rules[RULE_BURST] = r ? 150 : 100;                /* stronger */
        rules[RULE_TIRING] = r ? 50 : 100;                /* less */
        struct SortObj *o = skater();
        burst(o);
        push[r] = o->Xvel;
        left[r] = getpde(o);
    }
    printf("  burst: push %d, stronger %d; energy left %d, less tiring %d\n", push[0], push[1], left[0], left[1]);
    CHECK(left[0] == 0x1000 - 0xCC && left[1] == 0x1000 - 0x66);   /* (the push: the energy left / 128, of a step 200) */
    CHECK(push[0] == ((0x1000 - 0xCC) >> 7) * 200 && push[1] == ((0x1000 - 0x66) >> 7) * 150 / 100 * 200);
    classic_rules();
    /* bench rest: +9 a tick, faster +13; the box rests only by the rule */
    for (int r = 0; r < 2; r++) {
        memset(&g, 0, sizeof g);
        rules[RULE_BENCH_REST] = r ? 150 : 100;
        rules[RULE_BOX_REST] = (uint8_t)r;
        struct Team *tm = team_struct(0);
        tm->pdst[0 / 2] = 0xFFFE;                    /* on the bench */
        tm->pdst[2 / 2] = 120;                       /* in the box, two minutes */
        tm->pdst[4 / 2] = 0xFFFF;                    /* on the ice */
        for (int k = 0; k < 3; k++) tm->pde[k] = 0x800;
        RestoreBenchEnergy();
        uint16_t bench = tm->pde[0], box = tm->pde[2 / 2], ice = tm->pde[4 / 2];
        CHECK(bench == 0x800 + (r ? 13 : 9) && box == (r ? 0x800 + 13 : 0x800) && ice == 0x800);
    }
    classic_rules();
    /* streaks: a third of the hot / cold value added to a rating (x 5, at most 30), half of it
     * milder, none off */
    static const int want[3] = { 4 * 5 + 4, 4 * 5 + 2, 4 * 5 };
    for (int r = 0; r < 3; r++) {
        rules[RULE_STREAKS] = (uint8_t)(100 - 50 * r);
        struct SortObj *o = skater();
        team_struct(0)->hotcold[o->pnum][0] = 12;
        CHECK(AttributeCalc(4, o) == want[r]);
    }
    classic_rules();
    /* penalties: all and none draw nothing; 50 % and 25 % draw as the original's one in 2 and 4,
     * and others near their share */
    g.seed = 12345;
    CHECK(rule_chance(100) && !rule_chance(0) && g.seed == 12345);
    for (int r = 0; r < 2; r++) {
        uint32_t seed = 0x2468ACE;
        int same = 1;
        for (int k = 0; k < 200; k++) {
            g.seed = seed;
            int kept = rule_chance(r ? 25 : 50);
            uint32_t after = g.seed;
            g.seed = seed;
            same &= kept == (randomd0(r ? 4 : 2) == 0) && g.seed == after;
            seed = g.seed;
        }
        CHECK(same);
    }
    int kept = 0;
    g.seed = 777;
    for (int k = 0; k < 10000; k++) kept += rule_chance(70);
    printf("  penalties: 70 %% kept %d of 10000\n", kept);
    CHECK(kept > 6800 && kept < 7200);
    /* the puck's slowing: the original's for 100 %; scaled, and never 0 below zero (the least
     * there is -1: 0 would become the 1 above zero's, pushing a puck moving back faster) */
    CHECK(rule_puck_slowing(-1) == -1 && rule_puck_slowing(3) == 3);
    rules[RULE_PUCK_SLIDE] = 67;
    CHECK(rule_puck_slowing(-1) == -1 && rule_puck_slowing(-3) == -2 && rule_puck_slowing(1) == 0 && rule_puck_slowing(3) == 2);
    rules[RULE_PUCK_SLIDE] = 150;
    CHECK(rule_puck_slowing(-1) == -1 && rule_puck_slowing(-2) == -3 && rule_puck_slowing(2) == 3);
    classic_rules();
}

/* the League page's overall ratings (native/ratings.c, reckoned without the game's RAM) as the
 * cards show them (CalcAttrib with the overall weights, no streak, x 100 / its divisor,
 * AttribAdjust): every player of the cartridge */
static void overall_ratings(void)
{
    int players = 0, same = 0;
    for (int t = 0; t < (int)SYM_NumTeams; t++) {
        memset(&g, 0, sizeof g);
        struct Team *tm = team_struct(0);
        memset(tm->hotcold, 0, sizeof tm->hotcold);               /* (no streaks) */
        tm->team = (uint16_t)t;                           /* (as InitTeamStructure makes it) */
        tm->data = lg((uint16_t)t)->data;
        uint16_t count = GetPlayerCount(tm), goalies = GetGoalieCount(tm);
        for (uint16_t p = 0; p < count; p++) {
            const struct LgPlayer *P = &lg((uint16_t)t)->player[p];   /* his number, then his 14 ratings */
            const uint8_t *nib = P->rating;
            int goalie = p < goalies, number = (P->number >> 4) * 10 + (P->number & 15);
            uint16_t div, v = CalcAttrib(p, tm, rd32(rom_ptr(goalie ? SYM_GAttribOverallMask : SYM_PAttribOverallMask)), &div);
            int want = AttribAdjust((uint16_t)m68k_divu((uint32_t)v * 100u, div));
            players++;
            same += rating_overall(rom_cart, nib, number, goalie, NULL) == want;
        }
    }
    printf("  overall ratings: %d of %d players as their cards'\n", same, players);
    CHECK(players > 600 && same == players);
}

/* the League page's other ratings (native/ratings.c's rating_column) as the roster screen shows
 * them (GetNameandAttrib: CalcAttrib with the column's mask, no streak; AttribRating's x 100 / its
 * divisor, AttribAdjust; AttribWeight's pounds): every player of the cartridge, every column of
 * PAttribColumns (skaters) or GAttribColumns (goalies) those show, the overall ratings aside */
static void column_ratings(void)
{
    int checked = 0, same = 0, columns = 0;
    for (int t = 0; t < (int)SYM_NumTeams; t++) {
        memset(&g, 0, sizeof g);
        struct Team *tm = team_struct(0);                 /* (no streaks) */
        tm->team = (uint16_t)t;
        tm->data = lg((uint16_t)t)->data;
        uint16_t count = GetPlayerCount(tm), goalies = GetGoalieCount(tm);
        for (uint16_t p = 0; p < count; p++) {
            const struct LgPlayer *P = &lg((uint16_t)t)->player[p];
            int goalie = p < goalies, number = (P->number >> 4) * 10 + (P->number & 15), cols = 0;
            uint32_t overall = rd32(rom_ptr(goalie ? SYM_GAttribOverallMask : SYM_PAttribOverallMask));
            for (uint32_t a = goalie ? SYM_GAttribColumns : SYM_PAttribColumns; (int16_t)rd16(rom_ptr(a)) >= 0; a += rd16(rom_ptr(a)) + 4u) {
                uint32_t sel = rd32(rom_ptr(a + rd16(rom_ptr(a))));
                uint32_t handler = SYM_attribjmp + (uint32_t)(int16_t)rd16(rom_ptr(SYM_attribjmp + (int16_t)(uint16_t)sel));
                if ((handler != SYM_AttribRating && handler != SYM_AttribWeight) || sel == overall || (uint16_t)sel <= 2) continue;
                uint16_t div, v = CalcAttrib(p, tm, sel, &div);
                int weight = handler == SYM_AttribWeight;
                int want = weight ? 140 + 8 * v : AttribAdjust((uint16_t)m68k_divu((uint32_t)v * 100u, div));
                int got = weight ? 140 + 8 * P->rating[LR_WEIGHT] : rating_column(rom_cart, P->rating, number, sel >> 16);
                checked++;
                cols++;
                same += got == want;
            }
            if (cols > columns) columns = cols;
        }
    }
    printf("  roster screen ratings: %d of %d as the screen shows them (up to %d columns a player)\n", same, checked, columns);
    CHECK(checked > 6000 && same == checked);
}

int main(void)
{
    const char *rom = getenv("NHL94_ROM");
    if (!rom_load(rom ? rom : "NHL 94 (UE) [!].gen")) { fprintf(stderr, "cannot load ROM (set NHL94_ROM)\n"); return 2; }
    league_retail();                                      /* (the league's tables, the cartridge's) */
    periods();
    overtime();
    weights();
    assists();
    plusminus();
    stats_list();
    play_rules();
    overall_ratings();
    column_ratings();
    printf("rulestest: %d checks, %d failed\n", checks, fails);
    return fails != 0;
}
