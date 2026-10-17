/* Game rules (rules.h): Classic until the platform sets them at power on. */
#include <string.h>
#include "game.h"
#include "rules.h"

uint8_t rules[RULE_COUNT];
int8_t rule_plusminus[2][32];

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
