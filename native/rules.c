/* Game rules on the native side (src/rules.h): the catalogue the menu's Game rules page and the
 * command line work from, the rules of the next game, and the state part that keeps a game's
 * session (its rules, and its league: league.c). */
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

static const char *const skating_names[4] = { "classic", "faster", "fastest", "slower" };
static const char *const burst_names[4] = { "classic", "weaker", "weakest", "stronger" };
static const char *const shot_names[3] = { "classic", "harder", "softer" };
static const char *const slide_names[3] = { "classic", "farther", "shorter" };
static const char *const tiring_names[3] = { "classic", "less", "more" };
static const char *const rest_names[3] = { "classic", "faster", "slower" };
static const char *const penalty_names[3] = { "classic", "fewer", "rare" };
static const char *const penalty_labels[3] = { "classic", "about half", "a quarter" };
static const char *const streak_names[3] = { "classic", "milder", "off" };
static const char *const view_names[2] = { "classic", "wide" };
static const char *const view_labels[2] = { "256 wide", "320 wide" };

const mdrt_sdl_option rule_options[RULE_COUNT] = {
    [RULE_PERIOD_LENGTHS] = { "rules.period_lengths", "Period lengths", 2, period_names, period_labels, 0, 0, 0, 0 },
    [RULE_OVERTIME] = { "rules.overtime", "Season overtime", 3, overtime_names, overtime_labels, 0, 0, 0, 0 },
    [RULE_WEIGHT_FIX] = { "rules.weight_fix", "Weight bug fix", 2, off_on, NULL, 0, 0, 0, 0 },
    [RULE_ASSIST_FIX] = { "rules.assist_fix", "Second assist fix", 2, off_on, NULL, 0, 0, 0, 0 },
    [RULE_MORE_STATS] = { "rules.more_stats", "Plus/minus and checks", 2, off_on, NULL, 0, 0, 0, 0 },
    [RULE_SKATING] = { "rules.skating", "Skating speed", 4, skating_names, NULL, 0, 0, 0, 0 },
    [RULE_BURST] = { "rules.burst", "Speed burst", 4, burst_names, NULL, 0, 0, 0, 0 },
    [RULE_SHOT_POWER] = { "rules.shot_power", "Shot power", 3, shot_names, NULL, 0, 0, 0, 0 },
    [RULE_PUCK_SLIDE] = { "rules.puck_slide", "Puck slides", 3, slide_names, NULL, 0, 0, 0, 0 },
    [RULE_TIRING] = { "rules.tiring", "Tiring", 3, tiring_names, NULL, 0, 0, 0, 0 },
    [RULE_BENCH_REST] = { "rules.bench_rest", "Bench rest", 3, rest_names, NULL, 0, 0, 0, 0 },
    [RULE_BOX_REST] = { "rules.box_rest", "Rest in the penalty box", 2, off_on, NULL, 0, 0, 0, 0 },
    [RULE_PENALTIES] = { "rules.penalties", "Penalties called", 3, penalty_names, penalty_labels, 0, 0, 0, 0 },
    [RULE_STREAKS] = { "rules.streaks", "Hot and cold streaks", 3, streak_names, NULL, 0, 0, 0, 0 },
    [RULE_VIEW] = { "rules.view", "Rink view", 2, view_names, view_labels, 0, 0, 0, 0 },
};

/* the presets, the page's first line: each sets every rule (custom: as they are) */
static const char *const preset_names[4] = { "classic", "simulation", "arcade", "custom" };
enum { PRESET_CLASSIC, PRESET_SIMULATION, PRESET_ARCADE, PRESET_CUSTOM };
static const uint8_t presets[3][RULE_COUNT] = {
    [PRESET_CLASSIC] = { 0 },
    [PRESET_SIMULATION] = {
        [RULE_PERIOD_LENGTHS] = 1, [RULE_OVERTIME] = 1, [RULE_WEIGHT_FIX] = 1, [RULE_ASSIST_FIX] = 1, [RULE_MORE_STATS] = 1,
        [RULE_BURST] = 1, [RULE_BOX_REST] = 1, [RULE_PENALTIES] = 1, [RULE_STREAKS] = 1,
    },
    [PRESET_ARCADE] = {
        [RULE_PERIOD_LENGTHS] = 1, [RULE_MORE_STATS] = 1, [RULE_SKATING] = 1, [RULE_BURST] = 3, [RULE_SHOT_POWER] = 1,
        [RULE_PUCK_SLIDE] = 1, [RULE_TIRING] = 1, [RULE_PENALTIES] = 1,
    },
};

/* the page: the league (who plays: the cartridge's, or a folder of leagues/), the preset, then the rules */
enum { P_LEAGUE, P_PRESET, P_RULES };
mdrt_sdl_option rule_page[P_RULES + RULE_COUNT];

static void page_init(void)
{
    const char *const *names, *const *labels;
    if (rule_page[0].key) return;
    int n = league_list(&names, &labels);
    rule_page[P_LEAGUE] = (mdrt_sdl_option){ "rules.league", "League", n, names, labels, 0, 0, 0, 0 };
    rule_page[P_PRESET] = (mdrt_sdl_option){ "rules.preset", "Preset", 4, preset_names, NULL, 0, 0, 0, 0 };
    memcpy(rule_page + P_RULES, rule_options, sizeof rule_options);
}

const mdrt_sdl_option *rules_page(int *count)
{
    page_init();
    *count = P_RULES + RULE_COUNT;
    return rule_page;
}

/* a preset chosen sets every rule but the view (a matter of taste); a rule changed makes the
 * preset custom (or the preset it now is) */
int rules_page_changed(int which, int *values, void *user)
{
    (void)user;
    if (which == P_LEAGUE) return 0;
    if (which == P_PRESET) {
        if (values[P_PRESET] == PRESET_CUSTOM) return 0;
        for (int i = 0; i < RULE_COUNT; i++) if (i != RULE_VIEW) values[P_RULES + i] = presets[values[P_PRESET]][i];
        return 1;
    }
    int now = PRESET_CUSTOM;
    for (int p = 0; p < 3 && now == PRESET_CUSTOM; p++) {
        int same = 1;
        for (int i = 0; i < RULE_COUNT; i++) same &= i == RULE_VIEW || values[P_RULES + i] == presets[p][i];
        if (same) now = p;
    }
    if (values[P_PRESET] == now) return 0;
    values[P_PRESET] = now;
    return 1;
}

void rules_page_use(const int *values, void *user)
{
    char dir[600];
    int l = values[P_LEAGUE];
    snprintf(dir, sizeof dir, "leagues/%s", l > 0 && l < rule_page[P_LEAGUE].count ? rule_page[P_LEAGUE].names[l] : "");
    league_next(l > 0 && l < rule_page[P_LEAGUE].count ? dir : NULL);
    rules_next(values + P_RULES, user);
}

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

/* NAME=VALUE: the rule's key with or without "rules.", the value's name or its number; preset=NAME
 * sets them all (later ones change it) */
int rules_arg(const char *arg)
{
    const char *eq = strchr(arg, '=');
    if (!eq) return -1;
    size_t n = (size_t)(eq - arg);
    if ((n == 6 && !strncmp(arg, "preset", 6)) || (n == 12 && !strncmp(arg, "rules.preset", 12))) {
        for (int p = 0; p < 3; p++)
            if (!strcmp(eq + 1, preset_names[p])) {
                for (int i = 0; i < RULE_COUNT; i++) if (i != RULE_VIEW) next[i] = presets[p][i];
                return 0;
            }
        return -1;
    }
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

/* the state's part, the game's session: a version, the rules' count, each rule's value (a state
 * from a build with fewer rules leaves the rest Classic; one with more is not this build's), the
 * stats the rules keep beside the RAM (rule_plusminus), then (version 3) the league: its checksum
 * (8 bytes, 0 the cartridge's) and folder (a length byte, the name). The league's tables are in
 * the ROM the state's RAM points into, so a state loads with its own league or not at all. */
size_t rules_state_save(void *user, uint8_t *out)
{
    (void)user;
    const char *dir = league_dir();
    size_t n = strlen(dir) > 255 ? 255 : strlen(dir), at = 2 + RULE_COUNT + sizeof rule_plusminus;
    if (out) {
        uint64_t h = league_hash();
        out[0] = 3;
        out[1] = RULE_COUNT;
        memcpy(out + 2, rules, RULE_COUNT);
        memcpy(out + 2 + RULE_COUNT, rule_plusminus, sizeof rule_plusminus);
        for (int k = 0; k < 8; k++) out[at + k] = (uint8_t)(h >> (56 - 8 * k));
        out[at + 8] = (uint8_t)n;
        memcpy(out + at + 9, dir, n);
    }
    return at + 9 + n;
}

int rules_state_load(void *user, const uint8_t *in, size_t size)
{
    (void)user;
    if (size < 2 || (in[0] != 2 && in[0] != 3) || in[1] > RULE_COUNT) return -1;
    size_t at = 2u + in[1] + sizeof rule_plusminus;
    uint64_t h = 0;
    char dir[256] = "";
    if (in[0] == 2 ? size != at : size < at + 9 || size != at + 9 + in[at + 8]) return -1;
    if (in[0] == 3) {
        for (int k = 0; k < 8; k++) h = h << 8 | in[at + k];
        memcpy(dir, in + at + 9, in[at + 8]);
        dir[in[at + 8]] = 0;
    }
    if (league_use(dir, h) < 0) { fprintf(stderr, "the state: %s\n", league_error()); return -1; }
    memset(rules, 0, sizeof rules);
    memcpy(rules, in + 2, in[1]);
    memcpy(rule_plusminus, in + 2 + in[1], sizeof rule_plusminus);
    return 0;
}
