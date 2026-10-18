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
static const char *const view_names[2] = { "classic", "wide" };
static const char *const view_labels[2] = { "256 wide", "320 wide" };

/* the rules that are numbers: their named values (the page steps through them; the settings file
 * and --rule take a name or any number in the range) */
static const char *const overtime_names[3] = { "classic", "5min", "3min" };
static const char *const overtime_labels[3] = { "10:00", "5:00", "3:00" };
static const int overtime_minutes[3] = { 10, 5, 3 };
static const char *const skating_names[4] = { "classic", "faster", "fastest", "slower" };
static const char *const skating_labels[4] = { "classic", "faster, 110%", "fastest, 125%", "slower, 90%" };
static const int skating_pct[4] = { 100, 110, 125, 90 };
static const char *const burst_names[4] = { "classic", "weaker", "weakest", "stronger" };
static const char *const burst_labels[4] = { "classic", "weaker, 50%", "weakest, 25%", "stronger, 150%" };
static const int burst_pct[4] = { 100, 50, 25, 150 };
static const char *const shot_names[3] = { "classic", "harder", "softer" };
static const char *const shot_labels[3] = { "classic", "harder, 115%", "softer, 85%" };
static const int shot_pct[3] = { 100, 115, 85 };
static const char *const slide_names[3] = { "classic", "farther", "shorter" };
static const char *const slide_labels[3] = { "classic", "farther, 67%", "shorter, 150%" };
static const int slide_pct[3] = { 100, 67, 150 };
static const char *const tiring_names[3] = { "classic", "less", "more" };
static const char *const tiring_labels[3] = { "classic", "less, 50%", "more, 150%" };
static const int tiring_pct[3] = { 100, 50, 150 };
static const char *const rest_names[3] = { "classic", "faster", "slower" };
static const char *const rest_labels[3] = { "classic", "faster, 150%", "slower, 67%" };
static const int rest_pct[3] = { 100, 150, 67 };
static const char *const penalty_names[3] = { "classic", "fewer", "rare" };
static const char *const penalty_labels[3] = { "classic", "half, 50%", "a quarter, 25%" };
static const int penalty_pct[3] = { 100, 50, 25 };
static const char *const streak_names[3] = { "classic", "milder", "off" };
static const char *const streak_labels[3] = { "classic", "milder, 50%", "off, 0%" };
static const int streak_pct[3] = { 100, 50, 0 };

/* the numbers' ranges: the percentages within what the game's motion takes (headless games at
 * each end, rules_test) */
const mdrt_sdl_option rule_options[RULE_COUNT] = {
    [RULE_PERIOD_LENGTHS] = { "rules.period_lengths", "Period lengths", 2, period_names, period_labels, 0, 0, 0, 0, NULL, NULL },
    [RULE_OVERTIME] = { "rules.overtime", "Season overtime", 3, overtime_names, overtime_labels, 1, 20, 1, 0, overtime_minutes, " min" },
    [RULE_WEIGHT_FIX] = { "rules.weight_fix", "Weight bug fix", 2, off_on, NULL, 0, 0, 0, 0, NULL, NULL },
    [RULE_ASSIST_FIX] = { "rules.assist_fix", "Second assist fix", 2, off_on, NULL, 0, 0, 0, 0, NULL, NULL },
    [RULE_MORE_STATS] = { "rules.more_stats", "Plus/minus and checks", 2, off_on, NULL, 0, 0, 0, 0, NULL, NULL },
    [RULE_SKATING] = { "rules.skating", "Skating speed", 4, skating_names, skating_labels, 50, 150, 5, 0, skating_pct, "%" },
    [RULE_BURST] = { "rules.burst", "Speed burst", 4, burst_names, burst_labels, 0, 200, 5, 0, burst_pct, "%" },
    [RULE_SHOT_POWER] = { "rules.shot_power", "Shot power", 3, shot_names, shot_labels, 50, 150, 5, 0, shot_pct, "%" },
    [RULE_PUCK_SLIDE] = { "rules.puck_slide", "Puck slowing", 3, slide_names, slide_labels, 25, 200, 5, 0, slide_pct, "%" },
    [RULE_TIRING] = { "rules.tiring", "Tiring", 3, tiring_names, tiring_labels, 0, 200, 5, 0, tiring_pct, "%" },
    [RULE_BENCH_REST] = { "rules.bench_rest", "Bench rest", 3, rest_names, rest_labels, 0, 200, 5, 0, rest_pct, "%" },
    [RULE_BOX_REST] = { "rules.box_rest", "Rest in the penalty box", 2, off_on, NULL, 0, 0, 0, 0, NULL, NULL },
    [RULE_PENALTIES] = { "rules.penalties", "Penalties called", 3, penalty_names, penalty_labels, 0, 100, 5, 0, penalty_pct, "%" },
    [RULE_STREAKS] = { "rules.streaks", "Hot and cold streaks", 3, streak_names, streak_labels, 0, 200, 5, 0, streak_pct, "%" },
    [RULE_VIEW] = { "rules.view", "Rink view", 2, view_names, view_labels, 0, 0, 0, 0, NULL, NULL },
};

/* a choice's value: its number, or (a list's) its index */
static int value_of(int rule, int choice) { return rule_options[rule].numbers ? rule_options[rule].numbers[choice] : choice; }

/* a value the rule takes, or -1 */
static int valid(int rule, long v)
{
    const mdrt_sdl_option *op = &rule_options[rule];
    if (op->numbers) return v >= op->min && v <= op->max ? (int)v : -1;
    return v >= 0 && v < op->count ? (int)v : -1;
}

/* the presets, the page's first line: each sets every rule (custom: as they are) */
static const char *const preset_names[4] = { "classic", "simulation", "arcade", "custom" };
enum { PRESET_CLASSIC, PRESET_SIMULATION, PRESET_ARCADE, PRESET_CUSTOM };
static const uint8_t presets[3][RULE_COUNT] = {
    [PRESET_CLASSIC] = RULES_CLASSIC,
    [PRESET_SIMULATION] = {
        [RULE_PERIOD_LENGTHS] = 1, [RULE_OVERTIME] = 5, [RULE_WEIGHT_FIX] = 1, [RULE_ASSIST_FIX] = 1, [RULE_MORE_STATS] = 1,
        [RULE_SKATING] = 100, [RULE_BURST] = 50, [RULE_SHOT_POWER] = 100, [RULE_PUCK_SLIDE] = 100, [RULE_TIRING] = 100,
        [RULE_BENCH_REST] = 100, [RULE_BOX_REST] = 1, [RULE_PENALTIES] = 50, [RULE_STREAKS] = 50,
    },
    [PRESET_ARCADE] = {
        [RULE_PERIOD_LENGTHS] = 1, [RULE_OVERTIME] = 10, [RULE_MORE_STATS] = 1, [RULE_SKATING] = 110, [RULE_BURST] = 150,
        [RULE_SHOT_POWER] = 115, [RULE_PUCK_SLIDE] = 67, [RULE_TIRING] = 50, [RULE_BENCH_REST] = 100, [RULE_PENALTIES] = 50,
        [RULE_STREAKS] = 100,
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
    rule_page[P_LEAGUE] = (mdrt_sdl_option){ "rules.league", "League", n, names, labels, 0, 0, 0, 0, NULL, NULL };
    rule_page[P_PRESET] = (mdrt_sdl_option){ "rules.preset", "Preset", 4, preset_names, NULL, 0, 0, 0, 0, NULL, NULL };
    memcpy(rule_page + P_RULES, rule_options, sizeof rule_options);
}

const mdrt_sdl_option *rules_page(int *count)
{
    page_init();
    *count = P_RULES + RULE_COUNT;
    return rule_page;
}

/* a preset chosen sets every rule but the view (a matter of taste); a rule changed, or the rules
 * as the settings file has them (which -1: edited there, perhaps), make the preset custom (or the
 * preset they now are) */
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

static uint8_t next[RULE_COUNT] = RULES_CLASSIC;   /* the rules of the next game */

/* the platform's values: a number rule's number, a list's index (one out of range: Classic) */
void rules_next(const int *values, void *user)
{
    (void)user;
    for (int i = 0; i < RULE_COUNT; i++) next[i] = (uint8_t)(valid(i, values[i]) < 0 ? rule_classic[i] : values[i]);
}

void rules_power_on(void) { memcpy(rules, next, sizeof rules); }

int rules_classic(void) { return !memcmp(next, rule_classic, sizeof next); }

/* names[0..count) joined with ", " into out */
static void joined(char *out, size_t size, const char *const *names, int count)
{
    size_t at = 0;
    out[0] = 0;
    for (int k = 0; k < count && at < size; k++) at += (size_t)snprintf(out + at, size - at, "%s%s", k ? ", " : "", names[k]);
}

/* NAME=VALUE: the rule's key with or without "rules.", the value's name or its number (a number
 * rule's percent or minutes, a list's index); preset=NAME sets them all (later ones change it).
 * NULL, or what is wrong (for the player: the names and numbers it takes) */
const char *rules_arg(const char *arg)
{
    static char why[400];
    char list[300];
    const char *eq = strchr(arg, '=');
    if (!eq) return "a rule is NAME=VALUE";
    size_t n = (size_t)(eq - arg);
    if ((n == 6 && !strncmp(arg, "preset", 6)) || (n == 12 && !strncmp(arg, "rules.preset", 12))) {
        for (int p = 0; p < 3; p++)
            if (!strcmp(eq + 1, preset_names[p])) {
                for (int i = 0; i < RULE_COUNT; i++) if (i != RULE_VIEW) next[i] = presets[p][i];
                return NULL;
            }
        joined(list, sizeof list, preset_names, 3);
        snprintf(why, sizeof why, "the presets are %s", list);
        return why;
    }
    for (int i = 0; i < RULE_COUNT; i++) {
        const mdrt_sdl_option *op = &rule_options[i];
        const char *key = op->key, *bare = strchr(key, '.') + 1;
        if (!((strlen(key) == n && !strncmp(arg, key, n)) || (strlen(bare) == n && !strncmp(arg, bare, n)))) continue;
        for (int v = 0; v < op->count; v++)
            if (!strcmp(eq + 1, op->names[v])) { next[i] = (uint8_t)value_of(i, v); return NULL; }
        char *end;
        long v = strtol(eq + 1, &end, 10);
        if (end != eq + 1 && !*end && valid(i, v) >= 0) { next[i] = (uint8_t)v; return NULL; }
        joined(list, sizeof list, op->names, op->count);
        if (op->numbers) snprintf(why, sizeof why, "%s takes %s, or %d to %d%s", bare, list, op->min, op->max, op->unit);
        else snprintf(why, sizeof why, "%s takes %s (or 0 to %d)", bare, list, op->count - 1);
        return why;
    }
    const char *bares[RULE_COUNT + 1];
    for (int i = 0; i < RULE_COUNT; i++) bares[i] = strchr(rule_options[i].key, '.') + 1;
    bares[RULE_COUNT] = "preset";
    joined(list, sizeof list, bares, RULE_COUNT + 1);
    snprintf(why, sizeof why, "no such rule (the rules: %s)", list);
    return why;
}

/* the state's part, the game's session: a version, the rules' count, each rule's value (a state
 * from a build with fewer rules leaves the rest Classic; one with more is not this build's), the
 * stats the rules keep beside the RAM (rule_plusminus), then (from version 3) the league: its
 * checksum (8 bytes, 0 the cartridge's) and folder (a length byte, the name). The league's tables
 * are in the ROM the state's RAM points into, so a state loads with its own league or not at all.
 * Version 4's values are the rules' numbers; before it, each rule's choice on the page. */
size_t rules_state_save(void *user, uint8_t *out)
{
    (void)user;
    const char *dir = league_dir();
    size_t n = strlen(dir) > 255 ? 255 : strlen(dir), at = 2 + RULE_COUNT + sizeof rule_plusminus;
    if (out) {
        uint64_t h = league_hash();
        out[0] = 4;
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
    if (size < 2 || in[0] < 2 || in[0] > 4 || in[1] > RULE_COUNT) return -1;
    size_t at = 2u + in[1] + sizeof rule_plusminus;
    uint64_t h = 0;
    char dir[256] = "";
    if (in[0] == 2 ? size != at : size < at + 9 || size != at + 9 + in[at + 8]) return -1;
    if (in[0] >= 3) {
        for (int k = 0; k < 8; k++) h = h << 8 | in[at + k];
        memcpy(dir, in + at + 9, in[at + 8]);
        dir[in[at + 8]] = 0;
    }
    uint8_t v[RULE_COUNT];
    memcpy(v, rule_classic, sizeof v);
    for (int i = 0; i < in[1]; i++) {
        int x = in[0] < 4 ? (in[2 + i] < rule_options[i].count ? value_of(i, in[2 + i]) : -1) : valid(i, in[2 + i]);
        if (x < 0) { fprintf(stderr, "the state: a rule out of range\n"); return -1; }
        v[i] = (uint8_t)x;
    }
    if (league_use(dir, h) < 0) { fprintf(stderr, "the state: %s\n", league_error()); return -1; }
    memcpy(rules, v, sizeof rules);
    memcpy(rule_plusminus, in + 2 + in[1], sizeof rule_plusminus);
    return 0;
}
