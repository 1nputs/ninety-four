/* Game rules (rules.h): Classic until the platform sets them at power on. */
#include <string.h>
#include "game.h"
#include "rules.h"

uint8_t rules[RULE_COUNT];
int8_t rule_plusminus[2][32];

int rule_percent(int rule)
{
    static const uint8_t pct[RULE_COUNT][4] = {
        [RULE_SKATING] = { 100, 110, 125, 90 },  [RULE_BURST] = { 100, 50, 25, 150 },
        [RULE_SHOT_POWER] = { 100, 115, 85 },     [RULE_PUCK_SLIDE] = { 100, 67, 150 },
        [RULE_TIRING] = { 100, 50, 150 },         [RULE_BENCH_REST] = { 100, 150, 67 },
        [RULE_STREAKS] = { 100, 50, 0 },
    };
    return pct[rule][rules[rule] & 3];
}

void rules_clear_stats(void) { memset(rule_plusminus, 0, sizeof rule_plusminus); }

void rules_count_goal(int scorer, int strength)
{
    if (!rules[RULE_MORE_STATS] || strength > 2) return;   /* (a power play goal: none, as the NHL counts) */
    for (int i = 0; i < 12; i++) {                        /* the players' objects: skaters on the ice */
        const struct SortObj *o = sortobj(i);
        if (o->position <= 0 || (o->pnum & 0x80)) continue;   /* (off the ice, a goalie, nobody) */
        int team = (o->pflags & (1 << pfteam)) ? 1 : 0;
        rule_plusminus[team][o->pnum & 31] += team == scorer ? 1 : -1;
    }
}
