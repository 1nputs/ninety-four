/* Game rules: what a game is played by beyond the original. These are native extras, not in the
 * cartridge: each rule's value is a number (an on / off rule 0 or 1, a list's choice its index, a
 * percentage the percent, a length its minutes), and rule_classic has the original's ("Classic"),
 * which every check (make test, lockstep, golden) plays by. The host sets the rules before a game
 * starts (the menu's Game rules page, --rule on the command line); every player plays by the same
 * ones for the whole game, and states keep them. The game code reads them here, never the
 * settings file. */
#pragma once
#include <stdint.h>

enum {
    RULE_PERIOD_LENGTHS,        /* the setup's period lengths: 0 5, 10, 20 minutes; 1 also 2, 3, 7, 15 */
    RULE_OVERTIME,              /* regular season overtime's minutes: 10 the original's */
    RULE_WEIGHT_FIX,            /* 1: a check's weights summed in full (the original's 8-bit sum wraps,
                                 * so a heavy skater checked by a lighter pad player falls easily) */
    RULE_ASSIST_FIX,            /* 1: a give-and-go keeps the second assist of the player who began it */
    RULE_MORE_STATS,            /* 1: plus/minus counted, and the Player Stats list shows it and the
                                 * body checks (two more columns, the sort scrolling to them) */
    /* how the game plays: each a percentage of the original's (rule_scale), 100 the original */
    RULE_SKATING,               /* skating's acceleration and top speed */
    RULE_BURST,                 /* the speed burst's push */
    RULE_SHOT_POWER,            /* a shot's speed */
    RULE_PUCK_SLIDE,            /* the puck's slowing on the ice (less: it slides farther) */
    RULE_TIRING,                /* the energy skating and bursts use */
    RULE_BENCH_REST,            /* the energy the bench gives back */
    RULE_BOX_REST,              /* 1: players in the penalty box rest as on the bench */
    RULE_PENALTIES,             /* the penalties called (not icing or offside), a percentage (rule_chance) */
    RULE_STREAKS,               /* hot and cold streaks' effect on ratings (0: none) */
    RULE_VIEW,                  /* 1: the rink 320 wide (src/view.h): the camera moves less, players
                                 * count as off screen further out (a pad takes its goalie later) */
    RULE_COUNT
};
/* the original's values; a percentage is at most 255 */
#define RULES_CLASSIC { [RULE_OVERTIME] = 10, [RULE_SKATING] = 100, [RULE_BURST] = 100, [RULE_SHOT_POWER] = 100, \
                        [RULE_PUCK_SLIDE] = 100, [RULE_TIRING] = 100, [RULE_BENCH_REST] = 100, [RULE_PENALTIES] = 100, \
                        [RULE_STREAKS] = 100 }
extern const uint8_t rule_classic[RULE_COUNT];
extern uint8_t rules[RULE_COUNT];   /* each rule's value now (Classic until the platform sets them) */
/* v scaled by a percentage rule (the original's for 100 %; toward zero, as C divides) */
static inline int32_t rule_scale(int rule, int32_t v) { return rules[rule] != 100 ? v * rules[rule] / 100 : v; }
/* yes percent times in 100 (a percentage rule's), at random: one random number below the
 * fraction's denominator in its lowest terms (so 50 % draws one of 2, 25 % one of 4), none for 0
 * or 100 */
int rule_chance(int percent);
/* the puck's slowing on the ice (d0: its velocity's shift) by RULE_PUCK_SLIDE. One scaled to
 * nothing below zero is -1, the least toward a stop: the original's shift is never 0 there, and
 * the 1 it takes for 0 is the least only above it. */
int16_t rule_puck_slowing(int16_t d0);

/* stats the rules keep beside the game's RAM (which stays the original's), in states too: each
 * team's (0 home, 1 away: team_struct) plus/minus by roster number, cleared with the game's
 * stats (clearTeamStats) */
extern int8_t rule_plusminus[2][32];
void rules_clear_stats(void);
/* a goal by team scorer (0 / 1; strength: its goal entry's 2 + skaters more than the other's):
 * plus one for its skaters on the ice, minus one for the other's, unless a power play goal */
void rules_count_goal(int scorer, int strength);
