/* Game rules on the native side (src/rules.h): the catalogue the menu's Game rules page and the
 * command line work from, the rules of the next game, and the state part that keeps a game's. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "native.h"
#include "rules.h"

static const char *const off_on[2] = { "off", "on" };
static const char *const period_names[2] = { "classic", "more" };
static const char *const period_labels[2] = { "5, 10, 20 min", "2 to 20 min" };
static const char *const overtime_names[3] = { "classic", "5min", "3min" };
static const char *const overtime_labels[3] = { "10:00", "5:00", "3:00" };

const mdrt_sdl_option rule_options[RULE_COUNT] = {
    [RULE_PERIOD_LENGTHS] = { "rules.period_lengths", "Period lengths", 2, period_names, period_labels, 0, 0, 0, 0 },
    [RULE_OVERTIME] = { "rules.overtime", "Season overtime", 3, overtime_names, overtime_labels, 0, 0, 0, 0 },
    [RULE_WEIGHT_FIX] = { "rules.weight_fix", "Weight bug fix", 2, off_on, NULL, 0, 0, 0, 0 },
    [RULE_ASSIST_FIX] = { "rules.assist_fix", "Second assist fix", 2, off_on, NULL, 0, 0, 0, 0 },
    [RULE_MORE_STATS] = { "rules.more_stats", "Plus/minus and checks", 2, off_on, NULL, 0, 0, 0, 0 },
};

static uint8_t next[RULE_COUNT];        /* the rules of the next game */

void rules_next(const int *values, void *user)
{
    (void)user;
    for (int i = 0; i < RULE_COUNT; i++) next[i] = (uint8_t)(values[i] < 0 ? 0 : values[i] >= rule_options[i].count ? 0 : values[i]);
}

void rules_power_on(void) { memcpy(rules, next, sizeof rules); }

int rules_classic(void)
{
    for (int i = 0; i < RULE_COUNT; i++) if (next[i]) return 0;
    return 1;
}

/* NAME=VALUE: the rule's key with or without "rules.", the value's name or its number */
int rules_arg(const char *arg)
{
    const char *eq = strchr(arg, '=');
    if (!eq) return -1;
    size_t n = (size_t)(eq - arg);
    for (int i = 0; i < RULE_COUNT; i++) {
        const char *key = rule_options[i].key, *bare = strchr(key, '.') + 1;
        if (!((strlen(key) == n && !strncmp(arg, key, n)) || (strlen(bare) == n && !strncmp(arg, bare, n)))) continue;
        for (int v = 0; v < rule_options[i].count; v++)
            if (!strcmp(eq + 1, rule_options[i].names[v])) { next[i] = (uint8_t)v; return 0; }
        char *end;
        long v = strtol(eq + 1, &end, 10);
        if (*end || v < 0 || v >= rule_options[i].count) return -1;
        next[i] = (uint8_t)v;
        return 0;
    }
    return -1;
}

/* the state's part: a version, the rules' count, each rule's value (a state from a build with fewer
 * rules leaves the rest Classic; one with more is not this build's), then the stats the rules
 * keep beside the RAM (rule_plusminus) */
size_t rules_state_save(void *user, uint8_t *out)
{
    (void)user;
    if (out) {
        out[0] = 2;
        out[1] = RULE_COUNT;
        memcpy(out + 2, rules, RULE_COUNT);
        memcpy(out + 2 + RULE_COUNT, rule_plusminus, sizeof rule_plusminus);
    }
    return 2 + RULE_COUNT + sizeof rule_plusminus;
}

int rules_state_load(void *user, const uint8_t *in, size_t size)
{
    (void)user;
    if (size < 2 || in[0] != 2 || in[1] > RULE_COUNT || size != 2u + in[1] + sizeof rule_plusminus) return -1;
    memset(rules, 0, sizeof rules);
    memcpy(rules, in + 2, in[1]);
    memcpy(rule_plusminus, in + 2 + in[1], sizeof rule_plusminus);
    return 0;
}
