/* Leagues (src/league.h): who plays, from a folder of text files a player can edit, built in the
 * cartridge's own formats past its end at power on. A folder holds:
 *
 *   league.ini    name = ... and playoff_teams = N (the first N teams play for the cup; the rest,
 *                 the All-Stars, only in single games). Optional: the folder's name, every team.
 *   teams.csv     a team a row, in the order the game lists them: team (its abbreviation, how the
 *                 other files name it), city, nickname, arena, conference, art (the cartridge team
 *                 whose logo, banners and colours it shows), rating, offense and defense (0-7:
 *                 its other playoff games, simulated), pp, pk, home, away (0-15: its players'
 *                 bonuses on the power play, killing a penalty, at home, away), songs (the organ's
 *                 6), banner (the name on its banners, when not its city). An empty one: the art
 *                 team's. A team whose city is not its art team's gets banners of its own, its
 *                 name in the cartridge's letters on its art team's (banner.c).
 *   goalies.csv   team, number, name, 14 ratings (0-15: weight agility speed offense defense
 *                 shot_power checking glove_hand glove_left glove_right stick_right stick_left
 *                 consistency aggression) and start (1-15: how often he starts).
 *   players.csv   team, pos (F or D; C LW RW LD RD as well), number, name, 14 ratings (weight
 *                 agility speed offense defense shot_power checking hand stickhandling accuracy
 *                 endurance shot_pass passing aggression).
 *   lines.csv     team, line (SC1 SC2 CHK PP1 PP2 PK1 PK2 L8), G LD RD LW C RW X: the players'
 *                 numbers (empty: nobody). A team with none: made from its roster's order.
 *   colours.csv   team, palette (home away logo matchup matchup2), colours: 16 Genesis colour
 *                 words. A palette not given: the art team's.
 *   pictures.csv  team, number, picture: a cartridge picture (CardMarioLemieux, NoPicSkater1 ...).
 *   playoffs.csv  optional: rows of 16 teams, the draws a playoff starts from (a power of two of
 *                 them, at most 32); else made from the conferences.
 *
 * Columns go in any order, named by the first row; others are ignored, and so are empty rows and
 * rows starting with #. Text is plain ASCII, \xNN for another byte. A league has 18 to 32 teams
 * (18 or more for the cup); a team 2 to 4 goalies, 9 to 15 forwards, 6 to 15 defensemen and 25
 * players at most, each with his own number; names as long as the cartridge's longest. The game reads the league
 * through the tables of src/league.h, so it plays as the cartridge does with whatever teams it
 * has; the cartridge's own league, exported and built again, is the cartridge's byte for byte
 * (make leaguetest). */
#include <dirent.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "native.h"
#include "league.h"
#include "rom.h"
#include "symbols_gen.h"
#ifdef _WIN32
#include <direct.h>
#define make_dir(p) _mkdir(p)
#else
#define make_dir(p) mkdir(p, 0777)
#endif

/* the screens' room: the cartridge's longest */
#define LEN_CITY 14
#define LEN_ABBR 3
#define LEN_NICK 12
#define LEN_ARENA 23
#define LEN_NAME 19
#define LEN_CONF 15
#define LEN_BANNER 16
#define ROSTER 25               /* players a team: the cartridge's most */
#define CART_TEAMS 28
#define PICTURES 32
#define ROWS_MAX 32             /* playoff draws: the password keeps 5 bits */

static const char *const cart_abbr[CART_TEAMS] = {
    "ANH", "BOS", "BUF", "CGY", "CHI", "DAL", "DET", "EDM", "FLA", "HFD", "LA", "MTL", "NJ", "NYI",
    "NYR", "OTW", "PHI", "PIT", "QUE", "SJ", "STL", "TB", "TOR", "VAN", "WSH", "WPG", "ASE", "ASW",
};
enum { PAL_HOME, PAL_AWAY, PAL_LOGO, PAL_MATCHUP, PAL_MATCHUP2, PALS };
static const char *const pal_names[PALS] = { "home", "away", "logo", "matchup", "matchup2" };
static const char *const line_names[8] = { "SC1", "SC2", "CHK", "PP1", "PP2", "PK1", "PK2", "L8" };
static const char *const slot_names[7] = { "G", "LD", "RD", "LW", "C", "RW", "X" };
/* a player's 14 ratings, the nibbles after his number (the cartridge's order: wl so dp ch ga ey tm) */
static const char *const skater_cols[14] = { "weight", "agility", "speed", "offense", "defense", "shot_power", "checking",
                                             "hand", "stickhandling", "accuracy", "endurance", "shot_pass", "passing", "aggression" };
static const char *const goalie_cols[14] = { "weight", "agility", "speed", "offense", "defense", "shot_power", "checking",
                                             "glove_hand", "glove_left", "glove_right", "stick_right", "stick_left", "consistency", "aggression" };

struct player { char name[LEN_NAME + 1]; char pos; uint8_t number, nib[14], start; };
struct team {
    char abbr[LEN_ABBR + 1], city[LEN_CITY + 1], nickname[LEN_NICK + 1], arena[LEN_ARENA + 1], conference[LEN_CONF + 1];
    char banner[LEN_BANNER + 1];    /* the name on its banners ("": its city's, when its art team's is another) */
    int art;
    int rating, offense, defense, pp, pk, home, away, songs[6];   /* -1: the art team's */
    uint16_t pal[PALS][16];
    uint8_t has_pal[PALS];
    struct player pl[ROSTER];
    int players, goalies, forwards;
    uint8_t line[8][7];         /* roster slots from 1, 0 nobody */
    int has_lines;
    struct { uint32_t pic; uint8_t slot; } pic[PICTURES];
    int pics;
};
struct lg {
    char name[64];
    int teams, playoff_teams, rows;
    uint8_t seats[ROWS_MAX][16];
    struct team t[LEAGUE_MAX];
};
/* a league built: the cartridge with it past the end */
struct built { uint8_t *img; uint32_t size; struct league league; uint64_t hash; };

/* ---- errors ------------------------------------------------------------------------------------ */

static char err[700];
static const char *err_file;
static int err_line;

static int fail(const char *fmt, ...)
{
    char m[500];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(m, sizeof m, fmt, ap);
    va_end(ap);
    if (err_file && err_line) snprintf(err, sizeof err, "%s, line %d: %s", err_file, err_line, m);
    else if (err_file) snprintf(err, sizeof err, "%s: %s", err_file, m);
    else snprintf(err, sizeof err, "%s", m);
    return -1;
}

const char *league_error(void) { return err; }

/* ---- reading: files, CSV rows, fields ------------------------------------------------------------ */

/* the file's text (NUL ended), NULL when it is not there (*missing) or cannot be read */
static char *slurp(const char *dir, const char *name, int *missing)
{
    char path[1100];
    snprintf(path, sizeof path, "%s/%s", dir, name);
    *missing = 0;
    FILE *f = fopen(path, "rb");
    if (!f) { *missing = 1; return NULL; }
    long n = fseek(f, 0, SEEK_END) ? -1 : ftell(f);
    char *t = n >= 0 && n < (4 << 20) ? malloc((size_t)n + 1) : NULL;
    if (t && (fseek(f, 0, SEEK_SET) || fread(t, 1, (size_t)n, f) != (size_t)n)) { free(t); t = NULL; }
    fclose(f);
    if (!t) { err_file = name; err_line = 0; fail("cannot be read"); return NULL; }
    t[n] = 0;
    return t;
}

#define CSV_FIELDS 40
struct csv { char *p; int line, row_line; char *f[CSV_FIELDS]; int nf; int col[20]; };

/* the next row's fields, cut in place: 1, 0 at the end, -1 when it is malformed */
static int csv_row(struct csv *c)
{
    for (;;) {                                            /* (empty rows and # rows) */
        char *q = c->p;
        while (*q == ' ' || *q == '\t') q++;
        if (!*q) return 0;
        if (*q != '\n' && *q != '\r' && *q != '#' && *q != ',') break;
        if (*q == ',') {                                  /* a spreadsheet's empty row: commas only */
            char *r = q;
            while (*r == ',' || *r == ' ' || *r == '\t') r++;
            if (*r && *r != '\n' && *r != '\r') break;
            q = r;
        }
        while (*q && *q != '\n') q++;
        if (*q) q++;
        c->line++;
        c->p = q;
    }
    c->row_line = err_line = ++c->line;
    c->nf = 0;
    char *p = c->p;
    for (;;) {
        while (*p == ' ' || *p == '\t') p++;
        char *start, *end;
        if (*p == '"') {
            start = end = ++p;
            for (;;) {
                if (!*p) return fail("a quote is not closed");
                if (*p == '"') {
                    if (p[1] != '"') { p++; break; }
                    p++;
                }
                if (*p == '\n') c->line++;
                *end++ = *p++;
            }
            while (*p == ' ' || *p == '\t') p++;
            if (*p && *p != ',' && *p != '\n' && *p != '\r') return fail("text after a closing quote");
        } else {
            start = p;
            while (*p && *p != ',' && *p != '\n' && *p != '\r') p++;
            end = p;
            while (end > start && (end[-1] == ' ' || end[-1] == '\t')) end--;
        }
        char sep = *p;
        *end = 0;
        if (c->nf < CSV_FIELDS) c->f[c->nf++] = start;
        if (sep == ',') { p++; continue; }
        if (sep == '\r') { p++; if (*p == '\n') p++; }
        else if (sep == '\n') p++;
        break;
    }
    c->p = p;
    return 1;
}

static int same_word(const char *a, const char *b)
{
    for (; *a && *b; a++, b++) {
        char x = *a >= 'A' && *a <= 'Z' ? (char)(*a + 32) : *a, y = *b >= 'A' && *b <= 'Z' ? (char)(*b + 32) : *b;
        if (x != y) return 0;
    }
    return *a == *b;
}

/* the header row: where each of cols is (-1: not there); the first `need` must be */
static int csv_open(struct csv *c, char *text, const char *file, const char *const *cols, int ncols, int need)
{
    c->p = text;
    c->line = 0;
    err_file = file;
    err_line = 0;
    if (!strncmp(c->p, "\xEF\xBB\xBF", 3)) c->p += 3;   /* (a spreadsheet's UTF-8 mark) */
    int r = csv_row(c);
    if (r <= 0) return r < 0 ? -1 : fail("empty: the first row names the columns");
    for (int k = 0; k < ncols; k++) {
        c->col[k] = -1;
        for (int i = 0; i < c->nf; i++)
            if (same_word(c->f[i], cols[k])) { c->col[k] = i; break; }
        if (k < need && c->col[k] < 0) return fail("no %s column", cols[k]);
    }
    return 0;
}

static const char *get(const struct csv *c, int k) { int i = c->col[k]; return i >= 0 && i < c->nf ? c->f[i] : ""; }

static int hexd(int ch) { return ch >= '0' && ch <= '9' ? ch - '0' : ch >= 'a' && ch <= 'f' ? ch - 'a' + 10 : ch >= 'A' && ch <= 'F' ? ch - 'A' + 10 : -1; }

/* text into out (at most max bytes): plain ASCII, \xNN a byte, \\ a backslash */
static int text(const char *in, char *out, int max, const char *what)
{
    int n = 0;
    for (const unsigned char *p = (const unsigned char *)in; *p; p++) {
        unsigned v = *p;
        if (v == '\\') {
            if (p[1] == '\\') { v = '\\'; p++; }
            else if (p[1] == 'x' && hexd(p[2]) >= 0 && hexd(p[3]) >= 0) { v = (unsigned)(hexd(p[2]) << 4 | hexd(p[3])); p += 3; }
            else return fail("%s: \\ begins \\xNN (a byte) or \\\\ (a backslash)", what);
            if (!v) return fail("%s: \\x00 is not a character", what);
        } else if (v < 0x20 || v > 0x7E) {
            return fail("%s: only plain letters, digits and punctuation (\\xNN for another byte)", what);
        }
        if (n == max) return fail("%s is longer than %d characters (the screens' room)", what, max);
        out[n++] = (char)v;
    }
    out[n] = 0;
    return n;
}

static int num(const char *s, int lo, int hi, const char *what, int *out)
{
    char *end;
    long v = strtol(s, &end, 10);
    if (!*s) return fail("no %s", what);
    if (*end) return fail("%s: \"%s\" is not a number", what, s);
    if (v < lo || v > hi) return fail("%s: %ld is not from %d to %d", what, v, lo, hi);
    *out = (int)v;
    return 0;
}

/* an optional number: -1 when empty */
static int num_or_unset(const char *s, int lo, int hi, const char *what, int *out)
{
    if (!*s) { *out = -1; return 0; }
    return num(s, lo, hi, what, out);
}

static int team_named(const struct lg *L, const char *abbr)
{
    for (int t = 0; t < L->teams; t++)
        if (same_word(L->t[t].abbr, abbr)) return t;
    return -1;
}

static int find_team(const struct lg *L, const char *abbr, int *t)
{
    if (!*abbr) return fail("no team");
    if ((*t = team_named(L, abbr)) < 0) return fail("no team %s in teams.csv", abbr);
    return 0;
}

static int slot_of(const struct team *T, int number)
{
    for (int i = 0; i < T->players; i++)
        if (T->pl[i].number == number) return i;
    return -1;
}

/* a cartridge picture by its label (CardMarioLemieux, NoPicSkater1 ...), 0 when none */
static uint32_t picture_named(const char *name)
{
    if (strncmp(name, "Card", 4) && strncmp(name, "NoPic", 5)) return 0;
    for (size_t i = 0; i < sizeof rom_syms / sizeof rom_syms[0]; i++)
        if (rom_syms[i].name && !strcmp(rom_syms[i].name, name)) return rom_syms[i].addr;
    return 0;
}

static const char *picture_name(uint32_t addr)
{
    for (size_t i = 0; i < sizeof rom_syms / sizeof rom_syms[0]; i++)
        if (rom_syms[i].addr == addr && rom_syms[i].name && (!strncmp(rom_syms[i].name, "Card", 4) || !strncmp(rom_syms[i].name, "NoPic", 5)))
            return rom_syms[i].name;
    return NULL;
}

/* ---- the folder, read --------------------------------------------------------------------------- */

static int read_ini(const char *dir, struct lg *L)
{
    int missing;
    char *t = slurp(dir, "league.ini", &missing);
    if (!t) return missing ? 0 : -1;
    err_file = "league.ini";
    err_line = 0;
    int r = 0;
    for (char *line = t, *next; line && r == 0; line = next) {
        err_line++;
        next = strchr(line, '\n');
        if (next) *next++ = 0;
        char *e = line + strlen(line);
        while (e > line && (e[-1] == '\r' || e[-1] == ' ' || e[-1] == '\t')) *--e = 0;
        while (*line == ' ' || *line == '\t') line++;
        if (!*line || *line == '#' || *line == ';' || *line == '[') continue;
        char *eq = strchr(line, '=');
        if (!eq) { r = fail("not KEY = VALUE"); break; }
        char *k = eq;
        while (k > line && (k[-1] == ' ' || k[-1] == '\t')) k--;
        *k = 0;
        char *v = eq + 1;
        while (*v == ' ' || *v == '\t') v++;
        if (!strcmp(line, "name")) r = text(v, L->name, (int)sizeof L->name - 1, "name") < 0 ? -1 : 0;
        else if (!strcmp(line, "playoff_teams")) r = num(v, 18, LEAGUE_MAX, "playoff_teams", &L->playoff_teams);
    }
    free(t);
    return r;
}

static int read_teams(char *t, struct lg *L)
{
    static const char *const cols[] = { "team", "city", "nickname", "arena", "conference", "art", "rating", "offense", "defense",
                                        "pp", "pk", "home", "away", "songs", "banner" };
    struct csv c;
    if (csv_open(&c, t, "teams.csv", cols, 15, 2) < 0) return -1;
    int r;
    while ((r = csv_row(&c)) > 0) {
        if (L->teams == LEAGUE_MAX) return fail("more than %d teams", LEAGUE_MAX);
        struct team *T = &L->t[L->teams];
        int n = text(get(&c, 0), T->abbr, LEN_ABBR, "team");
        if (n <= 0) return n < 0 ? -1 : fail("no team (its abbreviation)");
        if (team_named(L, T->abbr) >= 0) return fail("team %s twice", T->abbr);
        L->teams++;
        if ((n = text(get(&c, 1), T->city, LEN_CITY, "city")) <= 0) return n < 0 ? -1 : fail("%s: no city", T->abbr);
        if (text(get(&c, 2), T->nickname, LEN_NICK, "nickname") < 0 || text(get(&c, 3), T->arena, LEN_ARENA, "arena") < 0
            || text(get(&c, 4), T->conference, LEN_CONF, "conference") < 0) return -1;
        const char *art = get(&c, 5);
        T->art = -1;
        for (int k = 0; k < CART_TEAMS; k++)
            if (same_word(*art ? art : T->abbr, cart_abbr[k])) T->art = k;
        if (T->art < 0)
            return fail("%s: art: %s is not a cartridge team (ANH BOS BUF ... WPG ASE ASW: whose logo and banners it shows)", T->abbr, *art ? art : "none given, and it");
        if (num_or_unset(get(&c, 6), 0, 99, "rating", &T->rating) < 0 || num_or_unset(get(&c, 7), 0, 7, "offense", &T->offense) < 0
            || num_or_unset(get(&c, 8), 0, 7, "defense", &T->defense) < 0 || num_or_unset(get(&c, 9), 0, 15, "pp", &T->pp) < 0
            || num_or_unset(get(&c, 10), 0, 15, "pk", &T->pk) < 0 || num_or_unset(get(&c, 11), 0, 15, "home", &T->home) < 0
            || num_or_unset(get(&c, 12), 0, 15, "away", &T->away) < 0) return -1;
        if (text(get(&c, 14), T->banner, LEN_BANNER, "banner") < 0) return -1;
        for (char *b = T->banner; *b; b++) if (*b >= 'a' && *b <= 'z') *b = (char)(*b - 32);
        const char *s = get(&c, 13);
        T->songs[0] = -1;
        if (*s) {
            for (int k = 0; k < 6; k++) {
                char *end;
                long v = strtol(s, &end, 10);
                if (end == s || v < 0 || v > 255) return fail("songs: 6 numbers (0-255) with spaces between");
                T->songs[k] = (int)v;
                s = end;
            }
            while (*s == ' ') s++;
            if (*s) return fail("songs: 6 numbers (0-255) with spaces between");
        }
    }
    if (r < 0) return -1;
    err_line = 0;
    if (L->teams < 18) return fail("%d teams: a league has at least 18 (16 in a playoff draw, and the two playing)", L->teams);
    return 0;
}

/* goalies.csv (skaters 0) or players.csv (1) */
static int read_players(char *t, struct lg *L, int skaters)
{
    const char *cols[19] = { "team", "number", "name", skaters ? "pos" : "start" };
    for (int k = 0; k < 14; k++) cols[4 + k] = skaters ? skater_cols[k] : goalie_cols[k];
    struct csv c;
    if (csv_open(&c, t, skaters ? "players.csv" : "goalies.csv", cols, 18, 18) < 0) return -1;
    int r;
    while ((r = csv_row(&c)) > 0) {
        int tm, v;
        if (find_team(L, get(&c, 0), &tm) < 0) return -1;
        struct team *T = &L->t[tm];
        if (T->players == ROSTER) return fail("%s: more than %d players", T->abbr, ROSTER);
        struct player *P = &T->pl[T->players];
        if (num(get(&c, 1), 0, 99, "number", &v) < 0) return -1;
        if (slot_of(T, v) >= 0) return fail("%s: number %d twice (lines and pictures name players by number)", T->abbr, v);
        P->number = (uint8_t)v;
        if ((v = text(get(&c, 2), P->name, LEN_NAME, "name")) <= 0) return v < 0 ? -1 : fail("no name");
        if (skaters) {
            const char *p = get(&c, 3);
            if (same_word(p, "F") || same_word(p, "C") || same_word(p, "LW") || same_word(p, "RW")) P->pos = 'F';
            else if (same_word(p, "D") || same_word(p, "LD") || same_word(p, "RD")) P->pos = 'D';
            else return fail("pos: F or D (C LW RW LD RD), not \"%s\"", p);
        } else {
            if (num(get(&c, 3), 1, 15, "start", &v) < 0) return -1;
            P->pos = 'G';
            P->start = (uint8_t)v;
        }
        for (int k = 0; k < 14; k++) {
            if (num(get(&c, 4 + k), 0, 15, cols[4 + k], &v) < 0) return -1;
            P->nib[k] = (uint8_t)v;
        }
        T->players++;
    }
    return r;
}

static int read_lines(char *t, struct lg *L)
{
    static const char *const cols[] = { "team", "line", "G", "LD", "RD", "LW", "C", "RW", "X" };
    struct csv c;
    if (csv_open(&c, t, "lines.csv", cols, 9, 9) < 0) return -1;
    int r;
    while ((r = csv_row(&c)) > 0) {
        int tm, line = -1;
        if (find_team(L, get(&c, 0), &tm) < 0) return -1;
        struct team *T = &L->t[tm];
        for (int k = 0; k < 8; k++)
            if (same_word(get(&c, 1), line_names[k]) || (get(&c, 1)[0] == '1' + k && !get(&c, 1)[1])) line = k;
        if (line < 0) return fail("line: SC1 SC2 CHK PP1 PP2 PK1 PK2 L8 (or 1-8), not \"%s\"", get(&c, 1));
        if (T->has_lines & 1 << line) return fail("%s: line %s twice", T->abbr, line_names[line]);
        T->has_lines |= 1 << line;
        for (int k = 0; k < 7; k++) {
            int n;
            const char *s = get(&c, 2 + k);
            if (*s == '#') s++;
            if (!*s) { T->line[line][k] = 0; continue; }
            if (num(s, 0, 99, slot_names[k], &n) < 0) return -1;
            int i = slot_of(T, n);
            if (i < 0) return fail("%s %s: no number %d on the team", T->abbr, slot_names[k], n);
            T->line[line][k] = (uint8_t)(i + 1);
        }
    }
    return r;
}

static int read_colours(char *t, struct lg *L)
{
    static const char *const cols[] = { "team", "palette", "colours" };
    struct csv c;
    if (csv_open(&c, t, "colours.csv", cols, 3, 3) < 0) return -1;
    int r;
    while ((r = csv_row(&c)) > 0) {
        int tm, p = -1;
        if (find_team(L, get(&c, 0), &tm) < 0) return -1;
        for (int k = 0; k < PALS; k++)
            if (same_word(get(&c, 1), pal_names[k])) p = k;
        if (p < 0) return fail("palette: home away logo matchup matchup2, not \"%s\"", get(&c, 1));
        const char *s = get(&c, 2);
        for (int k = 0; k < 16; k++) {
            char *end;
            while (*s == ' ') s++;
            unsigned long v = strtoul(s, &end, 16);
            if (end == s || v > 0xFFFF || (*end && *end != ' ')) return fail("colours: 16 Genesis colour words (0EEE ...) with spaces between");
            L->t[tm].pal[p][k] = (uint16_t)v;
            s = end;
        }
        while (*s == ' ') s++;
        if (*s) return fail("colours: 16 Genesis colour words (0EEE ...) with spaces between");
        L->t[tm].has_pal[p] = 1;
    }
    return r;
}

static int read_pictures(char *t, struct lg *L)
{
    static const char *const cols[] = { "team", "number", "picture" };
    struct csv c;
    if (csv_open(&c, t, "pictures.csv", cols, 3, 3) < 0) return -1;
    int r;
    while ((r = csv_row(&c)) > 0) {
        int tm, n;
        if (find_team(L, get(&c, 0), &tm) < 0 || num(get(&c, 1), 0, 99, "number", &n) < 0) return -1;
        struct team *T = &L->t[tm];
        int i = slot_of(T, n);
        uint32_t pic = picture_named(get(&c, 2));
        if (i < 0) return fail("%s: no number %d on the team", T->abbr, n);
        if (!pic) return fail("picture: \"%s\" is not one of the cartridge's (CardMarioLemieux, NoPicSkater1 ...)", get(&c, 2));
        if (T->pics == PICTURES) return fail("%s: more than %d pictures", T->abbr, PICTURES);
        T->pic[T->pics].pic = pic;
        T->pic[T->pics++].slot = (uint8_t)i;
    }
    return r;
}

static int read_playoffs(char *t, struct lg *L)
{
    static const char *const cols[16] = { "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15", "16" };
    struct csv c;
    if (csv_open(&c, t, "playoffs.csv", cols, 16, 16) < 0) return -1;
    int r;
    while ((r = csv_row(&c)) > 0) {
        if (L->rows == ROWS_MAX) return fail("more than %d draws", ROWS_MAX);
        uint32_t in = 0;
        for (int k = 0; k < 16; k++) {
            int tm;
            if (find_team(L, get(&c, k), &tm) < 0) return -1;
            if (tm >= L->playoff_teams) return fail("%s does not play for the cup (playoff_teams in league.ini)", L->t[tm].abbr);
            if (in & 1u << tm) return fail("%s twice in a draw", L->t[tm].abbr);
            in |= 1u << tm;
            L->seats[L->rows][k] = (uint8_t)tm;
        }
        L->rows++;
    }
    if (r < 0) return -1;
    err_line = 0;
    if (L->rows & (L->rows - 1)) return fail("%d draws: 1, 2, 4, 8, 16 or 32", L->rows);
    for (int tm = 0; tm < L->playoff_teams; tm++) {           /* (a new playoff looks for team 1 in them) */
        int seen = 0;
        for (int i = 0; i < L->rows * 16 && !seen; i++) seen = L->seats[i >> 4][i & 15] == tm;
        if (!seen) return fail("%s is in no draw: every playoff team needs one", L->t[tm].abbr);
    }
    return 0;
}

/* the draws, when not given: 32 of them, each conference's teams on its side when there are two
 * with 8 or more playoff teams, else 16 of them all; every playoff team in some (each drawn once
 * before any again), the order random from the league's teams */
static void make_draws(struct lg *L)
{
    uint32_t seed = 2166136261u;
    for (int t = 0; t < L->teams; t++)
        for (const char *s = L->t[t].abbr; *s; s++) seed = (seed ^ (uint8_t)*s) * 16777619u;
    seed |= 1;
    int pool[2][LEAGUE_MAX], n[2] = { 0, 0 }, sides = 2;
    const char *conf[2] = { NULL, NULL };
    for (int t = 0; t < L->playoff_teams; t++) {
        const char *c = L->t[t].conference;
        int s = !conf[0] || !strcmp(conf[0], c) ? 0 : !conf[1] || !strcmp(conf[1], c) ? 1 : 2;
        if (s == 2 || !*c) { sides = 1; break; }
        conf[s] = c;
        pool[s][n[s]++] = t;
    }
    if (sides == 2 && (n[0] < 8 || n[1] < 8)) sides = 1;
    if (sides == 1) {
        n[0] = L->playoff_teams;
        for (int t = 0; t < n[0]; t++) pool[0][t] = t;
    }
    int deck[2][LEAGUE_MAX], left[2] = { 0, 0 };
    L->rows = ROWS_MAX;
    for (int row = 0; row < ROWS_MAX; row++) {
        for (int s = 0; s < sides; s++) {
            int per = 16 / sides;
            uint32_t in = 0;
            for (int k = 0; k < per; k++) {
                if (!left[s]) {                           /* a new deck, shuffled, without this draw's teams */
                    for (int i = 0; i < n[s]; i++)
                        if (!(in & 1u << pool[s][i])) deck[s][left[s]++] = pool[s][i];
                    for (int i = left[s] - 1; i > 0; i--) {
                        seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5;
                        int j = (int)(seed % (uint32_t)(i + 1)), x = deck[s][i];
                        deck[s][i] = deck[s][j];
                        deck[s][j] = x;
                    }
                }
                int tm = deck[s][--left[s]];
                if (in & 1u << tm) { k--; continue; }     /* (drawn already from the last deck) */
                in |= 1u << tm;
                L->seats[row][s * per + k] = (uint8_t)tm;
            }
        }
    }
}

/* lines from the roster's order, for a team with none: the forwards and defensemen in turn */
static void make_lines(struct team *T)
{
    int f = 0, d = 0, nf = T->forwards, nd = T->players - T->goalies - T->forwards;
    for (int l = 0; l < 8; l++) {
        if (T->has_lines & 1 << l) continue;
        uint8_t *s = T->line[l];
        s[0] = 1;
        s[1] = (uint8_t)(1 + T->goalies + nf + d++ % nd);
        s[2] = (uint8_t)(1 + T->goalies + nf + d++ % nd);
        for (int k = 3; k < 7; k++) s[k] = (uint8_t)(1 + T->goalies + f++ % nf);
    }
}

static void free_lg(struct lg *L) { free(L); }

/* a folder's name without the slashes after it */
static void clean_dir(char *out, size_t size, const char *dir)
{
    snprintf(out, size, "%s", dir);
    for (size_t n = strlen(out); n > 1 && out[n - 1] == '/'; n--) out[n - 1] = 0;
}

/* the folder into L (allocated): 0, or -1 with league_error() */
static int read_league(const char *folder, struct lg **out)
{
    char dir[512];
    clean_dir(dir, sizeof dir, folder);
    struct lg *L = calloc(1, sizeof *L);
    char *t[7] = { NULL };
    static const char *const files[7] = { "teams.csv", "goalies.csv", "players.csv", "lines.csv", "colours.csv", "pictures.csv", "playoffs.csv" };
    int missing, r = -1;
    *out = NULL;
    err_file = NULL;
    err_line = 0;
    if (!L) return fail("out of memory");
    const char *base = strrchr(dir, '/');
    snprintf(L->name, sizeof L->name, "%.63s", base && base[1] ? base + 1 : dir);
    if (read_ini(dir, L) < 0) goto done;
    for (int i = 0; i < 7; i++) {
        t[i] = slurp(dir, files[i], &missing);
        if (!t[i] && !missing) goto done;
        if (!t[i] && i < 3) { err_file = files[i]; err_line = 0; fail("not in %s", dir); goto done; }
    }
    if (read_teams(t[0], L) < 0) goto done;
    if (!L->playoff_teams) L->playoff_teams = L->teams;
    err_file = "league.ini";
    err_line = 0;
    if (L->playoff_teams > L->teams) { fail("playoff_teams is %d, of %d teams", L->playoff_teams, L->teams); goto done; }
    if (read_players(t[1], L, 0) < 0) goto done;
    for (int i = 0; i < L->teams; i++) L->t[i].goalies = L->t[i].players;
    if (read_players(t[2], L, 1) < 0) goto done;
    err_line = 0;
    for (int i = 0; i < L->teams; i++) {                  /* the roster: goalies, forwards, defense */
        struct team *T = &L->t[i];
        struct player keep[ROSTER];
        int n = T->goalies;
        memcpy(keep, T->pl, sizeof keep);
        for (int k = T->goalies; k < T->players; k++) if (keep[k].pos == 'F') T->pl[n++] = keep[k];
        T->forwards = n - T->goalies;
        for (int k = T->goalies; k < T->players; k++) if (keep[k].pos == 'D') T->pl[n++] = keep[k];
        int nd = n - T->goalies - T->forwards;
        err_file = T->goalies < 2 || T->goalies > 4 ? "goalies.csv" : "players.csv";
        if (T->goalies < 2 || T->goalies > 4) { fail("%s has %d goalies: 2 to 4", T->abbr, T->goalies); goto done; }
        if (T->forwards < 9 || T->forwards > 15) { fail("%s has %d forwards: 9 to 15", T->abbr, T->forwards); goto done; }
        if (nd < 6 || nd > 15) { fail("%s has %d defensemen: 6 to 15", T->abbr, nd); goto done; }
    }
    if (t[3] && read_lines(t[3], L) < 0) goto done;
    for (int i = 0; i < L->teams; i++) make_lines(&L->t[i]);
    if (t[4] && read_colours(t[4], L) < 0) goto done;
    if (t[5] && read_pictures(t[5], L) < 0) goto done;
    if (t[6]) { if (read_playoffs(t[6], L) < 0) goto done; }
    else make_draws(L);
    r = 0;
done:
    for (int i = 0; i < 7; i++) free(t[i]);
    if (r < 0) free_lg(L);
    else *out = L;
    return r;
}

/* ---- the cartridge's league ---------------------------------------------------------------------- */

static uint16_t c16(const uint8_t *c, uint32_t a) { return (uint16_t)(c[a] << 8 | c[a + 1]); }
static uint32_t c32(const uint8_t *c, uint32_t a) { return (uint32_t)c16(c, a) << 16 | c16(c, a + 2); }

static void cart_string(const uint8_t *c, uint32_t *a, char *out, int max)
{
    uint16_t n = c16(c, *a);
    int k = 0;
    for (uint32_t i = 2; i < n && k < max; i++)
        if (c[*a + i]) out[k++] = (char)c[*a + i];
    out[k] = 0;
    *a += n;
}

/* team t of the cartridge, as a league folder has it */
static void cart_team(const uint8_t *c, int t, struct team *T)
{
    uint32_t b = c32(c, SYM_TeamList + 4u * (uint32_t)t), sr = b + c16(c, b + 8), so = c16(c, b + c16(c, b + 10));
    memset(T, 0, sizeof *T);
    T->art = t;
    for (int k = 0; k < 16; k++) {
        T->pal[PAL_HOME][k] = c16(c, b + c16(c, b + 2) + 2u * (uint32_t)k);
        T->pal[PAL_AWAY][k] = c16(c, b + c16(c, b + 2) + 32 + 2u * (uint32_t)k);
        T->pal[PAL_LOGO][k] = c16(c, SYM_TeamLogoPalettes + 32u * (uint32_t)t + 2u * (uint32_t)k);
        T->pal[PAL_MATCHUP][k] = c16(c, SYM_TeamPalettes + 64u * (uint32_t)t + 2u * (uint32_t)k);
        T->pal[PAL_MATCHUP2][k] = c16(c, SYM_TeamPalettes + 64u * (uint32_t)t + 32 + 2u * (uint32_t)k);
    }
    for (int p = 0; p < PALS; p++) T->has_pal[p] = 1;
    T->offense = c[sr] >> 4 & 7;
    T->defense = c[sr] & 7;
    T->pk = c[sr + 1] >> 4;
    T->pp = c[sr + 1] & 15;
    T->home = c[sr + 2] >> 4;
    T->away = c[sr + 2] & 15;
    T->forwards = c[sr + 3] >> 4;
    T->rating = c[SYM_TeamRatings + t];
    for (int k = 0; k < 6; k++) T->songs[k] = c[SYM_TeamSongs + 6u * (uint32_t)t + (uint32_t)k];
    for (int l = 0; l < 8; l++) memcpy(T->line[l], c + b + c16(c, b + 6) + 8u * (uint32_t)l, 7);
    T->has_lines = 0xFF;
    uint32_t p = b + c16(c, b);
    while (c16(c, p) != 2 && T->players < ROSTER) {
        struct player *P = &T->pl[T->players];
        cart_string(c, &p, P->name, LEN_NAME);
        P->number = (uint8_t)((c[p] >> 4) * 10 + (c[p] & 15));
        for (int k = 0; k < 14; k++) P->nib[k] = (uint8_t)(c[p + 1 + k / 2] >> (k & 1 ? 0 : 4) & 15);
        p += 8;
        T->players++;
    }
    for (uint16_t w = so; w; w = (uint16_t)(w << 4)) T->pl[T->goalies++].start = (uint8_t)(w >> 12);
    for (int i = 0; i < T->players; i++) T->pl[i].pos = i < T->goalies ? 'G' : i < T->goalies + T->forwards ? 'F' : 'D';
    p += 2;
    cart_string(c, &p, T->city, LEN_CITY);
    cart_string(c, &p, T->abbr, LEN_ABBR);
    cart_string(c, &p, T->nickname, LEN_NICK);
    cart_string(c, &p, T->arena, LEN_ARENA);
    for (uint32_t a = c32(c, SYM_FeaturedPictures + 4u * (uint32_t)t); c32(c, a) && T->pics < PICTURES; a += 6) {
        T->pic[T->pics].pic = c32(c, a);
        T->pic[T->pics++].slot = (uint8_t)c16(c, a + 4);
    }
    for (int r = 0; r < 32 && !T->conference[0]; r++)    /* the side of the cartridge's draws it plays on */
        for (int k = 0; k < 16; k++)
            if (c[SYM_playoffseats + 16u * (uint32_t)r + (uint32_t)k] == t) { strcpy(T->conference, k < 8 ? "east" : "west"); break; }
}

/* ---- building: the cartridge with the league past its end ---------------------------------------- */

struct buf { uint8_t *b; size_t n, cap; int bad; };

static void need(struct buf *b, size_t n)
{
    if (b->n + n <= b->cap) return;
    size_t cap = b->cap ? b->cap : 1 << 16;
    while (b->n + n > cap) cap *= 2;
    uint8_t *p = realloc(b->b, cap);
    if (!p) { b->bad = 1; b->n = 0; return; }
    b->b = p;
    b->cap = cap;
}
static void put(struct buf *b, const void *p, size_t n) { need(b, n); if (!b->bad) { memcpy(b->b + b->n, p, n); b->n += n; } }
static void put8(struct buf *b, unsigned v) { uint8_t x = (uint8_t)v; put(b, &x, 1); }
static void put16(struct buf *b, unsigned v) { put8(b, v >> 8); put8(b, v); }
static void put32(struct buf *b, uint32_t v) { put16(b, v >> 16); put16(b, v & 0xFFFF); }
static void even(struct buf *b) { if (b->n & 1) put8(b, 0); }
static void set16(struct buf *b, uint32_t at, unsigned v) { if (!b->bad) { b->b[at] = (uint8_t)(v >> 8); b->b[at + 1] = (uint8_t)v; } }
static void set32(struct buf *b, uint32_t at, uint32_t v) { set16(b, at, v >> 16); set16(b, at + 2, v & 0xFFFF); }
/* a String: its length word (with itself and the pad), the text, a 0 to make it even */
static void put_string(struct buf *b, const char *s)
{
    size_t n = strlen(s);
    put16(b, (unsigned)((2 + n + 1) & ~(size_t)1));
    put(b, s, n);
    even(b);
}

static uint32_t at(const struct buf *b) { return (uint32_t)b->n; }

/* the team's block, the cartridge's format: 6 offsets, its palettes, .sr, .sodds, the lines, the
 * players, the names */
static void put_block(struct buf *b, const struct team *T)
{
    uint32_t blk = at(b);
    put16(b, 146);
    put16(b, 12);
    put16(b, 0);                                          /* (the names: after the players) */
    put16(b, 82);
    put16(b, 76);
    put16(b, 80);
    for (int p = PAL_HOME; p <= PAL_AWAY; p++)
        for (int k = 0; k < 16; k++) put16(b, T->pal[p][k]);
    int nd = T->players - T->goalies - T->forwards;
    put8(b, (unsigned)(T->offense << 4 | T->defense));
    put8(b, (unsigned)(T->pk << 4 | T->pp));
    put8(b, (unsigned)(T->home << 4 | T->away));
    put8(b, (unsigned)(T->forwards << 4 | nd));
    unsigned odds = 0;
    for (int g = 0; g < T->goalies; g++) odds |= (unsigned)T->pl[g].start << (12 - 4 * g);
    put16(b, odds);
    for (int l = 0; l < 8; l++) { put(b, T->line[l], 7); put8(b, 0); }
    for (int i = 0; i < T->players; i++) {
        const struct player *P = &T->pl[i];
        put_string(b, P->name);
        put8(b, (unsigned)((P->number / 10) << 4 | P->number % 10));
        for (int k = 0; k < 14; k += 2) put8(b, (unsigned)(P->nib[k] << 4 | P->nib[k + 1]));
    }
    put16(b, 2);
    set16(b, blk + 4, at(b) - blk);
    put_string(b, T->city);
    put_string(b, T->abbr);
    put_string(b, T->nickname);
    put_string(b, T->arena);
}

static uint64_t fnv64(uint64_t h, const void *p, size_t n)
{
    for (const uint8_t *s = p; n--; s++) h = (h ^ *s) * 0x100000001B3ull;
    return h;
}

/* pad so the next size bytes do not cross a 128 KB boundary: the VDP's DMA does not cross one */
static void dma_align(struct buf *b, uint32_t size)
{
    uint32_t a = at(b);
    if ((a >> 17) != ((a + size - 1) >> 17))
        while (at(b) & 0x1FFFF && !b->bad) put8(b, 0);
}

/* the team name banners, when a team has its own: Teamblocksmap again with its banners' tiles
 * and map rows after the cartridge's 28 (plain tiles, as the game loads them for the playoff
 * tree), and every team's TeamBitmaps banner (the setup screen draws them one at a time) */
static void put_banners(struct buf *b, struct lg *L, const uint8_t *c, const char *const text[], struct league *lg)
{
    static uint8_t tiles[22 * 32];
    uint8_t px[16][88];
    int count, own = 0;
    uint32_t map;
    const uint8_t *cart_tiles = banner_set(BANNER_BLOCKS, &count, &map);
    uint32_t size = 8 + 2 + 32u * (uint32_t)count + 22u * 32 * LEAGUE_MAX;
    dma_align(b, size);
    uint32_t tb = lg->table[LG_Teamblocksmap] = at(b);
    put32(b, SYM_Teamblocksmap + c32(c, SYM_Teamblocksmap) - tb);   /* (its palette: the cartridge's) */
    put32(b, 0);
    for (int t = 0; t < L->teams; t++) own += text[t] != NULL;
    put16(b, (unsigned)(count + 22 * own));               /* (plain: DMA'd as they are) */
    put(b, cart_tiles, 32u * (uint32_t)count);
    for (int t = 0, k = 0; t < L->teams; t++) {
        lg->block[t] = (uint8_t)L->t[t].art;
        if (!text[t]) continue;
        banner_pixels(BANNER_BLOCKS, L->t[t].art, text[t], px);
        banner_tiles(px, tiles);
        lg->block[t] = (uint8_t)(CART_TEAMS + k++);
        lg->block_tiles[t] = at(b);
        put(b, tiles, sizeof tiles);
    }
    set32(b, tb + 4, at(b) - tb);
    put16(b, 11);
    put16(b, (unsigned)(2 * (CART_TEAMS + own)));
    put(b, c + map + 4, 2u * 11 * 2 * CART_TEAMS);
    for (int t = 0, k = 0; t < L->teams; t++) {
        if (!text[t]) continue;
        for (int i = 0; i < 22; i++)                      /* (its art team's priority and palette, no flips) */
            put16(b, (c16(c, map + 4 + 2u * (uint32_t)(L->t[t].art * 22 + i)) & 0xE000) | (unsigned)(count + 22 * k + i));
        k++;
    }
    dma_align(b, 22u * 32 * (uint32_t)L->teams);
    for (int t = 0; t < L->teams; t++) {
        banner_pixels(BANNER_BITMAPS, L->t[t].art, text[t], px);
        banner_tiles(px, tiles);
        lg->banner_tiles[t] = at(b);
        put(b, tiles, sizeof tiles);
    }
    banner_set(BANNER_BITMAPS, &count, &map);
    lg->table[LG_BannerSlotMap] = at(b);
    put16(b, 11);
    put16(b, 2);
    for (int i = 0; i < 22; i++) put16(b, (c16(c, map + 4 + 2u * (uint32_t)i) & 0xE000) | (unsigned)i);
}

/* L built on the cartridge c: the art team's for what the folder leaves out */
static int build(struct lg *L, const uint8_t *c, struct built *B)
{
    struct buf b = { 0 };
    const char *text[LEAGUE_MAX] = { NULL };
    int own = 0;
    memset(B, 0, sizeof *B);
    if (banner_prepare(c) < 0) return fail("the cartridge's team banners cannot be read");
    for (int t = 0; t < L->teams; t++) {                  /* (what a team leaves to its art team's) */
        struct team *T = &L->t[t], A;
        cart_team(c, T->art, &A);
        if (!T->banner[0] && !same_word(T->city, A.city))   /* (a city of its own: its name on its banners) */
            for (size_t k = 0; k <= strlen(T->city) && k <= LEN_BANNER; k++)
                T->banner[k] = T->city[k] >= 'a' && T->city[k] <= 'z' ? (char)(T->city[k] - 32) : T->city[k];
        if (T->banner[0]) {
            char why[200];
            if (banner_check(T->banner, why, sizeof why) < 0) { err_file = "teams.csv"; err_line = 0; return fail("%s: %s", T->abbr, why); }
            text[t] = T->banner;
            own++;
        }
        int *mine[7] = { &T->rating, &T->offense, &T->defense, &T->pp, &T->pk, &T->home, &T->away };
        const int theirs[7] = { A.rating, A.offense, A.defense, A.pp, A.pk, A.home, A.away };
        for (int k = 0; k < 7; k++) if (*mine[k] < 0) *mine[k] = theirs[k];
        if (T->songs[0] < 0) memcpy(T->songs, A.songs, sizeof T->songs);
        for (int p = 0; p < PALS; p++)
            if (!T->has_pal[p]) memcpy(T->pal[p], A.pal[p], sizeof T->pal[p]);
    }
    put(&b, c, ROM_SIZE);
    struct league *lg = &B->league;
    uint32_t list = at(&b);
    lg->table[LG_TeamList] = list;
    for (int t = 0; t < L->teams; t++) put32(&b, 0);
    for (int t = 0; t < L->teams; t++) {
        set32(&b, list + 4u * (uint32_t)t, at(&b));
        put_block(&b, &L->t[t]);
    }
    lg->table[LG_TeamPalettes] = at(&b);
    for (int t = 0; t < L->teams; t++)
        for (int p = PAL_MATCHUP; p <= PAL_MATCHUP2; p++)
            for (int k = 0; k < 16; k++) put16(&b, L->t[t].pal[p][k]);
    lg->table[LG_TeamLogoBitmaps] = at(&b);
    for (int t = 0; t < L->teams; t++) put32(&b, c32(c, SYM_TeamLogoBitmaps + 4u * (uint32_t)L->t[t].art));
    lg->table[LG_TeamGfxList] = at(&b);
    for (int t = 0; t < L->teams; t++) put32(&b, c32(c, SYM_TeamGfxList + 4u * (uint32_t)L->t[t].art));
    lg->table[LG_TeamSongs] = at(&b);
    for (int t = 0; t < L->teams; t++)
        for (int k = 0; k < 6; k++) put8(&b, (unsigned)L->t[t].songs[k]);
    even(&b);
    lg->table[LG_TeamRatings] = at(&b);
    for (int t = 0; t < L->teams; t++) put8(&b, (unsigned)L->t[t].rating);
    even(&b);
    lg->table[LG_TeamLogoPalettes] = at(&b);
    for (int t = 0; t < L->teams; t++)
        for (int k = 0; k < 16; k++) put16(&b, L->t[t].pal[PAL_LOGO][k]);
    uint32_t pics = lg->table[LG_FeaturedPictures] = at(&b);
    for (int t = 0; t < L->teams; t++) put32(&b, 0);
    for (int t = 0; t < L->teams; t++) {
        set32(&b, pics + 4u * (uint32_t)t, at(&b));
        for (int i = 0; i < L->t[t].pics; i++) { put32(&b, L->t[t].pic[i].pic); put16(&b, L->t[t].pic[i].slot); }
        put32(&b, 0);
    }
    lg->table[LG_playoffseats] = at(&b);
    put(&b, L->seats, 16u * (uint32_t)L->rows);
    lg->table[LG_PlayerRecordOffsets] = at(&b);           /* (save RAM: the cartridge's places, the rest where it has room) */
    for (int t = 0; t < L->teams; t++)
        put16(&b, t < CART_TEAMS ? 26u * (unsigned)t : LEAGUE_SRAM_PLAYERS / 4 + 26u * (unsigned)(t - CART_TEAMS));
    lg->table[LG_Teamblocksmap] = SYM_Teamblocksmap;
    for (int t = 0; t < L->teams; t++) lg->block[t] = (uint8_t)L->t[t].art;
    if (own) put_banners(&b, L, c, text, lg);
    if (b.bad) { free(b.b); return fail("out of memory"); }
    lg->teams = (uint16_t)L->teams;
    lg->playoff_teams = (uint16_t)L->playoff_teams;
    lg->playoff_rows = (uint16_t)L->rows;
    for (int t = 0; t < L->teams; t++) lg->art[t] = (uint8_t)L->t[t].art;
    B->img = b.b;
    B->size = at(&b);
    uint64_t h = fnv64(0xCBF29CE484222325ull, B->img + ROM_SIZE, B->size - ROM_SIZE);
    uint8_t counts[6] = { (uint8_t)(lg->teams >> 8), (uint8_t)lg->teams, (uint8_t)(lg->playoff_teams >> 8), (uint8_t)lg->playoff_teams,
                          (uint8_t)(lg->playoff_rows >> 8), (uint8_t)lg->playoff_rows };
    h = fnv64(fnv64(fnv64(h, counts, sizeof counts), lg->art, sizeof lg->art), lg->block, sizeof lg->block);
    B->hash = h ? h : 1;                                  /* (0 is the cartridge's) */
    return 0;
}

/* ---- this game's league ------------------------------------------------------------------------ */

static struct { char dir[512]; char name[64]; uint64_t hash; uint8_t *img; } now;
static char next_dir[512];

static void use_cartridge(void)
{
    rom_image = rom_cart;
    rom_limit = ROM_SIZE;
    league_retail();
    free(now.img);
    now.img = NULL;
    now.dir[0] = now.name[0] = 0;
    now.hash = 0;
}

static void use_built(struct built *B, const char *dir, const char *name)
{
    uint8_t *old = now.img;
    char clean[512];
    clean_dir(clean, sizeof clean, dir);
    dir = clean;
    rom_image = B->img;
    rom_limit = B->size;
    league = B->league;
    now.img = B->img;
    now.hash = B->hash;
    snprintf(now.dir, sizeof now.dir, "%s", dir);
    snprintf(now.name, sizeof now.name, "%s", name);
    free(old);
}

/* dir's league built on the cartridge: 0, or -1 with league_error() */
static int build_dir(const char *dir, struct built *B, char *name, size_t name_size)
{
    struct lg *L;
    if (!rom_cart) return fail("no cartridge");
    if (read_league(dir, &L) < 0) return -1;
    err_file = NULL;
    err_line = 0;
    int r = build(L, rom_cart, B);
    if (name) snprintf(name, name_size, "%s", L->name);
    free_lg(L);
    return r;
}

void league_next(const char *dir) { clean_dir(next_dir, sizeof next_dir, dir ? dir : ""); }
int league_next_is_cartridge(void) { return !next_dir[0]; }
uint64_t league_hash(void) { return now.hash; }
const char *league_dir(void) { return now.dir; }
const char *league_name(void) { return now.name[0] ? now.name : "NHL '94"; }

void league_power_on(void)
{
    struct built B;
    char name[64];
    if (!next_dir[0]) { use_cartridge(); return; }
    if (build_dir(next_dir, &B, name, sizeof name) < 0) {
        fprintf(stderr, "%s: %s: the game is played with the cartridge's teams\n", next_dir, err);
        use_cartridge();
        return;
    }
    use_built(&B, next_dir, name);
}

int league_use(const char *dir, uint64_t hash)
{
    struct built B;
    char name[64];
    if (hash == now.hash) return 0;
    if (!hash) { use_cartridge(); return 0; }
    if (!dir || !*dir) return fail("a league with no folder");
    if (build_dir(dir, &B, name, sizeof name) < 0) return fail("its league, %s, is not there: %s", dir, err);
    if (B.hash != hash) { free(B.img); return fail("its league, %s, has changed since", dir); }
    use_built(&B, dir, name);
    return 0;
}

int league_check(const char *dir, int describe)
{
    struct built B;
    char name[64];
    if (build_dir(dir, &B, name, sizeof name) < 0) return -1;
    if (describe) printf("%s: %s, %d teams (%d for the cup), %d playoff draws, %u bytes past the cartridge, checksum %016llx\n", dir, name, B.league.teams,
           B.league.playoff_teams, B.league.playoff_rows, (unsigned)(B.size - ROM_SIZE), (unsigned long long)B.hash);
    free(B.img);
    return 0;
}

/* ---- the leagues/ folder, for the menu ----------------------------------------------------------- */

#define LIST_MAX 16
static char list_name[LIST_MAX][64], list_label[LIST_MAX][40];
static const char *list_names[LIST_MAX], *list_labels[LIST_MAX];

static int by_name(const void *a, const void *b) { return strcmp(a, b); }

int league_list(const char *const **names, const char *const **labels)
{
    int n = 1;
    char found[LIST_MAX * 2][64];
    int nf = 0;
    snprintf(list_name[0], sizeof list_name[0], "cartridge");
    snprintf(list_label[0], sizeof list_label[0], "NHL '94 (the cartridge)");
    DIR *d = opendir("leagues");
    struct dirent *e;
    while (d && (e = readdir(d)) && nf < LIST_MAX * 2) {
        char path[600];
        struct stat st;
        if (e->d_name[0] == '.' || strlen(e->d_name) >= sizeof found[0]) continue;
        snprintf(path, sizeof path, "leagues/%.200s/teams.csv", e->d_name);
        if (stat(path, &st) == 0) snprintf(found[nf++], sizeof found[0], "%.63s", e->d_name);
    }
    if (d) closedir(d);
    qsort(found, (size_t)nf, sizeof found[0], by_name);
    for (int i = 0; i < nf && n < LIST_MAX; i++) {
        char dir[600];
        struct lg *L;
        if (!strcmp(found[i], "cartridge")) { fprintf(stderr, "leagues/cartridge: the name is the cartridge's own: give the folder another\n"); continue; }
        snprintf(dir, sizeof dir, "leagues/%.63s", found[i]);
        if (read_league(dir, &L) < 0) { fprintf(stderr, "%s: %s (not in the menu)\n", dir, err); continue; }
        snprintf(list_name[n], sizeof list_name[n], "%.63s", found[i]);
        snprintf(list_label[n], sizeof list_label[n], "%.39s", L->name);
        free_lg(L);
        n++;
    }
    for (int i = 0; i < n; i++) { list_names[i] = list_name[i]; list_labels[i] = list_label[i]; }
    *names = list_names;
    *labels = list_labels;
    return n;
}

/* the save RAM's file for the next game: the cartridge's base, or base-NAME.srm (its records are its own) */
const char *league_sram(const char *base)
{
    static char path[1200];
    if (!next_dir[0] || !base) return base;
    const char *slash = strrchr(next_dir, '/'), *name = slash && slash[1] ? slash + 1 : next_dir;
    const char *dot = strrchr(base, '.'), *bs = strrchr(base, '/');
    int stem = dot && (!bs || dot > bs) ? (int)(dot - base) : (int)strlen(base);
    snprintf(path, sizeof path, "%.*s-%s.srm", stem, base, name);
    return path;
}

/* ---- the cartridge's league, written out --------------------------------------------------------- */

/* a CSV field: quoted when it must be, a byte outside plain ASCII as \xNN */
static void field(FILE *f, const char *s, int last)
{
    int quote = !*s ? 0 : s[0] == ' ' || s[strlen(s) - 1] == ' ' || s[0] == '#' || strpbrk(s, ",\"") != NULL;
    if (quote) fputc('"', f);
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        if (*p == '"') fputs("\"\"", f);
        else if (*p == '\\') fputs("\\\\", f);
        else if (*p < 0x20 || *p > 0x7E) fprintf(f, "\\x%02X", *p);
        else fputc(*p, f);
    }
    if (quote) fputc('"', f);
    fputc(last ? '\n' : ',', f);
}

static FILE *create(const char *dir, const char *name)
{
    char path[1100];
    snprintf(path, sizeof path, "%s/%s", dir, name);
    FILE *f = fopen(path, "w");
    if (!f) fail("cannot write %s", path);
    return f;
}

static int make_dirs(const char *dir)
{
    char p[1024];
    snprintf(p, sizeof p, "%s", dir);
    for (char *s = p + 1; *s; s++)
        if (*s == '/') { *s = 0; make_dir(p); *s = '/'; }
    make_dir(p);
    struct stat st;
    return stat(p, &st) == 0 && S_ISDIR(st.st_mode) ? 0 : fail("cannot make the folder %s", dir);
}

int league_export(const char *dir)
{
    const uint8_t *c = rom_cart;
    err_file = NULL;
    err_line = 0;
    if (!c) return fail("no cartridge");
    if (make_dirs(dir) < 0) return -1;
    static struct team T[CART_TEAMS];
    for (int t = 0; t < CART_TEAMS; t++) cart_team(c, t, &T[t]);
    FILE *f;
    char s[64];
    if (!(f = create(dir, "league.ini"))) return -1;
    fprintf(f, "# NHL '94's league, from the cartridge: the files beside this one are its teams, players,\n"
               "# lines, colours and pictures, to change as you like (README.md, \"Leagues\")\n"
               "name = NHL '94 (from the cartridge)\nplayoff_teams = %d\n", SYM_NumPlayoffTeams);
    fclose(f);
    if (!(f = create(dir, "teams.csv"))) return -1;
    fputs("team,city,nickname,arena,conference,art,rating,offense,defense,pp,pk,home,away,songs,banner\n", f);
    for (int t = 0; t < CART_TEAMS; t++) {
        const struct team *X = &T[t];
        field(f, X->abbr, 0); field(f, X->city, 0); field(f, X->nickname, 0); field(f, X->arena, 0); field(f, X->conference, 0);
        field(f, cart_abbr[t], 0);
        fprintf(f, "%d,%d,%d,%d,%d,%d,%d,%d %d %d %d %d %d,\n", X->rating, X->offense, X->defense, X->pp, X->pk, X->home, X->away,
                X->songs[0], X->songs[1], X->songs[2], X->songs[3], X->songs[4], X->songs[5]);
    }
    fclose(f);
    for (int sk = 0; sk < 2; sk++) {
        if (!(f = create(dir, sk ? "players.csv" : "goalies.csv"))) return -1;
        fputs(sk ? "team,pos,number,name" : "team,number,name", f);
        for (int k = 0; k < 14; k++) fprintf(f, ",%s", sk ? skater_cols[k] : goalie_cols[k]);
        fputs(sk ? "\n" : ",start\n", f);
        for (int t = 0; t < CART_TEAMS; t++)
            for (int i = sk ? T[t].goalies : 0; i < (sk ? T[t].players : T[t].goalies); i++) {
                const struct player *P = &T[t].pl[i];
                field(f, T[t].abbr, 0);
                if (sk) fprintf(f, "%c,", P->pos);
                fprintf(f, "%d,", P->number);
                field(f, P->name, 0);
                for (int k = 0; k < 14; k++) fprintf(f, "%d%c", P->nib[k], k < 13 || !sk ? ',' : '\n');
                if (!sk) fprintf(f, "%d\n", P->start);
            }
        fclose(f);
    }
    if (!(f = create(dir, "lines.csv"))) return -1;
    fputs("team,line,G,LD,RD,LW,C,RW,X\n", f);
    for (int t = 0; t < CART_TEAMS; t++)
        for (int l = 0; l < 8; l++) {
            field(f, T[t].abbr, 0);
            fputs(line_names[l], f);
            for (int k = 0; k < 7; k++) {
                int slot = T[t].line[l][k];
                if (slot) fprintf(f, ",%d", T[t].pl[slot - 1].number);
                else fputc(',', f);
            }
            fputc('\n', f);
        }
    fclose(f);
    if (!(f = create(dir, "colours.csv"))) return -1;
    fputs("team,palette,colours\n", f);
    for (int t = 0; t < CART_TEAMS; t++)
        for (int p = 0; p < PALS; p++) {
            field(f, T[t].abbr, 0);
            fprintf(f, "%s,", pal_names[p]);
            for (int k = 0; k < 16; k++) fprintf(f, "%04X%c", T[t].pal[p][k], k < 15 ? ' ' : '\n');
        }
    fclose(f);
    if (!(f = create(dir, "pictures.csv"))) return -1;
    fputs("team,number,picture\n", f);
    for (int t = 0; t < CART_TEAMS; t++)
        for (int i = 0; i < T[t].pics; i++) {
            const char *name = picture_name(T[t].pic[i].pic);
            if (!name || T[t].pic[i].slot >= T[t].players) continue;
            field(f, T[t].abbr, 0);
            fprintf(f, "%d,%s\n", T[t].pl[T[t].pic[i].slot].number, name);
        }
    fclose(f);
    if (!(f = create(dir, "playoffs.csv"))) return -1;
    for (int k = 1; k <= 16; k++) fprintf(f, "%d%c", k, k < 16 ? ',' : '\n');
    for (int r = 0; r < (int)SYM_PlayoffRows; r++)
        for (int k = 0; k < 16; k++) {
            int tm = c[SYM_playoffseats + 16u * (uint32_t)r + (uint32_t)k];
            snprintf(s, sizeof s, "%s", tm < CART_TEAMS ? T[tm].abbr : "?");
            field(f, s, k == 15);
        }
    fclose(f);
    return 0;
}
