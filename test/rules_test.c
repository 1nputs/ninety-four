/* The game rules (src/rules.h) on the game's own routines, with RAM set up by hand and a hardware
 * layer that does nothing (hal_null.c): Classic plays as the original, and each rule changes only
 * what it says. make rulestest (the ROM: NHL94_ROM, or the repository's). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "game.h"
#include "rom.h"
#include "rules.h"

static int checks, fails;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

/* the period lengths: GetPeriodTime for each option, and the setup menu's range for it */
static void periods(void)
{
    static const uint16_t classic[4] = { 300, 600, 1200, 30 }, more[8] = { 300, 420, 600, 900, 1200, 120, 180, 30 };
    memset(rules, 0, sizeof rules);
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
    memset(rules, 0, sizeof rules);
}

/* overtime: ResetClock in the regular season's overtime, and elsewhere the period's length */
static void overtime(void)
{
    static const uint16_t ot[3] = { 600, 300, 180 };
    g.OptPerlen = 0;
    for (int v = 0; v < 3; v++) {
        rules[RULE_OVERTIME] = (uint8_t)v;
        g.gsp = 3; g.OptPlayMode = 0;
        ResetClock();
        CHECK(g.gameclock == ot[v] && g.PerTimeTotal == ot[v]);
        g.gsp = 3; g.OptPlayMode = 2;                     /* (playoff overtime: a period, as the original) */
        ResetClock();
        CHECK(g.gameclock == 300);
        g.gsp = 1; g.OptPlayMode = 0;
        ResetClock();
        CHECK(g.gameclock == 300);
    }
    memset(rules, 0, sizeof rules);
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
    memset(rules, 0, sizeof rules);
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
        uint8_t *tm = team_struct(0);
        wr16(tm + tmlasttouch, 0xFF00);
        wr16(tm + tmassist1, 0xFF00);
        wr16(tm + tmassist2, 0xFF00);
        touch(pl, 5, puck);                               /* Z */
        touch(pl, 6, puck);                               /* B */
        touch(pl, 7, puck);                               /* A */
        touch(pl, 6, puck);                               /* B again */
        uint16_t last = rd16(tm + tmlasttouch), a1 = rd16(tm + tmassist1), a2 = rd16(tm + tmassist2);
        printf("  assists %s: last %04X, assists %04X %04X\n", fix ? "with the rule" : "classic", last, a1, a2);
        CHECK(last == 6 && a1 == 7);
        if (fix) CHECK(a2 == 5);                          /* Z keeps his */
        else CHECK((a2 >> 8) == 0xFF);                    /* none */
    }
    memset(rules, 0, sizeof rules);
}

int main(void)
{
    const char *rom = getenv("NHL94_ROM");
    if (!rom_load(rom ? rom : "NHL 94 (UE) [!].gen")) { fprintf(stderr, "cannot load ROM (set NHL94_ROM)\n"); return 2; }
    periods();
    overtime();
    weights();
    assists();
    printf("rulestest: %d checks, %d failed\n", checks, fails);
    return fails != 0;
}
