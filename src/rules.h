/* Game rules: what a game is played by beyond the original. These are native extras, not in the
 * cartridge: each rule's value 0 is the original exactly ("Classic"), and every check (make test,
 * lockstep, golden) plays by Classic. The host sets the rules before a game starts (the menu's
 * Game rules page, --rule on the command line); every player plays by the same ones for the
 * whole game, and states keep them. The game code reads them here, never the settings file. */
#pragma once
#include <stdint.h>

enum {
    RULE_PERIOD_LENGTHS,        /* the setup's period lengths: 0 5, 10, 20 minutes; 1 also 2, 3, 7, 15 */
    RULE_OVERTIME,              /* regular season overtime: 0 10:00, 1 5:00, 2 3:00 */
    RULE_WEIGHT_FIX,            /* 1: a check's weights summed in full (the original's 8-bit sum wraps,
                                 * so a heavy skater checked by a lighter pad player falls easily) */
    RULE_ASSIST_FIX,            /* 1: a give-and-go keeps the second assist of the player who began it */
    RULE_MORE_STATS,            /* 1: plus/minus counted, and the Player Stats list shows it and the
                                 * body checks (two more columns, the sort scrolling to them) */
    /* how the game plays: each a percentage of the original (rule_percent), 0 the original's */
    RULE_SKATING,               /* skating's acceleration and top speed: 100, 110, 125, 90 % */
    RULE_BURST,                 /* the speed burst's push: 100, 50, 25, 150 % */
    RULE_SHOT_POWER,            /* a shot's speed: 100, 115, 85 % */
    RULE_PUCK_SLIDE,            /* the puck's slowing on the ice: 100, 67 (it slides farther), 150 % */
    RULE_TIRING,                /* the energy skating and bursts use: 100, 50, 150 % */
    RULE_BENCH_REST,            /* the energy the bench gives back: 100, 150, 67 % */
    RULE_BOX_REST,              /* 1: players in the penalty box rest as on the bench */
    RULE_PENALTIES,             /* penalties called (not icing or offside): all, about half, a quarter */
    RULE_STREAKS,               /* hot and cold streaks' effect on ratings: 100, 50, 0 % */
    RULE_COUNT
};
extern uint8_t rules[RULE_COUNT];   /* each rule's value (0: the original's) */
/* a percentage rule's percentage now (100: the original) */
int rule_percent(int rule);
/* v scaled by a percentage rule (exact for 100 %; toward zero, as C divides) */
static inline int32_t rule_scale(int rule, int32_t v) { return rules[rule] ? v * rule_percent(rule) / 100 : v; }

/* stats the rules keep beside the game's RAM (which stays the original's), in states too: each
 * team's (0 home, 1 away: team_struct) plus/minus by roster number, cleared with the game's
 * stats (clearTeamStats) */
extern int8_t rule_plusminus[2][32];
void rules_clear_stats(void);
/* a goal by team scorer (0 / 1; strength: its goal entry's 2 + skaters more than the other's):
 * plus one for its skaters on the ice, minus one for the other's, unless a power play goal */
void rules_count_goal(int scorer, int strength);
