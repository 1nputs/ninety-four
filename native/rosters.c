/* Rosters (src/rosters.h): who plays, from a folder of text files a player can edit, made at power
 * on: its teams the game's tables (roster_team), its art past the cartridge's end in the cartridge's
 * formats. A folder holds:
 *
 *   rosters.ini    name = ... and playoff_teams = N (the first N teams play for the cup; the rest,
 *                 the All-Stars, only in single games); hiscore_screen = X,Y X,Y (where the
 *                 screen after the EA logo puts its two pictures: its logo, then HiScoreImg; a
 *                 hack's whole screen at 0,0 0,0). Optional: the folder's name, every team, the
 *                 cartridge's places.
 *   teams.csv     a team a row, in the order the game lists them: team (its abbreviation, how the
 *                 other files name it), city, nickname, arena, conference, art (the cartridge team
 *                 whose logo, banners and colours it shows), rating, offense and defense (0-7:
 *                 its other playoff games, simulated: offense the more it scores, defense the
 *                 more it lets in), pp, pk, home, away (0-15: its skaters' ratings gain on the
 *                 power play and lose killing a penalty, gain at home and lose away), songs (the organ's
 *                 6), banner (the name on its banners, when not its city). An empty one: the art
 *                 team's. A team whose city is not its art team's gets banners of its own, its
 *                 name in the cartridge's letters on its art team's (banner.c), unless it has a
 *                 picture of its own for them (art/banners, below); a name given here comes
 *                 before that picture.
 *   goalies.csv   team, number, name, 14 ratings (0-15: weight agility speed off_awareness
 *                 def_awareness puck_control unused1 glove_hand unused2 unused3 stick_right
 *                 stick_left glove_right glove_left) and start (1-15: how often he starts).
 *   players.csv   team, pos (F or D; C LW RW LD RD as well), number, name, 14 ratings (weight
 *                 agility speed off_awareness def_awareness shot_power checking handed_fighting
 *                 stick_handling shot_accuracy endurance shoot_or_pass pass_accuracy
 *                 aggressiveness: the roster screen's names, skater_cols below).
 *   lines.csv     team, line (SC1 SC2 CHK PP1 PP2 PK1 PK2 L8), G LD RD LW C RW X: the players'
 *                 numbers, or names where two wear one number (empty: nobody). A team with none:
 *                 made from its roster's order.
 *   colours.csv   team, palette (home away logo matchup matchup2), colours: 16 Genesis colour
 *                 words. A palette not given: the art team's.
 *   pictures.csv  team, number (or name, as in lines.csv), picture: a photo of the folder's
 *                 (art/photos/NAME.png), else the cartridge's of that label (CardMarioLemieux,
 *                 NoPicSkater1 ...).
 *   playoffs.csv  optional: rows of 16 teams, the draws a playoff starts from (a power of two of
 *                 them, at most 32); else made from the conferences.
 *   art/          optional PNGs (art.c has their sizes and colours): logos/ABBR.png (a team's
 *                 logo, in its own colours: its logo palette), banners/ABBR.png (its banners,
 *                 over its name's), ice/ABBR.png (its centre ice), photos/NAME.png; and game/
 *                 the art all the teams share (rosters.h's ART_: TitleScreenImg.png, Sprites.png
 *                 ...), each NAME.bin the asset as the game reads it, when it is not the
 *                 cartridge's (a hack's), NAME.png its picture to draw over (taken back
 *                 through its map). A team with none shows its art team's; a picture the same
 *                 as the cartridge's is the cartridge's own.
 *
 * Columns go in any order, named by the first row; others are ignored, and so are empty rows and
 * rows starting with #. Text is plain ASCII, \xNN for another byte. A roster set has 18 to 32 teams
 * (18 or more for the cup); a team 2 to 4 goalies, 9 to 15 forwards, 6 to 15 defensemen and 26
 * players at most; names as long as the screens take (LEN_ below). The
 * folder is written from the cartridge (rosters_export) or from a hack made on the community's
 * 32-team template (rosters_extract, romsrc.c: read as data, never run), and may travel as a
 * package (rosters_pack: a zip of its files, played as it is from rosters/). The game reads the roster set
 * through the tables of src/rosters.h, so it plays as the cartridge does with whatever teams it
 * has; the cartridge's own roster set, exported and built again, is the cartridge's: its tables as
 * the game reads them off the cartridge, field for field, its art the cartridge's (make
 * rosterstest). docs/ROSTERS.md tells it all for a roster set's maker. */
#include <dirent.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "native.h"
#include "rosters.h"
#include "rom.h"
#include "symbols_gen.h"
#include "mdrt/zip.h"
#ifdef _WIN32
#include <direct.h>
#define make_dir(p) _mkdir(p)
#else
#define make_dir(p) mkdir(p, 0777)
#endif

/* the screens' room: the longest of the cartridge's and of the hacks on the 32-team template */
#define LEN_CITY 17
#define LEN_ABBR 3
#define LEN_NICK 14
#define LEN_ARENA 26
#define LEN_NAME 24
#define LEN_CONF 15
#define LEN_BANNER 16
#define ROSTER 26               /* players a team: the game's roster's room (and its save RAM's) */
#define CART_TEAMS 28
#define PICTURES 32
#define ROWS_MAX 32             /* playoff draws: the password keeps 5 bits */
#define LEN_PHOTO 40
#define ICE_MAX (16 * 8 * 16 * 8)  /* (a centre ice picture: the rink round its 24 tiles) */
#define PHOTOS_MAX (ROSTERS_MAX * PICTURES)
#define OPTION_NAME 21          /* the setup screen's room for a team's name (the cartridge's longest: 18) */
#define IMAGE_MAX 0xA00000u     /* a roster set built: the cartridge and all that comes past it (to the Z80's and I/O's addresses) */
#define SRAM_START 0x200000u    /* (the save RAM's addresses on the bus: nothing the VDP takes by DMA there) */
#define SRAM_END 0x400000u

static const char *const cart_abbr[CART_TEAMS] = {
    "ANH", "BOS", "BUF", "CGY", "CHI", "DAL", "DET", "EDM", "FLA", "HFD", "LA", "MTL", "NJ", "NYI",
    "NYR", "OTW", "PHI", "PIT", "QUE", "SJ", "STL", "TB", "TOR", "VAN", "WSH", "WPG", "ASE", "ASW",
};
enum { PAL_HOME, PAL_AWAY, PAL_LOGO, PAL_MATCHUP, PAL_MATCHUP2, PALS };
static const char *const pal_names[PALS] = { "home", "away", "logo", "matchup", "matchup2" };
static const char *const line_names[8] = { "SC1", "SC2", "CHK", "PP1", "PP2", "PK1", "PK2", "L8" };
static const char *const slot_names[7] = { "G", "LD", "RD", "LW", "C", "RW", "X" };
/* a player's 14 ratings, the nibbles after his number, named as the game's roster screen labels
 * them (PAttribColumns, GAttribColumns; setplayer loads them). A skater's 8th is his hand (bit
 * 0: right handed) and fighting (bits 1-3); his 12th, which no screen shows, is his odds of
 * shooting rather than passing when the computer has the puck (spodds: the community calls it
 * roughness). A goalie's 4th is shown by no screen, and his 7th, 9th and 10th are used by
 * nothing. */
static const char *const skater_cols[14] = { "weight", "agility", "speed", "off_awareness", "def_awareness", "shot_power", "checking",
                                             "handed_fighting", "stick_handling", "shot_accuracy", "endurance", "shoot_or_pass",
                                             "pass_accuracy", "aggressiveness" };
static const char *const goalie_cols[14] = { "weight", "agility", "speed", "off_awareness", "def_awareness", "puck_control", "unused1",
                                             "glove_hand", "unused2", "unused3", "stick_right", "stick_left", "glove_right", "glove_left" };

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
    struct { uint32_t pic; uint8_t slot; int16_t photo; } pic[PICTURES];   /* the cartridge's picture, or photo (>= 0) one of the folder's */
    int pics;
    /* its art from the folder (art/): its logo (and the logo's colours), its banners, its centre ice */
    uint8_t logo[ART_LOGO * ART_LOGO], banners[ART_BANNER_H * ART_BANNER_W], ice[ICE_MAX];
    uint16_t logo_pal[16];
    int has_logo, has_banners, has_ice;
};
struct photo { char name[LEN_PHOTO + 1]; uint8_t px[ART_LOGO * ART_LOGO]; };
struct folder {
    char name[64], dir[512];
    int teams, playoff_teams, rows;
    uint8_t hiscore_at[2][2];   /* (rosters.h) */
    uint8_t seats[ROWS_MAX][16];
    struct team t[ROSTERS_MAX];
    struct photo *photo;        /* the folder's photos (art/photos) the pictures name */
    int photos;
};
/* a roster set built: its tables (rosters.h), and its art past the cartridge (art: from ROM_SIZE to
 * size; NULL when it has none of its own) */
struct tables { struct RosterTeam team[ROSTERS_MAX]; uint8_t seats[ROWS_MAX][16]; };
struct built { uint8_t *art; uint32_t size; struct rosters rosters; struct tables *tables; uint64_t hash; };
static void built_free(struct built *B) { free(B->art); free(B->tables); B->art = NULL; B->tables = NULL; }

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

const char *rosters_error(void) { return err; }

/* ---- reading: files, CSV rows, fields ------------------------------------------------------------ */

/* the file's text (NUL ended), NULL when it is not there (*missing) or cannot be read */
/* ---- where a roster set's files are: a folder, or a package (a zip of one, mdrt/zip.h) -------------- */

static struct { char path[512]; int zip; mdrt_zip z; uint8_t *bytes; } src;

static int is_package(const char *path)
{
    size_t n = strlen(path);
    const char *e = path + n - 4;
    return n > 4 && e[0] == '.' && (e[1] | 32) == 'z' && (e[2] | 32) == 'i' && (e[3] | 32) == 'p';
}

/* the file at path, whole (at most max bytes; a NUL after them) into *out: its length; 0 and *out
 * NULL when there is none; -1 when it cannot be read */
static long file_bytes(const char *path, uint8_t **out, size_t max)
{
    FILE *f = fopen(path, "rb");
    *out = NULL;
    if (!f) return 0;
    long n = fseek(f, 0, SEEK_END) ? -1 : ftell(f);
    uint8_t *b = n >= 0 && (size_t)n <= max ? malloc((size_t)n + 1) : NULL;
    if (b && (fseek(f, 0, SEEK_SET) || fread(b, 1, (size_t)n, f) != (size_t)n)) { free(b); b = NULL; }
    fclose(f);
    if (!b) return n > 0 && (size_t)n > max ? fail("%s: too big", path) : fail("%s cannot be read", path);
    b[n] = 0;
    *out = b;
    return n;
}

static void src_close(void)
{
    if (src.zip) mdrt_zip_close(&src.z);
    free(src.bytes);
    memset(&src, 0, sizeof src);
}

/* the next game's roster set as a package's bytes, under a name of its own (netplay: the session's,
 * sent by its host) */
static struct { char label[64]; uint8_t *b; size_t n; int version; } sent;   /* (kept for the run) */

/* the roster set at path (a folder, or a package: NAME.zip; or the roster set sent, by its name) is the one
 * read now: 0, -1 */
static int src_open(const char *path)
{
    src_close();
    snprintf(src.path, sizeof src.path, "%s", path);
    if (sent.b && !strcmp(path, sent.label)) {           /* (the bytes copied: src_close frees them) */
        char why[200];
        if (!(src.bytes = malloc(sent.n))) return fail("out of memory");
        memcpy(src.bytes, sent.b, sent.n);
        if (mdrt_zip_open(&src.z, src.bytes, sent.n, why, sizeof why) < 0) return fail("%s: %s", path, why);
        src.zip = 1;
        return 0;
    }
    if (!is_package(path)) return 0;
    long n = file_bytes(path, &src.bytes, MDRT_ZIP_MAX_TOTAL);
    char why[200];
    if (n <= 0) return n < 0 ? -1 : fail("no package %s", path);
    if (mdrt_zip_open(&src.z, src.bytes, (size_t)n, why, sizeof why) < 0) return fail("%s: %s", path, why);
    src.zip = 1;
    return 0;
}

/* its file name (art/logos/ANA.png ...): as file_bytes */
static long src_read(const char *name, uint8_t **out, size_t max)
{
    char path[1200];
    *out = NULL;
    if (src.zip) {
        int i = mdrt_zip_find(&src.z, name);
        if (i < 0) return 0;
        if (src.z.entry[i].size > max) return fail("%s/%s: too big", src.path, name);
        long n = mdrt_zip_read(&src.z, i, out);
        return n < 0 ? fail("%s/%s: damaged", src.path, name) : n;
    }
    snprintf(path, sizeof path, "%s/%s", src.path, name);
    return file_bytes(path, out, max);
}

static char *slurp(const char *dir, const char *name, int *missing)
{
    uint8_t *b;
    (void)dir;
    long n = src_read(name, &b, 4 << 20);
    *missing = n == 0 && !b;
    if (n < 0) { char why[700]; snprintf(why, sizeof why, "%s", err); err_file = name; err_line = 0; fail("cannot be read (%s)", why); }
    return (char *)b;
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

/* a starts with the n characters of b, case aside */
static int same_start(const char *a, const char *b, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        char x = a[i] >= 'A' && a[i] <= 'Z' ? (char)(a[i] + 32) : a[i], y = b[i] >= 'A' && b[i] <= 'Z' ? (char)(b[i] + 32) : b[i];
        if (!x || x != y) return 0;
    }
    return 1;
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

static int team_named(const struct folder *L, const char *abbr)
{
    for (int t = 0; t < L->teams; t++)
        if (same_word(L->t[t].abbr, abbr)) return t;
    return -1;
}

static int find_team(const struct folder *L, const char *abbr, int *t)
{
    if (!*abbr) return fail("no team");
    if ((*t = team_named(L, abbr)) < 0) return fail("no team %s in teams.csv", abbr);
    return 0;
}

/* the player a cell names: his number (#27 or 27), or his name where his number is shared ->
 * his roster slot: 0, -1 */
static int find_player(const struct team *T, const char *cell, const char *what, int *slot)
{
    int n, found = -1, count = 0;
    if (*cell == '#') cell++;
    if (*cell >= '0' && *cell <= '9') {
        if (num(cell, 0, 99, what, &n) < 0) return -1;
        for (int i = 0; i < T->players; i++)
            if (T->pl[i].number == n) { found = i; count++; }
        if (!count) return fail("%s %s: no number %d on the team", T->abbr, what, n);
        if (count > 1) return fail("%s %s: %d players wear %d: name him instead", T->abbr, what, count, n);
    } else {
        for (int i = 0; i < T->players; i++)
            if (same_word(T->pl[i].name, cell)) { found = i; count++; }
        if (!count) return fail("%s %s: no number or player \"%s\" on the team", T->abbr, what, cell);
        if (count > 1) return fail("%s %s: %d players named %s", T->abbr, what, count, cell);
    }
    *slot = found;
    return 0;
}

/* how a folder names the player in slot i: his number, or his name when it is shared */
static const char *player_cell(const struct team *T, int i, char *out, size_t size)
{
    int shared = 0;
    for (int k = 0; k < T->players; k++) shared += T->pl[k].number == T->pl[i].number;
    if (shared > 1) snprintf(out, size, "%s", T->pl[i].name);
    else snprintf(out, size, "%d", T->pl[i].number);
    return out;
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

static int read_ini(const char *dir, struct folder *L)
{
    int missing;
    char *t = slurp(dir, "rosters.ini", &missing);
    if (!t) return missing ? 0 : -1;
    err_file = "rosters.ini";
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
        else if (!strcmp(line, "playoff_teams")) r = num(v, 18, ROSTERS_MAX, "playoff_teams", &L->playoff_teams);
        else if (!strcmp(line, "hiscore_screen")) {
            int at[4], n = sscanf(v, "%d,%d %d,%d", &at[0], &at[1], &at[2], &at[3]);
            for (int i = 0; i < 4 && n == 4; i++) n -= at[i] < 0 || at[i] >= (i % 2 ? 28 : 40);
            if (n != 4) r = fail("hiscore_screen is two places, column,row (0-39, 0-27) each: %s", v);
            else for (int i = 0; i < 4; i++) L->hiscore_at[i / 2][i % 2] = (uint8_t)at[i];
        }
    }
    free(t);
    return r;
}

static int read_teams(char *t, struct folder *L)
{
    static const char *const cols[] = { "team", "city", "nickname", "arena", "conference", "art", "rating", "offense", "defense",
                                        "pp", "pk", "home", "away", "songs", "banner" };
    struct csv c;
    if (csv_open(&c, t, "teams.csv", cols, 15, 2) < 0) return -1;
    int r;
    while ((r = csv_row(&c)) > 0) {
        if (L->teams == ROSTERS_MAX) return fail("more than %d teams", ROSTERS_MAX);
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
    if (L->teams < 18) return fail("%d teams: a roster set has at least 18 (16 in a playoff draw, and the two playing)", L->teams);
    return 0;
}

/* goalies.csv (skaters 0) or players.csv (1) */
static int read_players(char *t, struct folder *L, int skaters)
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

static int read_lines(char *t, struct folder *L)
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
            int i;
            const char *s = get(&c, 2 + k);
            if (!*s || !strcmp(s, "#")) { T->line[line][k] = 0; continue; }
            if (find_player(T, s, slot_names[k], &i) < 0) return -1;
            T->line[line][k] = (uint8_t)(i + 1);
        }
    }
    return r;
}

static int read_colours(char *t, struct folder *L)
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

/* ---- the folder's art (art/), read: a picture there for a team or a photo is its own ---------- */

static const struct romsrc *cart_src(void)
{
    static struct romsrc s;
    if (s.b != rom_cart) romsrc_cartridge(&s, rom_cart);
    return &s;
}

/* the colours a view of shared art shows in when it has none of its own (the rink's), and
 * sprites' (the first team's home jersey), in s */
static void shared_colours(const struct romsrc *s, uint16_t screen[64], uint16_t team[16])
{
    uint32_t rink = s->asset[ART_Rinktilelist], pal = rs32(s, rink), b = rs32(s, s->table[RS_TeamList]);
    for (int k = 0; k < 64; k++) screen[k] = rs16(s, rink + pal + 2u * (uint32_t)k);
    for (int k = 0; k < 16; k++) team[k] = rs16(s, b + rs16(s, b + 2) + 2u * (uint32_t)k);
}

/* art/KIND/NAME.png, read into p (w x h): 1, 0 none, -1 (rosters_error) */
static int art_png(const char *dir, const char *kind, const char *name, mdrt_png *p, int w, int h)
{
    char rel[300], path[1200];
    uint8_t *b;
    (void)dir;
    snprintf(rel, sizeof rel, "art/%s/%s.png", kind, name);
    snprintf(path, sizeof path, "%s/%s", src.path, rel);
    long n = src_read(rel, &b, 16u << 20);
    err_file = NULL;
    err_line = 0;
    if (n <= 0 && !b) return n < 0 ? -1 : 0;
    int r = art_load_bytes(path, b, (size_t)n, p, w, h);
    free(b);
    if (r < 0) return fail("%s", art_error());
    return 1;
}

/* the photo art/photos/NAME.png (its 8 shades: colours 5 to 12 of the photos' palette, an indexed
 * PNG's indexes, or each colour as the shade nearest its brightness) into px: 1, 0 none, -1 */
static int read_photo(const char *dir, const char *name, uint8_t px[ART_LOGO * ART_LOGO])
{
    mdrt_png p;
    int r = art_png(dir, "photos", name, &p, ART_LOGO, ART_LOGO);
    if (r <= 0) return r;
    uint16_t pal[16];
    art_photo_palette(cart_src(), pal);
    int by_index = p.index != NULL;
    for (int i = 0; by_index && i < ART_LOGO * ART_LOGO; i++) by_index = p.index[i] >= 5 && p.index[i] <= 12;
    for (int i = 0; i < ART_LOGO * ART_LOGO; i++) {
        if (by_index) { px[i] = p.index[i]; continue; }
        uint32_t c = p.rgba[i];
        long lum = (long)(c >> 16 & 0xFF) * 299 + (long)(c >> 8 & 0xFF) * 587 + (long)(c & 0xFF) * 114, best = -1;
        for (int k = 5; k <= 12; k++) {
            uint32_t q = art_rgb(pal[k]);
            long d = labs((long)(q >> 16 & 0xFF) * 299 + (long)(q >> 8 & 0xFF) * 587 + (long)(q & 0xFF) * 114 - lum);
            if (best < 0 || d < best) { best = d; px[i] = (uint8_t)k; }
        }
    }
    mdrt_png_free(&p);
    return 1;
}

/* the photo named name (the folder's, read now if it is not yet) or the cartridge's picture of that
 * label: *photo its place in L (or -1), *pic the cartridge's (or 0). 0, -1 */
static int find_photo(const char *dir, struct folder *L, const char *name, int *photo, uint32_t *pic)
{
    *photo = -1;
    *pic = 0;
    for (int k = 0; k < L->photos; k++)
        if (!strcmp(L->photo[k].name, name)) { *photo = k; return 0; }
    if (strlen(name) > LEN_PHOTO || strchr(name, '/') || strchr(name, '\\') || name[0] == '.') return fail("picture: \"%s\" is no name for a photo", name);
    if (!L->photo && !(L->photo = calloc(PHOTOS_MAX, sizeof *L->photo))) return fail("out of memory");
    if (L->photos == PHOTOS_MAX) return fail("more than %d photos", PHOTOS_MAX);
    struct photo *P = &L->photo[L->photos];
    int r = read_photo(dir, name, P->px);
    if (r < 0) return -1;
    if (r > 0) {
        snprintf(P->name, sizeof P->name, "%s", name);
        *photo = L->photos++;
        return 0;
    }
    if (!(*pic = picture_named(name)))
        return fail("picture: \"%s\" is neither a photo of the folder's (art/photos/%s.png) nor one of the cartridge's (CardMarioLemieux, NoPicSkater1 ...)", name, name);
    return 0;
}

static int read_pictures(char *t, struct folder *L, const char *dir)
{
    static const char *const cols[] = { "team", "number", "picture" };
    struct csv c;
    if (csv_open(&c, t, "pictures.csv", cols, 3, 3) < 0) return -1;
    int r;
    while ((r = csv_row(&c)) > 0) {
        int tm, i, photo;
        uint32_t pic;
        if (find_team(L, get(&c, 0), &tm) < 0) return -1;
        struct team *T = &L->t[tm];
        if (find_player(T, get(&c, 1), "number", &i) < 0) return -1;
        int line = err_line;
        if (find_photo(dir, L, get(&c, 2), &photo, &pic) < 0) {
            if (!err_file) { char why[700]; snprintf(why, sizeof why, "%s", err); err_file = "pictures.csv"; err_line = line; return fail("%s", why); }
            return -1;
        }
        err_file = "pictures.csv";
        err_line = line;
        if (T->pics == PICTURES) return fail("%s: more than %d pictures", T->abbr, PICTURES);
        T->pic[T->pics].pic = pic;
        T->pic[T->pics].photo = (int16_t)photo;
        T->pic[T->pics++].slot = (uint8_t)i;
    }
    return r;
}

/* each team's logo, banners and centre ice from art/ (logos/ABBR.png, banners/ABBR.png,
 * ice/ABBR.png), when the folder has them: 0, -1 */
static int read_art(const char *dir, struct folder *L)
{
    const struct romsrc *cs = cart_src();
    uint16_t pal[ART_BANNER_COLOURS], ice_pal[16];
    uint8_t px[ICE_MAX];
    int iw = 0, ih = 0;
    err_file = NULL;
    err_line = 0;
    if (art_banners(cs, 0, px, pal) < 0 || art_ice(cs, 0, px, &iw, &ih, ice_pal) < 0) return fail("the cartridge's art cannot be read");
    for (int t = 0; t < L->teams; t++) {
        struct team *T = &L->t[t];
        mdrt_png p;
        int r;
        char path[1200];
        if ((r = art_png(dir, "logos", T->abbr, &p, ART_LOGO, ART_LOGO)) < 0) return -1;
        if (r) {
            snprintf(path, sizeof path, "%s/art/logos/%s.png", dir, T->abbr);
            r = art_own_palette(path, &p, T->logo_pal, T->logo);
            mdrt_png_free(&p);
            if (r < 0) return fail("%s", art_error());
            T->has_logo = 1;
            memcpy(T->pal[PAL_LOGO], T->logo_pal, sizeof T->logo_pal);   /* (its colours: the logo's, whatever colours.csv says) */
            T->has_pal[PAL_LOGO] = 1;
        }
        if ((r = art_png(dir, "banners", T->abbr, &p, ART_BANNER_W, ART_BANNER_H)) < 0) return -1;
        if (r) {
            snprintf(path, sizeof path, "%s/art/banners/%s.png", dir, T->abbr);
            r = art_indexes(path, &p, 0, 0, ART_BANNER_W, 16, pal, 64, 0, T->banners) < 0
                || art_indexes(path, &p, 0, 16, ART_BANNER_W, 16, pal + ART_BANNER_SETUP, 16, ART_BANNER_SETUP, T->banners + 16 * ART_BANNER_W) < 0 ? -1 : 0;
            mdrt_png_free(&p);
            if (r < 0) return fail("%s", art_error());
            for (int c = 0; c < 22; c++) {                /* (a cell of the top: the colours of one line) */
                int line = -1;
                for (int i = 0; i < 64; i++) {
                    int v = T->banners[(c / 11 * 8 + i / 8) * ART_BANNER_W + c % 11 * 8 + i % 8];
                    if (v % 16 && line >= 0 && v / 16 != line) return fail("%s: the cell at %d, %d has colours of two lines (%d and %d)", path, c % 11 * 8, c / 11 * 8, line * 16, v / 16 * 16);
                    if (v % 16) line = v / 16;
                }
            }
            T->has_banners = 1;
        }
        if ((r = art_png(dir, "ice", T->abbr, &p, iw, ih)) < 0) return -1;
        if (r) {
            snprintf(path, sizeof path, "%s/art/ice/%s.png", dir, T->abbr);
            r = art_indexes(path, &p, 0, 0, iw, ih, ice_pal, 16, 0, T->ice);
            mdrt_png_free(&p);
            if (r < 0) return fail("%s", art_error());
            T->has_ice = 1;
        }
    }
    return 0;
}

static int read_playoffs(char *t, struct folder *L)
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
            if (tm >= L->playoff_teams) return fail("%s does not play for the cup (playoff_teams in rosters.ini)", L->t[tm].abbr);
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
 * before any again), the order random from the roster set's teams */
static void make_draws(struct folder *L)
{
    uint32_t seed = 2166136261u;
    for (int t = 0; t < L->teams; t++)
        for (const char *s = L->t[t].abbr; *s; s++) seed = (seed ^ (uint8_t)*s) * 16777619u;
    seed |= 1;
    int pool[2][ROSTERS_MAX], n[2] = { 0, 0 }, sides = 2;
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
    int deck[2][ROSTERS_MAX], left[2] = { 0, 0 };
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

static void free_lg(struct folder *L) { if (L) free(L->photo); free(L); }

/* a folder's name without the slashes after it */
static void clean_dir(char *out, size_t size, const char *dir)
{
    snprintf(out, size, "%s", dir);
    for (size_t n = strlen(out); n > 1 && out[n - 1] == '/'; n--) out[n - 1] = 0;
}

/* the folder into L (allocated), its pictures and art too when art (read against the cartridge's,
 * which the window's menu has not loaded yet when it lists the roster sets): 0, or -1 with rosters_error() */
static int read_rosters(const char *folder, struct folder **out, int art)
{
    char dir[512];
    clean_dir(dir, sizeof dir, folder);
    struct folder *L = calloc(1, sizeof *L);
    char *t[7] = { NULL };
    static const char *const files[7] = { "teams.csv", "goalies.csv", "players.csv", "lines.csv", "colours.csv", "pictures.csv", "playoffs.csv" };
    int missing, r = -1;
    *out = NULL;
    err_file = NULL;
    err_line = 0;
    if (!L) return fail("out of memory");
    snprintf(L->dir, sizeof L->dir, "%s", dir);
    memcpy(L->hiscore_at, rosters_cartridge.hiscore_at, sizeof L->hiscore_at);
    if (src_open(dir) < 0) { free_lg(L); return -1; }
    const char *base = strrchr(dir, '/');
    snprintf(L->name, sizeof L->name, "%.63s", base && base[1] ? base + 1 : dir);
    if (is_package(L->name)) L->name[strlen(L->name) - 4] = 0;
    if (read_ini(dir, L) < 0) goto done;
    for (int i = 0; i < 7; i++) {
        t[i] = slurp(dir, files[i], &missing);
        if (!t[i] && !missing) goto done;
        if (!t[i] && i < 3) { err_file = files[i]; err_line = 0; fail("not in %s", dir); goto done; }
    }
    if (read_teams(t[0], L) < 0) goto done;
    if (!L->playoff_teams) L->playoff_teams = L->teams;
    err_file = "rosters.ini";
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
    if (art && t[5] && read_pictures(t[5], L, dir) < 0) goto done;   /* (its pictures and art: read against the cartridge's) */
    if (art && read_art(dir, L) < 0) goto done;
    if (t[6]) { if (read_playoffs(t[6], L) < 0) goto done; }
    else make_draws(L);
    r = 0;
done:
    for (int i = 0; i < 7; i++) free(t[i]);
    if (r < 0) free_lg(L);
    else *out = L;
    return r;
}

/* ---- a ROM's roster set: the cartridge's, or a hack's (romsrc.c) ------------------------------------ */

static void src_string(const struct romsrc *s, uint32_t *a, char *out, int max)
{
    uint16_t n = rs16(s, *a);
    int k = 0;
    for (uint32_t i = 2; i < n && k < max; i++)
        if (rs8(s, *a + i)) out[k++] = (char)rs8(s, *a + i);
    out[k] = 0;
    *a += n;
}

/* team t of the ROM s, as a roster set folder has it (its art: the cartridge's team t; its pictures
 * at their places in s) */
static void src_team(const struct romsrc *s, int t, struct team *T)
{
    uint32_t b = rs32(s, s->table[RS_TeamList] + 4u * (uint32_t)t), sr = b + rs16(s, b + 8), so = rs16(s, b + rs16(s, b + 10));
    memset(T, 0, sizeof *T);
    T->art = t;
    for (int k = 0; k < 16; k++) {
        T->pal[PAL_HOME][k] = rs16(s, b + rs16(s, b + 2) + 2u * (uint32_t)k);
        T->pal[PAL_AWAY][k] = rs16(s, b + rs16(s, b + 2) + 32 + 2u * (uint32_t)k);
        T->pal[PAL_LOGO][k] = rs16(s, s->table[RS_TeamLogoPalettes] + 32u * (uint32_t)t + 2u * (uint32_t)k);
        T->pal[PAL_MATCHUP][k] = rs16(s, s->table[RS_TeamPalettes] + 64u * (uint32_t)t + 2u * (uint32_t)k);
        T->pal[PAL_MATCHUP2][k] = rs16(s, s->table[RS_TeamPalettes] + 64u * (uint32_t)t + 32 + 2u * (uint32_t)k);
    }
    for (int p = 0; p < PALS; p++) T->has_pal[p] = 1;
    T->offense = rs8(s, sr) >> 4 & 7;
    T->defense = rs8(s, sr) & 7;
    T->pk = rs8(s, sr + 1) >> 4;
    T->pp = rs8(s, sr + 1) & 15;
    T->home = rs8(s, sr + 2) >> 4;
    T->away = rs8(s, sr + 2) & 15;
    T->forwards = rs8(s, sr + 3) >> 4;
    T->rating = rs8(s, s->table[RS_TeamRatings] + (uint32_t)t);
    for (int k = 0; k < 6; k++) T->songs[k] = rs8(s, s->table[RS_TeamSongs] + 6u * (uint32_t)t + (uint32_t)k);
    for (int l = 0; l < 8; l++)
        for (int k = 0; k < 7; k++) T->line[l][k] = rs8(s, b + rs16(s, b + 6) + 8u * (uint32_t)l + (uint32_t)k);
    T->has_lines = 0xFF;
    uint32_t p = b + rs16(s, b);
    while (rs16(s, p) != 2 && T->players < ROSTER) {
        struct player *P = &T->pl[T->players];
        src_string(s, &p, P->name, LEN_NAME);
        P->number = (uint8_t)((rs8(s, p) >> 4) * 10 + (rs8(s, p) & 15));
        for (int k = 0; k < 14; k++) P->nib[k] = (uint8_t)(rs8(s, p + 1 + (uint32_t)k / 2) >> (k & 1 ? 0 : 4) & 15);
        p += 8;
        T->players++;
    }
    /* goalies: as the game counts them (GetGoalieCount: the .sodds word's nibbles to its last
     * one not 0) */
    uint16_t w = so;
    do T->pl[T->goalies++].start = (uint8_t)(w >> 12); while ((w = (uint16_t)(w << 4)) != 0);
    for (int i = 0; i < T->players; i++) T->pl[i].pos = i < T->goalies ? 'G' : i < T->goalies + T->forwards ? 'F' : 'D';
    p += 2;
    src_string(s, &p, T->city, LEN_CITY);
    src_string(s, &p, T->abbr, LEN_ABBR);
    src_string(s, &p, T->nickname, LEN_NICK);
    src_string(s, &p, T->arena, LEN_ARENA);
    for (uint32_t a = rs32(s, s->table[RS_FeaturedPictures] + 4u * (uint32_t)t), k = 0; rs32(s, a) && T->pics < PICTURES && k < 64; a += 6, k++) {
        T->pic[T->pics].pic = rs32(s, a);
        T->pic[T->pics].photo = -1;
        T->pic[T->pics++].slot = (uint8_t)rs16(s, a + 4);
    }
    for (int r = 0; r < s->rows && !T->conference[0]; r++)   /* the side of the draws it plays on */
        for (int k = 0; k < 16; k++)
            if (rs8(s, s->table[RS_playoffseats] + 16u * (uint32_t)r + (uint32_t)k) == t) { strcpy(T->conference, k < 8 ? "east" : "west"); break; }
}

/* the cartridge's team c */
static void cart_team(const uint8_t *c, int t, struct team *T)
{
    struct romsrc s;
    romsrc_cartridge(&s, c);
    src_team(&s, t, T);
}

/* a word of a (letters only, case aside) is all of b */
static int has_word(const char *a, const char *b)
{
    size_t n = strlen(b);
    for (const char *p = a; *p; p++)
        if ((p == a || p[-1] == ' ') && same_start(p, b, n) && (!p[n] || p[n] == ' ')) return 1;
    return 0;
}

/* the cartridge's team whose art a hack's team T shows until it has its own: the same club (its
 * nickname, or its city where only one club plays), the club it was before it moved, else the
 * one whose home jersey is the nearest */
static int art_of(const struct team *T, const struct team cart[CART_TEAMS])
{
    static const struct { const char *city, *was; } moved_from[] = {
        { "Carolina", "HFD" }, { "Colorado", "QUE" }, { "Arizona", "WPG" }, { "Phoenix", "WPG" }, { "Utah", "WPG" },
        { "Minnesota", "DAL" }, { "Atlanta", "WPG" },
    };
    const char *nick_last = strrchr(T->nickname, ' ');
    nick_last = nick_last ? nick_last + 1 : T->nickname;
    for (int k = 0; k < CART_TEAMS - 2; k++)
        if (*nick_last && has_word(cart[k].nickname, nick_last)) return k;
    int only = -1, count = 0;
    for (int k = 0; k < CART_TEAMS - 2; k++)
        if (same_start(T->city, cart[k].city, strlen(cart[k].city)) && (!T->city[strlen(cart[k].city)] || T->city[strlen(cart[k].city)] == ' ')) { only = k; count++; }
    if (count == 1) return only;
    for (size_t i = 0; i < sizeof moved_from / sizeof moved_from[0]; i++)
        if (same_start(T->city, moved_from[i].city, strlen(moved_from[i].city)))
            for (int k = 0; k < CART_TEAMS; k++)
                if (!strcmp(cart_abbr[k], moved_from[i].was)) return k;
    int best = 0;
    long best_d = -1;
    for (int k = 0; k < CART_TEAMS - 2; k++) {
        long d = 0;
        for (int i = 1; i < 16; i++)
            for (int sh = 1; sh < 12; sh += 4) {
                long x = (long)(T->pal[PAL_HOME][i] >> sh & 7) - (long)(cart[k].pal[PAL_HOME][i] >> sh & 7);
                d += x * x;
            }
        if (best_d < 0 || d < best_d) { best = k; best_d = d; }
    }
    return best;
}

/* ---- building: the roster set's tables, and the cartridge with its art past the end ------------------- */

struct buf { uint8_t *b; size_t n, cap; int bad; };

static uint16_t c16(const uint8_t *c, uint32_t a) { return (uint16_t)(c[a] << 8 | c[a + 1]); }
static uint32_t c32(const uint8_t *c, uint32_t a) { return (uint32_t)c16(c, a) << 16 | c16(c, a + 2); }

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
static uint32_t at(const struct buf *b) { return (uint32_t)b->n; }

static uint64_t fnv64(uint64_t h, const void *p, size_t n)
{
    for (const uint8_t *s = p; n--; s++) h = (h ^ *s) * 0x100000001B3ull;
    return h;
}

/* ---- the tables the game reads (rosters.h) ------------------------------------------------------ */

/* text s as a String, as the cartridge keeps one (put_string's) */
static void roster_string(struct RosterText *d, const char *s)
{
    size_t n = strlen(s);
    unsigned len = (unsigned)((2 + n + 1) & ~(size_t)1);
    memset(d, 0, sizeof *d);
    d->s[0] = (uint8_t)(len >> 8);
    d->s[1] = (uint8_t)len;
    memcpy(d->s + 2, s, n);
}

/* team t of L as the game reads it (what the cartridge's block and tables hold of a team; its
 * pictures at photo_at for the folder's photos, its logo and centre ice at logo and ice) */
static void roster_team_of(const struct folder *L, int t, const uint32_t *photo_at, uint32_t logo, uint32_t ice, struct RosterTeam *G)
{
    const struct team *T = &L->t[t];
    memset(G, 0, sizeof *G);
    G->data = RS_DATA(t);
    roster_string(&G->city, T->city);
    roster_string(&G->abbr, T->abbr);
    roster_string(&G->nickname, T->nickname);
    roster_string(&G->arena, T->arena);
    memcpy(G->colours[0], T->pal[PAL_HOME], sizeof G->colours[0]);
    memcpy(G->colours[1], T->pal[PAL_AWAY], sizeof G->colours[1]);
    memcpy(G->matchup[0], T->pal[PAL_MATCHUP], sizeof G->matchup[0]);
    memcpy(G->matchup[1], T->pal[PAL_MATCHUP2], sizeof G->matchup[1]);
    G->offense = (uint8_t)T->offense;
    G->defense = (uint8_t)T->defense;
    G->pp = (uint8_t)T->pp;
    G->pk = (uint8_t)T->pk;
    G->home = (uint8_t)T->home;
    G->away = (uint8_t)T->away;
    G->forwards = (uint8_t)T->forwards;
    G->defensemen = (uint8_t)(T->players - T->goalies - T->forwards);
    for (int k = 0; k < T->goalies; k++) G->starts |= (uint16_t)(T->pl[k].start << (12 - 4 * k));
    uint16_t w = G->starts;                               /* (as the game counts them) */
    do G->goalies++; while ((w = (uint16_t)(w << 4)) != 0);
    for (int l = 0; l < 8; l++) memcpy(G->lines[l], T->line[l], 7);
    G->players = (uint8_t)T->players;
    for (int i = 0; i < T->players; i++) {
        const struct player *P = &T->pl[i];
        roster_string(&G->player[i].name, P->name);
        G->player[i].number = (uint8_t)((P->number / 10) << 4 | P->number % 10);
        memcpy(G->player[i].rating, P->nib, sizeof G->player[i].rating);
    }
    G->rating = (uint8_t)T->rating;
    for (int k = 0; k < 6; k++) G->songs[k] = (uint8_t)T->songs[k];
    G->records = (uint16_t)(t < CART_TEAMS ? 26u * (unsigned)t : ROSTERS_SRAM_PLAYERS / 4 + 26u * (unsigned)(t - CART_TEAMS));   /* (save RAM: the cartridge's places, the rest where it has room) */
    G->pictures = (uint8_t)T->pics;
    for (int i = 0; i < T->pics; i++) {
        G->picture[i].pic = T->pic[i].photo >= 0 ? photo_at[T->pic[i].photo] : T->pic[i].pic;
        G->picture[i].slot = T->pic[i].slot;
    }
    /* the setup screen's name: its city, its whole name when another has its city (and the menu
     * has room: else its nickname) */
    int shared = 0;
    for (int k = 0; k < L->teams; k++) shared += k != t && same_word(L->t[k].city, T->city);
    char whole[LEN_CITY + LEN_NICK + 2];
    snprintf(whole, sizeof whole, "%s %s", T->city, T->nickname);
    roster_string(&G->option_name, !shared ? T->city : strlen(whole) <= OPTION_NAME ? whole : T->nickname);
    G->logo = logo;
    G->ice = ice;
    memcpy(G->logo_colours, T->pal[PAL_LOGO], sizeof G->logo_colours);
}

/* the tables' part of a roster set's checksum, field by field */
static uint64_t tables_hash(uint64_t h, const struct RosterTeam *teams, int n, const uint8_t seats[][16], int rows)
{
    for (int t = 0; t < n; t++) {
        const struct RosterTeam *G = &teams[t];
        const struct RosterText *text[] = { &G->city, &G->abbr, &G->nickname, &G->arena, &G->option_name };
        for (size_t k = 0; k < sizeof text / sizeof text[0]; k++) h = fnv64(h, text[k]->s, sizeof text[k]->s);
        uint8_t b[5 * 16 * 2 + 8];
        for (int k = 0; k < 80; k++) {
            uint16_t c = k < 32 ? G->colours[k / 16][k % 16] : k < 64 ? G->matchup[(k - 32) / 16][k % 16] : G->logo_colours[k - 64];
            b[2 * k] = (uint8_t)(c >> 8);
            b[2 * k + 1] = (uint8_t)c;
        }
        for (int k = 0; k < 4; k++) {
            b[160 + k] = (uint8_t)(G->logo >> (24 - 8 * k));
            b[164 + k] = (uint8_t)(G->ice >> (24 - 8 * k));
        }
        h = fnv64(h, b, sizeof b);
        uint8_t v[] = { G->offense, G->defense, G->pp, G->pk, G->home, G->away, G->forwards, G->defensemen, G->goalies,
                        (uint8_t)(G->starts >> 8), (uint8_t)G->starts, G->players, G->rating, (uint8_t)(G->records >> 8),
                        (uint8_t)G->records, G->pictures };
        h = fnv64(fnv64(fnv64(h, v, sizeof v), G->lines, sizeof G->lines), G->songs, sizeof G->songs);
        for (int i = 0; i < G->players; i++)
            h = fnv64(fnv64(fnv64(h, G->player[i].name.s, sizeof G->player[i].name.s), &G->player[i].number, 1),
                      G->player[i].rating, sizeof G->player[i].rating);
        for (int i = 0; i < G->pictures; i++) {
            uint8_t p[6] = { (uint8_t)(G->picture[i].pic >> 24), (uint8_t)(G->picture[i].pic >> 16), (uint8_t)(G->picture[i].pic >> 8),
                             (uint8_t)G->picture[i].pic, (uint8_t)(G->picture[i].slot >> 8), (uint8_t)G->picture[i].slot };
            h = fnv64(h, p, sizeof p);
        }
    }
    return fnv64(h, seats, 16u * (size_t)rows);
}

/* past the save RAM's addresses when the next size bytes would reach them: the VDP's DMA reads
 * the bus, where $200000-$3FFFFF is the save RAM (the game's own reads go anywhere) */
static void room(struct buf *b, uint32_t size)
{
    uint32_t a = at(b);
    if (a < SRAM_END && a + size > SRAM_START) {
        need(b, SRAM_END - a);
        if (!b->bad) { memset(b->b + b->n, 0, SRAM_END - a); b->n = SRAM_END; }
    }
}

/* pad so the next size bytes do not cross a 128 KB boundary (the VDP's DMA does not cross one),
 * nor reach the save RAM's addresses */
static void dma_align(struct buf *b, uint32_t size)
{
    room(b, size);
    uint32_t a = at(b);
    if ((a >> 17) != ((a + size - 1) >> 17))
        while (at(b) & 0x1FFFF && !b->bad) put8(b, 0);
}

/* set's half (0 on top, 1 below) of a team's own banners (88 x 32) as banner.c's pixels */
static void half(const uint8_t *own, int set, uint8_t px[16][88])
{
    for (int y = 0; y < 16; y++)
        for (int x = 0; x < 88; x++) {
            int v = own[(16 * set + y) * ART_BANNER_W + x];
            px[y][x] = (uint8_t)(set ? v - ART_BANNER_SETUP : v % 16);
        }
}

/* the line of the top banner's cell c (its colours': 1 when they are all 0) */
static unsigned cell_line(const uint8_t *own, int c)
{
    for (int i = 0; i < 64; i++) {
        int v = own[(c / 11 * 8 + i / 8) * ART_BANNER_W + c % 11 * 8 + i % 8];
        if (v % 16) return (unsigned)(v / 16);
    }
    return own[(c / 11 * 8) * ART_BANNER_W + c % 11 * 8] / 16;
}

/* the team name banners, when a team has its own (own[t]: both, 88 x 32): Teamblocksmap again
 * with its banners' tiles and map rows after the cartridge's 28 (plain tiles, as the game loads
 * them for the playoff tree), and every team's TeamBitmaps banner (the setup screen draws them
 * one at a time) */
static void put_banners(struct buf *b, struct folder *L, const uint8_t *c, uint8_t *const own_px[], struct rosters *rs)
{
    static uint8_t tiles[22 * 32];
    uint8_t px[16][88];
    int count, own = 0;
    uint32_t map;
    const uint8_t *cart_tiles = banner_set(BANNER_BLOCKS, &count, &map);
    uint32_t size = 8 + 2 + 32u * (uint32_t)count + 22u * 32 * ROSTERS_MAX;
    dma_align(b, size);
    uint32_t tb = rs->table[RS_Teamblocksmap] = at(b);
    put32(b, SYM_Teamblocksmap + c32(c, SYM_Teamblocksmap) - tb);   /* (its palette: the cartridge's) */
    put32(b, 0);
    for (int t = 0; t < L->teams; t++) own += own_px[t] != NULL;
    /* of the cartridge's set, the tiles the teams without banners of their own show (their art
     * teams' rows'), in order: its whole 501 beside 22 a team of their own run past the room the
     * playoff screen loads them all into (into its background's map: the hacks' 32 take 704) */
    static uint8_t needed[2048];
    static uint16_t remap[2048];
    int used = 0;
    memset(needed, 0, sizeof needed);
    for (int t = 0; t < L->teams; t++)
        for (int i = 0; !own_px[t] && i < 22; i++) needed[c16(c, map + 4 + 2u * (uint32_t)(L->t[t].art * 22 + i)) & 0x7FF] = 1;
    for (int i = 0; i < count; i++) remap[i] = needed[i] ? (uint16_t)used++ : 0;
    put16(b, (unsigned)(used + 22 * own));                /* (plain: DMA'd as they are) */
    for (int i = 0; i < count; i++) if (needed[i]) put(b, cart_tiles + 32 * i, 32);
    for (int t = 0, k = 0; t < L->teams; t++) {
        rs->block[t] = (uint8_t)L->t[t].art;
        if (!own_px[t]) continue;
        half(own_px[t], 0, px);
        banner_tiles(px, tiles);
        rs->block[t] = (uint8_t)(CART_TEAMS + k++);
        rs->block_tiles[t] = at(b);
        put(b, tiles, sizeof tiles);
    }
    set32(b, tb + 4, at(b) - tb);
    put16(b, 11);
    put16(b, (unsigned)(2 * (CART_TEAMS + own)));
    for (int i = 0; i < 11 * 2 * CART_TEAMS; i++) {       /* (the cartridge's rows, at their tiles' places now) */
        uint16_t w = c16(c, map + 4 + 2u * (uint32_t)i);
        put16(b, (w & 0xF800u) | remap[(w & 0x7FF) < count ? (w & 0x7FF) : 0]);
    }
    for (int t = 0, k = 0; t < L->teams; t++) {
        if (!own_px[t]) continue;
        for (int i = 0; i < 22; i++)                      /* (its art team's priority, its own lines, no flips) */
            put16(b, (c16(c, map + 4 + 2u * (uint32_t)(L->t[t].art * 22 + i)) & 0x8000) | cell_line(own_px[t], i) << 13 | (unsigned)(used + 22 * k + i));
        k++;
    }
    dma_align(b, 22u * 32 * (uint32_t)L->teams);
    for (int t = 0; t < L->teams; t++) {
        if (own_px[t]) half(own_px[t], 1, px);
        else banner_pixels(BANNER_BITMAPS, L->t[t].art, NULL, px);
        banner_tiles(px, tiles);
        rs->banner_tiles[t] = at(b);
        put(b, tiles, sizeof tiles);
    }
    banner_set(BANNER_BITMAPS, &count, &map);
    rs->table[RS_BannerSlotMap] = at(b);
    put16(b, 11);
    put16(b, 2);
    for (int i = 0; i < 22; i++) put16(b, (c16(c, map + 4 + 2u * (uint32_t)i) & 0xE000) | (unsigned)i);
    /* the setup screen's set: 22 tiles (each team's are DMA'd into them), not the cartridge's
     * 398 loaded twice (LoadSetupTiles): the room the hacks make for a setup screen of their own
     * (the NHL 2026 edition's: 1152 tiles) */
    uint32_t pal = SYM_TeamBitmaps + c32(c, SYM_TeamBitmaps), first = rs->banner_tiles[0];
    dma_align(b, 8 + 2 + 22 * 32 + 128 + 4 + 44);
    rs->asset[ART_TeamBitmaps] = at(b);
    put32(b, 8 + 2 + 22 * 32);
    put32(b, 8 + 2 + 22 * 32 + 128);
    put16(b, 22);
    if (!b->bad) put(b, b->b + first, 22 * 32);           /* (the first team's: any will do) */
    put(b, c + pal, 128);
    put16(b, 11);
    put16(b, 2);
    for (int i = 0; i < 22; i++) put16(b, (c16(c, map + 4 + 2u * (uint32_t)i) & 0xE000) | (unsigned)i);
}

/* the shared art of the folder (art/game): each asset's bytes from NAME.bin (a hack's: the
 * cartridge's otherwise), made again from NAME.png when that is not the same picture; past the
 * cartridge when they are not its own, read through rosters.asset (ART()). Their tiles go where
 * the VDP's DMA takes them whole (none across 128 KB; the players' sprites, taken a frame at a
 * time, where they are in their bank) */
static int shared_build(struct buf *b, struct folder *L, struct rosters *rs)
{
    const struct romsrc *cs = cart_src();
    uint16_t screen[64], team[16];
    shared_colours(cs, screen, team);
    for (int i = 0; i < art_shared(); i++) {
        int k = art_shared_asset(i), tiles_off, tk = art_shared_tiles(i, &tiles_off);
        char path[1200];
        uint8_t *blob, *made = NULL;
        struct romsrc bs = *cs;
        uint32_t a = cs->asset[k];
        snprintf(path, sizeof path, "art/game/%s.bin", art_shared_name(i));
        long n = src_read(path, &blob, 0x80000);
        if (n < 0) return -1;
        if (n == 0 && blob) { free(blob); blob = NULL; }
        if (n) {
            bs.b = blob;
            bs.size = (uint32_t)n;
            a = 0;
            if (!art_shared_size(&bs, i, 0)) { free(blob); return fail("art/game/%s.bin is not the game's %s", art_shared_name(i), art_shared_name(i)); }
        }
        if (k == ART_Rinktilelist)                        /* (the rink's colours: those of the pictures after it with none) */
            for (int c = 0; c < 64; c++) screen[c] = rs16(&bs, a + rs32(&bs, a) + 2u * (uint32_t)c);
        const uint8_t *bytes = n ? blob : NULL;
        size_t size = n ? (size_t)n : 0;
        mdrt_png p;
        int r = art_png(L->dir, "game", art_shared_name(i), &p, 0, 0);
        snprintf(path, sizeof path, "%s/art/game/%s.png", src.path, art_shared_name(i));
        if (r < 0) { free(blob); return -1; }
        if (r) {
            struct art_view base, v;
            memcpy(team, L->t[0].pal[PAL_HOME], sizeof team);
            if (art_shared_view(&bs, i, a, screen, team, &base) < 0) { mdrt_png_free(&p); free(blob); return fail("%s cannot be drawn", art_shared_name(i)); }
            r = art_shared_read(path, &p, i, &base, &v);
            mdrt_png_free(&p);
            if (r == 0 && !art_view_same(&base, &v)) {
                size = art_shared_make(&bs, i, a, &v, &made);
                if (!size) r = -1;
                bytes = made;
            }
            art_view_free(&base);
            art_view_free(&v);
            if (r < 0) { free(blob); free(made); return fail("%s", art_error()); }
        }
        if (bytes) {
            if (k == ART_Sprites) {                       /* (where they are in their 128 KB bank) */
                room(b, (uint32_t)size + 0x20000);
                while ((at(b) & 0x1FFFF) != (SYM_Sprites & 0x1FFFF) && !b->bad) put8(b, 0);
            }
            else dma_align(b, (uint32_t)size);
            rs->asset[k] = at(b);
            put(b, bytes, size);
            even(b);
            if (tk >= 0) rs->asset[tk] = rs->asset[k] + (uint32_t)tiles_off;
        }
        free(blob);
        free(made);
    }
    return 0;
}

/* L built on the cartridge c: the art team's for what the folder leaves out */
static int build(struct folder *L, const uint8_t *c, struct built *B)
{
    struct buf b = { 0 };
    static uint8_t made[ROSTERS_MAX][ART_BANNER_H * ART_BANNER_W];
    uint8_t *own_px[ROSTERS_MAX] = { NULL };
    int own = 0;
    memset(B, 0, sizeof *B);
    if (banner_prepare(c) < 0) return fail("the cartridge's team banners cannot be read");
    const struct romsrc *cs = cart_src();
    for (int t = 0; t < L->teams; t++) {                  /* (what a team leaves to its art team's) */
        struct team *T = &L->t[t], A;
        cart_team(c, T->art, &A);
        /* its banners: the name in its banner column; else its picture (art/banners), unless that
         * is its art team's as it is; else its city's name, when the city is its own (and fits) */
        char why[200];
        int picture = 0;
        if (T->has_banners) {
            uint16_t pal[ART_BANNER_COLOURS];
            if (art_banners(cs, T->art, made[t], pal) < 0) return fail("the cartridge's team banners cannot be read");
            picture = memcmp(made[t], T->banners, sizeof made[t]) != 0;
        }
        if (!T->banner[0] && !picture && !same_word(T->city, A.city) && strlen(T->city) <= LEN_BANNER) {
            for (size_t k = 0; k <= strlen(T->city); k++)
                T->banner[k] = T->city[k] >= 'a' && T->city[k] <= 'z' ? (char)(T->city[k] - 32) : T->city[k];
            if (banner_check(T->banner, why, sizeof why) < 0) T->banner[0] = 0;
        }
        if (!T->banner[0] && picture) {
            own_px[t] = T->banners;
            own++;
        } else if (T->banner[0]) {
            if (banner_check(T->banner, why, sizeof why) < 0) { err_file = "teams.csv"; err_line = 0; return fail("%s: %s", T->abbr, why); }
            uint8_t px[16][88];
            uint16_t pal[ART_BANNER_COLOURS];
            if (art_banners(cs, T->art, made[t], pal) < 0) return fail("the cartridge's team banners cannot be read");   /* (its cells' lines) */
            for (int set = 0; set < 2; set++) {
                banner_pixels(set ? BANNER_BITMAPS : BANNER_BLOCKS, T->art, T->banner, px);
                for (int y = 0; y < 16; y++)
                    for (int x = 0; x < 88; x++) {
                        uint8_t *v = &made[t][(16 * set + y) * ART_BANNER_W + x];
                        *v = (uint8_t)(set ? ART_BANNER_SETUP + px[y][x] : *v / 16 * 16 + px[y][x]);
                    }
            }
            own_px[t] = made[t];
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
    struct rosters *rs = &B->rosters;
    memcpy(rs->asset, rosters_cartridge.asset, sizeof rs->asset);   /* (the shared art: the cartridge's, unless the roster set has its own) */
    B->tables = calloc(1, sizeof *B->tables);
    if (!B->tables) { free(b.b); return fail("out of memory"); }
    /* the art of its own: each logo and centre ice not its art team's, each photo not the
     * cartridge's of its name (the tiles a logo or centre ice has are DMA'd: none across 128 KB) */
    uint32_t logo_at[ROSTERS_MAX], ice_at[ROSTERS_MAX], *photo_at = calloc((size_t)L->photos + 1, sizeof *photo_at);
    if (!photo_at) { free(b.b); return fail("out of memory"); }
    for (int t = 0; t < L->teams; t++) {
        struct team *T = &L->t[t];
        uint8_t px[ICE_MAX], asset[ART_LOGO_BYTES];
        uint16_t pal[16];
        logo_at[t] = c32(c, SYM_TeamLogoBitmaps + 4u * (uint32_t)T->art);
        ice_at[t] = c32(c, SYM_TeamGfxList + 4u * (uint32_t)T->art);
        if (T->has_logo && (art_logo(cs, T->art, px, pal) < 0 || memcmp(px, T->logo, ART_LOGO * ART_LOGO) || memcmp(pal, T->logo_pal, sizeof pal))) {
            size_t n = art_logo_make(T->logo, asset);
            dma_align(&b, (uint32_t)n);
            logo_at[t] = at(&b);
            put(&b, asset, n);
            even(&b);
        }
        if (T->has_ice) {
            int w, h;
            if (art_ice(cs, T->art, px, &w, &h, pal) < 0 || art_ice_make(cs, T->ice, w, h, asset) < 0) {
                free(photo_at);
                free(b.b);
                return fail("%s: %s", T->abbr, art_error());
            }
            if (memcmp(asset, c + ice_at[t], ART_ICE_BYTES)) {
                dma_align(&b, ART_ICE_BYTES);
                ice_at[t] = at(&b);
                put(&b, asset, ART_ICE_BYTES);
            }
        }
    }
    if (shared_build(&b, L, rs) < 0) { free(photo_at); free(b.b); return -1; }
    for (int k = 0; k < L->photos; k++) {
        uint8_t px[ART_LOGO * ART_LOGO], asset[ART_PHOTO_BYTES];
        uint32_t pic = picture_named(L->photo[k].name);
        if (pic && art_photo(cs, pic, px) == 0 && !memcmp(px, L->photo[k].px, sizeof px)) { photo_at[k] = pic; continue; }
        if (art_photo_make(L->photo[k].px, asset) < 0) {
            free(photo_at);
            free(b.b);
            return fail("art/photos/%s.png: a photo has 8 shades, colours 5 to 12 of its palette", L->photo[k].name);
        }
        photo_at[k] = at(&b);
        put(&b, asset, sizeof asset);
    }
    for (int t = 0; t < L->teams; t++) roster_team_of(L, t, photo_at, logo_at[t], ice_at[t], &B->tables->team[t]);   /* (the teams: tables, not in the image) */
    memcpy(B->tables->seats, L->seats, 16u * (uint32_t)L->rows);
    rs->table[RS_Teamblocksmap] = SYM_Teamblocksmap;
    for (int t = 0; t < L->teams; t++) rs->block[t] = (uint8_t)L->t[t].art;
    /* banners drawn a team at a time (put_banners) when the roster set has its own, or a setup screen
     * of more tiles than the cartridge's: the room the full set of setup banners would take */
    uint32_t setup = rs->asset[ART_GameSetUpMap];
    const uint8_t *sb = setup >= ROM_SIZE && !b.bad ? b.b : c;
    int room = (c16(sb, setup + 8) & 0x7FFF) > (c16(c, SYM_GameSetUpMap + 8) & 0x7FFF);
    if (own || room) put_banners(&b, L, c, own_px, rs);
    free(photo_at);
    if (b.bad) { free(b.b); free(B->tables); return fail("out of memory"); }
    if (at(&b) > IMAGE_MAX) { free(b.b); free(B->tables); return fail("%u KB past the cartridge: a roster set holds %u KB at most", (unsigned)((at(&b) - ROM_SIZE - (at(&b) > SRAM_END ? SRAM_END - SRAM_START : 0)) >> 10), (IMAGE_MAX - ROM_SIZE - (SRAM_END - SRAM_START)) >> 10); }
    rs->teams = (uint16_t)L->teams;
    rs->playoff_teams = (uint16_t)L->playoff_teams;
    memcpy(rs->hiscore_at, L->hiscore_at, sizeof rs->hiscore_at);
    rs->playoff_rows = (uint16_t)L->rows;
    for (int t = 0; t < L->teams; t++) rs->art[t] = (uint8_t)L->t[t].art;
    B->size = at(&b);                                     /* (built on a copy of the cartridge, kept no longer: its art past it) */
    if (B->size > ROM_SIZE) {
        memmove(b.b, b.b + ROM_SIZE, B->size - ROM_SIZE);
        uint8_t *art = realloc(b.b, B->size - ROM_SIZE);
        B->art = art ? art : b.b;
    } else free(b.b);
    uint64_t h = fnv64(0xCBF29CE484222325ull, B->art, B->size - ROM_SIZE);
    uint8_t counts[6] = { (uint8_t)(rs->teams >> 8), (uint8_t)rs->teams, (uint8_t)(rs->playoff_teams >> 8), (uint8_t)rs->playoff_teams,
                          (uint8_t)(rs->playoff_rows >> 8), (uint8_t)rs->playoff_rows };
    h = fnv64(fnv64(fnv64(h, counts, sizeof counts), rs->art, sizeof rs->art), rs->block, sizeof rs->block);
    h = tables_hash(h, B->tables->team, L->teams, B->tables->seats, L->rows);
    if (memcmp(rs->hiscore_at, rosters_cartridge.hiscore_at, sizeof rs->hiscore_at))   /* (a roster set with the cartridge's keeps its checksum) */
        h = fnv64(h, rs->hiscore_at, sizeof rs->hiscore_at);
    B->hash = h ? h : 1;                                  /* (0 is the cartridge's) */
    return 0;
}

/* ---- this game's roster set ------------------------------------------------------------------------ */

static struct { char dir[512]; char name[64]; uint64_t hash; uint8_t *art; } now;
static int pack_files(const char *dir, mdrt_zip_writer *w, int *count);
static char next_dir[512];

static void use_cartridge(void)
{
    rom_art = NULL;
    rom_limit = ROM_SIZE;
    rosters_retail();
    free(now.art);
    now.art = NULL;
    now.dir[0] = now.name[0] = 0;
    now.hash = 0;
    art_map_make();
}

static void use_built(struct built *B, const char *dir, const char *name)
{
    uint8_t *old = now.art;
    char clean[512];
    clean_dir(clean, sizeof clean, dir);
    dir = clean;
    rom_art = B->art;
    rom_limit = B->size;
    rosters = B->rosters;
    now.art = B->art;
    now.hash = B->hash;
    snprintf(now.dir, sizeof now.dir, "%s", dir);
    snprintf(now.name, sizeof now.name, "%s", name);
    free(old);
    memcpy(roster_team, B->tables->team, sizeof roster_team);    /* (its teams) */
    memcpy(roster_seats, B->tables->seats, sizeof roster_seats);
    free(B->tables);
    B->tables = NULL;
    art_map_make();
}

/* dir's roster set built on the cartridge: 0, or -1 with rosters_error() */
/* one roster set read at a time (src): the game's thread builds one at power on, and the window's may
 * read one the player found meanwhile (rosters_add) */
static atomic_flag reading = ATOMIC_FLAG_INIT;

static int build_dir(const char *dir, struct built *B, char *name, size_t name_size)
{
    struct folder *L;
    if (!rom_cart) return fail("no cartridge");
    while (atomic_flag_test_and_set(&reading)) {}        /* (the window's read takes a moment) */
    if (read_rosters(dir, &L, 1) < 0) { src_close(); atomic_flag_clear(&reading); return -1; }
    err_file = NULL;
    err_line = 0;
    int r = build(L, rom_cart, B);
    if (name) snprintf(name, name_size, "%s", L->name);
    free_lg(L);
    src_close();
    atomic_flag_clear(&reading);
    return r;
}

void rosters_next(const char *dir) { clean_dir(next_dir, sizeof next_dir, dir ? dir : ""); }

int rosters_next_bytes(const uint8_t *b, size_t n)
{
    rosters_next(NULL);
    if (!n) return 0;
    while (atomic_flag_test_and_set(&reading)) {}        /* (the window may be reading it: rosters_session) */
    if (!sent.b || sent.n != n || memcmp(sent.b, b, n)) { /* (kept for the run: another session's another) */
        uint8_t *c = malloc(n);
        if (!c) { atomic_flag_clear(&reading); return fail("out of memory"); }
        memcpy(c, b, n);
        free(sent.b);
        sent.b = c;
        sent.n = n;
        sent.version++;
        snprintf(sent.label, sizeof sent.label, "host-rosters");
    }
    atomic_flag_clear(&reading);
    snprintf(next_dir, sizeof next_dir, "%s", sent.label);
    return 0;
}

long rosters_next_package(uint8_t **out)
{
    *out = NULL;
    err_file = NULL;
    err_line = 0;
    if (!next_dir[0]) return 0;
    if (sent.b && !strcmp(next_dir, sent.label)) {        /* (the roster set a host sent: as it came) */
        if (!(*out = malloc(sent.n))) return fail("out of memory");
        memcpy(*out, sent.b, sent.n);
        return (long)sent.n;
    }
    if (is_package(next_dir)) {                           /* (a package: its bytes as they are) */
        long n = file_bytes(next_dir, out, MDRT_ZIP_MAX_TOTAL);
        return n > 0 ? n : n < 0 ? -1 : fail("no package %s", next_dir);
    }
    mdrt_zip_writer w = { 0 };                            /* (a folder: its files packed, as --pack-rosters packs them) */
    int count = 0;
    if (pack_files(next_dir, &w, &count) < 0) { mdrt_zip_writer_free(&w); return -1; }
    size_t n = mdrt_zip_finish(&w, out);
    return n ? (long)n : fail("%s cannot be packed", next_dir);
}
int rosters_next_is_cartridge(void) { return !next_dir[0]; }
uint64_t rosters_hash(void) { return now.hash; }
const char *rosters_dir(void) { return now.dir; }
const char *rosters_name(void) { return now.name[0] ? now.name : "NHL '94"; }

void rosters_power_on(void)
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

int rosters_use(const char *dir, uint64_t hash)
{
    struct built B;
    char name[64];
    if (hash == now.hash) return 0;
    if (!hash) { use_cartridge(); return 0; }
    if (!dir || !*dir) return fail("a roster set with no folder");
    if (build_dir(dir, &B, name, sizeof name) < 0) return fail("its roster set, %s, is not there: %s", dir, err);
    if (B.hash != hash) { built_free(&B); return fail("its roster set, %s, has changed since", dir); }
    use_built(&B, dir, name);
    return 0;
}

int rosters_check(const char *dir, int describe)
{
    struct built B;
    char name[64];
    if (build_dir(dir, &B, name, sizeof name) < 0) return -1;
    if (describe) printf("%s: %s, %d teams (%d for the cup), %d playoff draws, %u bytes past the cartridge, checksum %016llx\n", dir, name, B.rosters.teams,
           B.rosters.playoff_teams, B.rosters.playoff_rows, (unsigned)(B.size - ROM_SIZE - (B.size > SRAM_END ? SRAM_END - SRAM_START : 0)),
           (unsigned long long)B.hash);                   /* (the save RAM's addresses left out) */
    built_free(&B);
    return 0;
}

/* ---- the rosters/ folder, for the menu ----------------------------------------------------------- */

#define LIST_MAX 32
static char list_name[LIST_MAX][512], list_label[LIST_MAX][40];   /* (a folder of rosters/, or a path the player found) */
static const char *list_names[LIST_MAX], *list_labels[LIST_MAX];
static int list_n;

static int by_name(const void *a, const void *b) { return strcmp(a, b); }

/* a roster set as the Rosters page tells of it, kept when the list is made (before the cartridge is
 * loaded, perhaps: the players' overall ratings are reckoned when the page asks, with its weights) */
enum { B_PP, B_PK, B_HOME, B_AWAY, B_OFFENSE, B_DEFENSE, BONUSES };   /* (a team's .sr nibbles) */
struct about_player { char name[LEN_NAME + 1]; uint8_t team, number, goalie, nib[14]; char pos; };
struct about {
    int teams, playoff_teams, package;
    long bytes;                 /* (a package's) */
    struct { char name[LEN_CITY + 1], abbr[LEN_ABBR + 1]; int rating, art, bonus[BONUSES], players; } t[ROSTERS_MAX];   /* (-1: its art team's) */
    int players;
    struct about_player pl[ROSTERS_MAX * ROSTER];
};
static struct about *about[LIST_MAX];
static int list_unread;                 /* roster sets in rosters/ the list leaves out (why: on stderr) */

/* the teams and players of a roster set of n teams (the first playoff of them for the cup) */
static void about_teams(struct about *A, const struct team *T, int n, int playoff)
{
    A->teams = n;
    A->playoff_teams = playoff;
    for (int t = 0; t < n; t++) {
        int shared = 0;
        for (int k = 0; k < n; k++) shared += k != t && same_word(T[k].city, T[t].city);
        snprintf(A->t[t].name, sizeof A->t[t].name, "%s", shared ? T[t].nickname : T[t].city);   /* (as short as says which) */
        snprintf(A->t[t].abbr, sizeof A->t[t].abbr, "%s", T[t].abbr);
        A->t[t].rating = T[t].rating;
        A->t[t].art = T[t].art;
        const int bonus[BONUSES] = { T[t].pp, T[t].pk, T[t].home, T[t].away, T[t].offense, T[t].defense };
        memcpy(A->t[t].bonus, bonus, sizeof bonus);
        A->t[t].players = T[t].players;
        for (int i = 0; i < T[t].players && A->players < ROSTERS_MAX * ROSTER; i++) {
            const struct player *P = &T[t].pl[i];
            int goalie = i < T[t].goalies;
            snprintf(A->pl[A->players].name, sizeof A->pl[0].name, "%s", P->name);
            A->pl[A->players].team = (uint8_t)t;
            A->pl[A->players].number = P->number;
            A->pl[A->players].goalie = (uint8_t)goalie;
            A->pl[A->players].pos = goalie ? 'G' : i < T[t].goalies + T[t].forwards ? 'F' : 'D';
            memcpy(A->pl[A->players++].nib, P->nib, 14);
        }
    }
}

/* a name in at most n letters: as it is, else its first name's initial and the rest */
static void short_name(char *out, const char *name, int n)
{
    const char *space = strchr(name, ' ');
    if ((int)strlen(name) <= n || !space) snprintf(out, (size_t)n + 1, "%s", name);
    else snprintf(out, (size_t)n + 1, "%c. %s", name[0], space + 1);
}

/* a file's name in at most n letters: as it is, else its start and end with ... between */
static void short_path(char *out, const char *path, int n)
{
    int len = (int)strlen(path), head = (n - 3) / 2;
    if (len <= n) snprintf(out, (size_t)n + 1, "%.*s", n, path);
    else snprintf(out, (size_t)n + 1, "%.*s...%.*s", head, path, n - 3 - head, path + len - (n - 3 - head));
}

/* a player of the Rosters page and his card's overall rating (sum: the sum it comes from) */
struct rated { int i, rating, sum; };

/* list entry n, its name given: its label, and what the Rosters page says of it (L read from dir) */
static void listed(int n, const struct folder *L, const char *dir)
{
    snprintf(list_label[n], sizeof list_label[n], "%.39s", L->name);
    list_names[n] = list_name[n];
    list_labels[n] = list_label[n];
    free(about[n]);
    if ((about[n] = calloc(1, sizeof *about[n]))) {
        struct stat st;
        about_teams(about[n], L->t, L->teams, L->playoff_teams);
        about[n]->package = src.zip;
        about[n]->bytes = src.zip && stat(dir, &st) == 0 ? (long)st.st_size : 0;
    }
}

int rosters_unread(void) { return list_unread; }

const char *rosters_list_dir(int i, char *out, size_t size)
{
    if (i <= 0 || i >= list_n) return NULL;
    const char *n = list_name[i];
    if (sent.b && !strcmp(n, sent.label)) snprintf(out, size, "%s", n);   /* (the roster set a host sent: by its name) */
    else if (strpbrk(n, "/\\") || (n[0] && n[1] == ':')) snprintf(out, size, "%s", n);   /* (found by the player: its path) */
    else snprintf(out, size, "rosters/%s", n);
    return out;
}

void rosters_files(int i, char *text, size_t size)
{
    char dir[600], file[64], kb[24];
    if (i <= 0 || i >= list_n || !about[i]) { snprintf(text, size, "built in"); return; }
    if (sent.b && !strcmp(list_name[i], sent.label)) { snprintf(text, size, "sent by the host, %ld KB", (about[i]->bytes + 1023) / 1024); return; }
    snprintf(kb, sizeof kb, about[i]->package ? ", %ld KB" : ", a folder", (about[i]->bytes + 1023) / 1024);
    short_path(file, rosters_list_dir(i, dir, sizeof dir), 49 - 5 - 2 - (int)strlen(kb));
    snprintf(text, size, "%s%s", file, kb);
}

/* a path's place on disk, to know two names of one folder for one (else the path as it is) */
static void real_place(const char *path, char *out, size_t size)
{
#ifdef _WIN32
    if (!_fullpath(out, path, size)) snprintf(out, size, "%s", path);
#else
    char *r = realpath(path, NULL);
    snprintf(out, size, "%s", r ? r : path);
    free(r);
#endif
}

int rosters_session_entry(void)
{
    static int at = -1, version;
    if (!sent.b) return -1;
    if (at > 0 && version == sent.version) return at;
    if (atomic_flag_test_and_set(&reading)) return -1;   /* (the game is reading one: asked again later) */
    int k = at > 0 ? at : list_n, r = -1;
    struct folder *L;
    if (k < LIST_MAX && (r = read_rosters(sent.label, &L, 0)) == 0) {
        snprintf(list_name[k], sizeof list_name[0], "%s", sent.label);
        listed(k, L, sent.label);
        snprintf(list_label[k], sizeof list_label[k], "%.26s (the host's)", L->name);
        if (about[k]) about[k]->bytes = (long)sent.n;
        free_lg(L);
        if (k == list_n) list_n++;
        at = k;
        version = sent.version;
    }
    src_close();
    atomic_flag_clear(&reading);
    return r == 0 ? at : -1;
}

int rosters_add(const char *path, char *why, size_t why_size)
{
    char place[1100], other[1100], dir[600];
    struct folder *L;
    real_place(path, place, sizeof place);
    for (int i = 1; i < list_n; i++) {                    /* (listed already, by whatever name) */
        real_place(rosters_list_dir(i, dir, sizeof dir), other, sizeof other);
        if (!strcmp(place, other)) return i;
    }
    if (strlen(path) >= sizeof list_name[0]) { snprintf(why, why_size, "the path is too long"); return -1; }
    if (list_n >= LIST_MAX) { snprintf(why, why_size, "the list has room for %d roster sets", LIST_MAX - 1); return -1; }
    if (atomic_flag_test_and_set(&reading)) { snprintf(why, why_size, "the game is reading its roster set: try again"); return -1; }
    int r = read_rosters(path, &L, 0);
    if (r == 0) {
        snprintf(list_name[list_n], sizeof list_name[0], "%s", path);
        listed(list_n++, L, path);
        free_lg(L);
    } else snprintf(why, why_size, "%s", err);
    src_close();
    atomic_flag_clear(&reading);
    return r == 0 ? list_n - 1 : -1;
}

/* the roster screen's columns the Rosters page ranks players by (PAttribColumns: their masks) */
#define COL_SPEED 0x0800u
#define COL_SHOT_POWER 0x0100u
#define COL_SHOT_ACCURACY 0x0010u
#define COL_PASS_ACCURACY 0x0002u
#define COL_STICK_HANDLING 0x0020u
#define COL_CHECKING 0x0080u

/* a team's bonus (B_: the .sr nibbles), its art team's when the roster set leaves it to that */
static int bonus(const struct about *A, int t, int k)
{
    int v = A->t[t].bonus[k];
    if (v >= 0 || !rom_cart) return v < 0 ? 0 : v;
    uint32_t b = c32(rom_cart, SYM_TeamList + 4u * (uint32_t)A->t[t].art), sr = b + c16(rom_cart, b + 8);
    uint8_t x0 = rom_cart[sr], x1 = rom_cart[sr + 1], x2 = rom_cart[sr + 2];
    static const int shift[BONUSES] = { 0, 4, 4, 0, 4, 0 };
    uint8_t x = k == B_PP || k == B_PK ? x1 : k == B_HOME || k == B_AWAY ? x2 : x0;
    return x >> shift[k] & (k >= B_OFFENSE ? 7 : 15);
}

/* the players of the teams that play for the cup (not All-Stars), a player once (an All-Star's copy
 * aside), with their ratings: r[] (n of them), the overall's sum in sum */
static int cup_players(const struct about *A, struct rated *r)
{
    int n = 0;
    for (int p = 0; p < A->players; p++) {
        int dup = 0;
        for (int k = 0; k < n && !dup; k++) dup = !strcmp(A->pl[r[k].i].name, A->pl[p].name);
        if (A->pl[p].team >= A->playoff_teams || dup) continue;
        r[n].i = p;
        r[n].rating = rating_overall(rom_cart, A->pl[p].nib, A->pl[p].number, A->pl[p].goalie, &r[n].sum);
        n++;
    }
    return n;
}

#define LINE(...) do { if (*at < size) *at += (size_t)snprintf(text + *at, size - *at, __VA_ARGS__); } while (0)

/* a section of the Rosters page: its title, then ten of players (goalie: the goalies, else the
 * skaters), best first: by their card's overall rating, or (col) by the rating of a roster screen
 * column, then the overall. A column's rating is shown as the roster screen shows it: a rating of
 * 0 to 6 (the cartridge's scale: the game plays a rating as 5 times it, at most 30, a streak
 * added: AttributeCalc) as 25, 34, 43, 54, 72, 90 or 99, nothing between. Past 7 the screen's
 * reckoning runs over (8 to 14 come out as 25), so a roster set that rates past 7 shows its own
 * numbers, and says so */
struct ranked { int i, shown, key[3]; };
static int by_ranked(const void *a, const void *b)
{
    const struct ranked *x = a, *y = b;
    for (int k = 0; k < 3; k++) if (x->key[k] != y->key[k]) return y->key[k] - x->key[k];
    return x->i - y->i;
}
static void player_section(const struct about *A, const struct rated *r, int n, const char *title, int goalie, unsigned col,
                           const char *unit, char *text, size_t size, size_t *at)
{
    static struct ranked k[ROSTERS_MAX * ROSTER];
    int m = 0, most = 0;
    for (int j = 0; j < n; j++) {
        const struct about_player *P = &A->pl[r[j].i];
        if (P->goalie != goalie) continue;
        int raw = 0;
        for (int d2 = 0; d2 < 14; d2++) if (col & 1u << d2) raw += P->nib[13 - d2];
        if (raw > most) most = raw;
        k[m++] = (struct ranked){ r[j].i, col ? raw : r[j].rating, { col ? raw : r[j].sum, r[j].sum, 0 } };
    }
    qsort(k, (size_t)m, sizeof k[0], by_ranked);
    int own = col && most > 7;                            /* (past the roster screen's reckoning: the roster set's own numbers) */
    for (int j = 0; col && !own && j < m; j++) k[j].shown = rating_column(rom_cart, A->pl[k[j].i].nib, A->pl[k[j].i].number, col);
    LINE("\f%s\n", title);
    if (own) LINE("Rated 0-15, past the game's 0-6\n");
    char name[LEN_NAME + 1];
    for (int j = 0; j < m && j < 10; j++) {
        short_name(name, A->pl[k[j].i].name, 18);
        LINE("%-18s %-3s\t%d%s\n", name, A->t[A->pl[k[j].i].team].abbr, k[j].shown, unit);
    }
}

/* a section of the teams that play for the cup by a bonus of theirs (setplayer adds it to its
 * skaters' ratings, or takes it off: least, a penalty), the best first, those as good on a line
 * (their abbreviations, ten to a line), signed as the game adds it */
static void team_section(const struct about *A, int b, int least, const char *title, const char *what, char *text, size_t size, size_t *at)
{
    struct ranked k[ROSTERS_MAX];
    int m = 0;
    for (int t = 0; t < A->playoff_teams; t++) {
        int v = bonus(A, t, b);
        k[m++] = (struct ranked){ t, v, { least ? -v : v, 0, 0 } };
    }
    qsort(k, (size_t)m, sizeof k[0], by_ranked);
    LINE("\f%s\n%s\n", title, what);
    for (int j = 0, lines = 1; j < m && lines < 11; lines++) {   /* (eleven lines: a page of the menu's under the Rosters line) */
        char value[8], abbrs[48] = "";
        snprintf(value, sizeof value, "%s%d", k[j].shown ? least ? "-" : "+" : "", k[j].shown);
        int first = j, room = lines == 10 ? 9 : 10;         /* (the last line: room for "...") */
        for (int c = 0; j < m && k[j].shown == k[first].shown && c < room; j++, c++)
            snprintf(abbrs + strlen(abbrs), sizeof abbrs - strlen(abbrs), "%s%s", c ? " " : "", A->t[k[j].i].abbr);
        if (lines == 10 && j < m) snprintf(abbrs + strlen(abbrs), sizeof abbrs - strlen(abbrs), " ...");
        LINE("  %s\t%s\n", first && k[first - 1].shown == k[first].shown ? "" : value, abbrs);
    }
}

/* the teams that play for the cup with value v (of bonus b; -1: players on its roster): the team
 * (its name, the value and unit after it), or those that share it (their abbreviations, a few) */
static const char *teams_with(const struct about *A, int b, int v, const char *unit, char *out, size_t size)
{
    int n = 0, first = -1;
    for (int t = 0; t < A->playoff_teams; t++)
        if ((b < 0 ? A->t[t].players : bonus(A, t, b)) == v) { if (first < 0) first = t; n++; }
    if (n == 1) { snprintf(out, size, b < 0 ? "%s, %d%s" : "%s", A->t[first].name, v, unit); return out; }
    if (n > 5) { snprintf(out, size, b < 0 ? "%d teams, %d%s" : "%d teams", n, v, unit); return out; }
    out[0] = 0;
    for (int t = 0; t < A->playoff_teams; t++)
        if ((b < 0 ? A->t[t].players : bonus(A, t, b)) == v)
            snprintf(out + strlen(out), size - strlen(out), "%s%s", out[0] ? " " : "", A->t[t].abbr);
    if (b < 0) snprintf(out + strlen(out), size - strlen(out), ", %d%s", v, unit);
    return out;
}

void rosters_about(int i, char *text, size_t size)
{
    static struct about *cart;            /* (the cartridge's: made when first asked, it loaded) */
    static struct rated r[ROSTERS_MAX * ROSTER];
    struct team *T;
    struct about *A = i == 0 ? cart : i > 0 && i < LIST_MAX ? about[i] : NULL;
    size_t at0 = 0, *at = &at0;
    text[0] = 0;
    if (i == 0 && !A && rom_cart && (A = calloc(1, sizeof *A)) && (T = calloc(SYM_NumTeams, sizeof *T))) {
        for (int t = 0; t < (int)SYM_NumTeams; t++) cart_team(rom_cart, t, &T[t]);
        about_teams(A, T, (int)SYM_NumTeams, (int)SYM_NumPlayoffTeams);
        free(T);
        cart = A;
    }
    if (!A) { snprintf(text, size, "%s", i == 0 ? "The cartridge's teams: once its ROM is chosen\n" : ""); return; }
    LINE("Teams\t%d (%d cup)\n", A->teams, A->playoff_teams);
    if (!rom_cart) { LINE("Ratings\tonce the cartridge's ROM is chosen\n"); return; }
    int n = cup_players(A, r);
    /* the players, as their cards rate them, then by a rating as the roster screen shows it */
    player_section(A, r, n, "Best skaters", 0, 0, "", text, size, at);
    player_section(A, r, n, "Best goalies", 1, 0, "", text, size, at);
    {                                                      /* the teams, by the rating the game gives each */
        struct ranked k[ROSTERS_MAX];
        int m = 0;
        for (int t = 0; t < A->playoff_teams; t++) {
            int rating = A->t[t].rating >= 0 ? A->t[t].rating : rom_cart[SYM_TeamRatings + (uint32_t)A->t[t].art];   /* (-1: its art team's) */
            k[m++] = (struct ranked){ t, rating, { rating, 0, 0 } };
        }
        qsort(k, (size_t)m, sizeof k[0], by_ranked);
        LINE("\fBest teams\n");
        for (int j = 0; j < m && j < 10; j++) LINE("%-18s %-3s\t%d\n", A->t[k[j].i].name, A->t[k[j].i].abbr, k[j].shown);
    }
    player_section(A, r, n, "Fastest skaters", 0, COL_SPEED, "", text, size, at);
    player_section(A, r, n, "Hardest shots", 0, COL_SHOT_POWER, "", text, size, at);
    player_section(A, r, n, "Sharpest shooters", 0, COL_SHOT_ACCURACY, "", text, size, at);
    player_section(A, r, n, "Best passers", 0, COL_PASS_ACCURACY, "", text, size, at);
    player_section(A, r, n, "Best stickhandlers", 0, COL_STICK_HANDLING, "", text, size, at);
    player_section(A, r, n, "Biggest hitters", 0, COL_CHECKING, "", text, size, at);
    {                                                      /* the heaviest skaters and the lightest (the roster screen's Weight) */
        static struct ranked k[ROSTERS_MAX * ROSTER];
        int m = 0;
        for (int j = 0; j < n; j++)
            if (!A->pl[r[j].i].goalie) k[m++] = (struct ranked){ r[j].i, 140 + 8 * A->pl[r[j].i].nib[0], { A->pl[r[j].i].nib[0], r[j].sum, 0 } };
        qsort(k, (size_t)m, sizeof k[0], by_ranked);
        char name[LEN_NAME + 1];
        LINE("\fHeaviest skaters\n");
        for (int j = 0; j < m && j < 10; j++) {
            short_name(name, A->pl[k[j].i].name, 18);
            LINE("%-18s %-3s\t%d lb\n", name, A->t[A->pl[k[j].i].team].abbr, k[j].shown);
        }
        LINE("\fLightest skaters\n");
        for (int j = m - 1; j >= 0 && j >= m - 10; j--) {
            short_name(name, A->pl[k[j].i].name, 18);
            LINE("%-18s %-3s\t%d lb\n", name, A->t[A->pl[k[j].i].team].abbr, k[j].shown);
        }
    }
    team_section(A, B_PP, 0, "Power play", "What its skaters gain a man up", text, size, at);
    team_section(A, B_PK, 1, "Penalty killing", "What its skaters lose a man down", text, size, at);
    team_section(A, B_HOME, 0, "Home ice", "What its skaters gain at home", text, size, at);
    team_section(A, B_AWAY, 1, "On the road", "What its skaters lose away", text, size, at);
    {                                                      /* the roster set in numbers */
        int skaters = 0, goalies = 0, forwards = 0, right = 0, weight = 0, numbers[100] = { 0 }, most = 0;
        for (int j = 0; j < n; j++) {
            const struct about_player *P = &A->pl[r[j].i];
            if (P->goalie) { goalies++; continue; }
            skaters++;
            forwards += P->pos == 'F';
            right += P->nib[LR_HANDED_FIGHTING] & 1;
            weight += P->nib[0];
            if (P->number < 100) numbers[P->number]++;
        }
        for (int k = 1; k < 100; k++) if (numbers[k] > numbers[most]) most = k;
        LINE("\fIn numbers\n");
        LINE("Players\t%d: %d skaters, %d goalies\n", skaters + goalies, skaters, goalies);
        LINE("Skaters\t%d forwards, %d defensemen\n", forwards, skaters - forwards);
        LINE("Shooting\t%d left, %d right\n", skaters - right, right);
        if (skaters) LINE("The average skater\t%d lb\n", 140 + (8 * weight + skaters / 2) / skaters);
        if (numbers[most]) LINE("Number worn most\t#%d, by %d skaters\n", most, numbers[most]);
        /* the rosters' sizes; who scores most and allows least when the playoffs simulate their
         * other games (getscore: the scorer's offense row and the other team's defense row, the
         * higher the more goals): a team, or how many share it */
        int lo = 99, hi = 0, olo = 99, ohi = 0;
        for (int t = 0; t < A->playoff_teams; t++) {
            int p = A->t[t].players, o = bonus(A, t, B_OFFENSE), d = bonus(A, t, B_DEFENSE);
            lo = p < lo ? p : lo; hi = p > hi ? p : hi;
            ohi = o > ohi ? o : ohi; olo = d < olo ? d : olo;
        }
        if (A->playoff_teams && lo == hi) LINE("Every roster\t%d players\n", hi);
        else if (A->playoff_teams) {
            LINE("Rosters\t%d to %d players\n", lo, hi);
            char who[48];
            LINE("The most\t%s\n", teams_with(A, -1, hi, " players", who, sizeof who));
            LINE("The fewest\t%s\n", teams_with(A, -1, lo, " players", who, sizeof who));
        }
        char who[48];
        if (A->playoff_teams) LINE("Scores most when simulated\t%s\n", teams_with(A, B_OFFENSE, ohi, "", who, sizeof who));
        if (A->playoff_teams) LINE("Allows least when simulated\t%s\n", teams_with(A, B_DEFENSE, olo, "", who, sizeof who));
    }
}
#undef LINE

int rosters_list(const char *const **names, const char *const **labels)
{
    int n = 1;
    char found[LIST_MAX * 2][64];
    int nf = 0;
    list_unread = 0;
    snprintf(list_name[0], sizeof list_name[0], "cartridge");
    snprintf(list_label[0], sizeof list_label[0], "NHL '94 (the cartridge)");
    DIR *d = opendir("rosters");
    struct dirent *e;
    while (d && (e = readdir(d)) && nf < LIST_MAX * 2) {
        char path[600];
        struct stat st;
        if (e->d_name[0] == '.' || strlen(e->d_name) >= sizeof found[0]) continue;
        snprintf(path, sizeof path, "rosters/%.200s/teams.csv", e->d_name);
        if (stat(path, &st) == 0 || is_package(e->d_name)) snprintf(found[nf++], sizeof found[0], "%.63s", e->d_name);   /* (a folder, or a package) */
    }
    if (d) closedir(d);
    qsort(found, (size_t)nf, sizeof found[0], by_name);
    for (int i = 0; i < nf && n < LIST_MAX; i++) {
        char dir[600];
        struct folder *L;
        if (!strcmp(found[i], "cartridge")) { fprintf(stderr, "rosters/cartridge: the name is the cartridge's own: give the folder another\n"); list_unread++; continue; }
        snprintf(dir, sizeof dir, "rosters/%.63s", found[i]);
        if (read_rosters(dir, &L, 0) < 0) { fprintf(stderr, "%s: %s (not in the menu)\n", dir, err); src_close(); list_unread++; continue; }   /* (no cartridge yet: its art is read when it is played) */
        snprintf(list_name[n], sizeof list_name[n], "%.63s", found[i]);
        listed(n++, L, dir);
        free_lg(L);
        src_close();
    }
    for (int i = 0; i < n; i++) { list_names[i] = list_name[i]; list_labels[i] = list_label[i]; }
    list_n = n;
    *names = list_names;
    *labels = list_labels;
    return n;
}

/* the save RAM's file for the next game: the cartridge's base, or base-NAME.srm (its records are its own) */
const char *rosters_sram(const char *base)
{
    static char path[1200];
    if (!next_dir[0] || !base) return base;
    const char *slash = strrchr(next_dir, '/'), *name = slash && slash[1] ? slash + 1 : next_dir;
    const char *dot = strrchr(base, '.'), *bs = strrchr(base, '/');
    int stem = dot && (!bs || dot > bs) ? (int)(dot - base) : (int)strlen(base), len = (int)strlen(name);
    if (is_package(name)) len -= 4;                       /* (a package's: its name's) */
    snprintf(path, sizeof path, "%.*s-%.*s.srm", stem, base, len, name);
    return path;
}

/* ---- the cartridge's roster set, written out --------------------------------------------------------- */

/* a CSV field and a comma after it (last: a new line): quoted when it must be, a byte outside
 * plain ASCII as \xNN */
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
    if (last != 2) fputc(last ? '\n' : ',', f);   /* (2: nothing after) */
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

/* s's playoff draws as a folder has them (seats), each playoff team in one at least: the game
 * looks for the player's team in them (NewPO) and would look forever; a team in none takes the
 * place of the one in the most draws (said on stdout). 0, or -1 when they are not draws of
 * playoff teams (then made from the conferences) */
static int src_draws(const struct romsrc *s, const struct team *T, uint8_t seats[ROWS_MAX][16])
{
    int count[ROSTERS_MAX] = { 0 };
    for (int r = 0; r < s->rows; r++) {
        uint32_t in = 0;
        for (int k = 0; k < 16; k++) {
            int t = seats[r][k] = rs8(s, s->table[RS_playoffseats] + 16u * (uint32_t)r + (uint32_t)k);
            if (t >= s->playoff_teams || (in >> t & 1)) return -1;
            in |= 1u << t;
            count[t]++;
        }
    }
    for (int m = 0; m < s->playoff_teams; m++) {
        if (count[m]) continue;
        int br = 0, bk = 0;
        for (int r = 0; r < s->rows; r++)
            for (int k = 0; k < 16; k++)
                if (count[seats[r][k]] > count[seats[br][bk]]) { br = r; bk = k; }
        if (count[seats[br][bk]] < 2) return -1;
        printf("  playoffs.csv: %s was in no draw (the game would look for it forever): in draw %d in place of %s\n",
               T[m].abbr, br + 1, T[seats[br][bk]].abbr);
        count[seats[br][bk]]--;
        seats[br][bk] = (uint8_t)m;
        count[m]++;
    }
    return 0;
}

/* the shared art (art.c) into art/game: a PNG of each; and, from a hack, each that is not the
 * cartridge's as its bytes too (NAME.bin: what the game reads, the picture drawn over it) */
static int shared_extract(const struct romsrc *s, const char *dir)
{
    const struct romsrc *cs = cart_src();
    uint16_t screen[64], team[16];
    shared_colours(s, screen, team);
    for (int i = 0; i < art_shared(); i++) {
        int k = art_shared_asset(i);
        uint8_t *mine = NULL, *cart = NULL;
        size_t n = art_shared_copy(s, i, s->asset[k], &mine), cn = art_shared_copy(cs, i, cs->asset[k], &cart);
        char path[1200];
        if (!cn) { free(mine); return fail("the cartridge's %s cannot be read", art_shared_name(i)); }
        if (!n && s->hack) printf("  art/game: the hack's %s does not read: the cartridge's\n", art_shared_name(i));
        int own = s->hack && n && (n != cn || memcmp(mine, cart, n));
        if (own) {
            snprintf(path, sizeof path, "%.900s/art/game/%s.bin", dir, art_shared_name(i));
            FILE *f = fopen(path, "wb");
            int ok = f && fwrite(mine, 1, n, f) == n;
            if (f && fclose(f)) ok = 0;
            if (!ok) { free(mine); free(cart); return fail("cannot write %s", path); }
        }
        struct art_view v;                                /* (drawn where it is: the same picture as its copy) */
        int drawn = art_shared_view(own ? s : cs, i, own ? s->asset[k] : cs->asset[k], screen, team, &v);
        free(mine);
        free(cart);
        if (drawn < 0) return fail("%s cannot be drawn", art_shared_name(i));
        snprintf(path, sizeof path, "%.900s/art/game/%s.png", dir, art_shared_name(i));
        int r = art_save(path, v.px, v.w, v.h, v.pal, v.n, 0);
        art_view_free(&v);
        if (r < 0) return fail("%s", art_error());
    }
    return 0;
}

int rosters_export(const char *dir)
{
    struct romsrc s;
    err_file = NULL;
    err_line = 0;
    if (!rom_cart) return fail("no cartridge");
    romsrc_cartridge(&s, rom_cart);
    return rosters_extract(&s, dir, "NHL '94 (from the cartridge)");
}

int rosters_extract(const struct romsrc *s, const char *dir, const char *name)
{
    static struct team T[ROSTERS_MAX], cart[CART_TEAMS];
    static uint8_t seats[ROWS_MAX][16];
    err_file = NULL;
    err_line = 0;
    if (!rom_cart) return fail("no cartridge");
    for (int t = 0; t < CART_TEAMS; t++) cart_team(rom_cart, t, &cart[t]);
    for (int t = 0; t < s->teams; t++) {
        src_team(s, t, &T[t]);
        if (s->hack) T[t].art = art_of(&T[t], cart);
        for (int k = 0; k < t; k++)
            if (same_word(T[k].abbr, T[t].abbr)) return fail("two teams are %s (the files name teams by their abbreviations)", T[t].abbr);
    }
    int draws = src_draws(s, T, seats) == 0;
    if (!draws) printf("  playoffs.csv: its draws are not draws of its playoff teams: left to be made from the conferences\n");
    if (make_dirs(dir) < 0) return -1;
    FILE *f;
    char cell[64];
    if (!(f = create(dir, "rosters.ini"))) return -1;
    if (s->hack)
        fprintf(f, "# %s's roster set, from the hack: the files beside this one are its teams, players, lines,\n"
                   "# colours and art (art/: logos, banners, centre ice, photos, and in art/game the title screens,\n"
                   "# rinks, sprites and the rest it has of its own), to change as you like (README.md, \"Rosters\")\n", name);
    else
        fprintf(f, "# NHL '94's roster set, from the cartridge: the files beside this one are its teams, players,\n"
                   "# lines, colours, pictures and art (art/: logos, banners, centre ice, photos), to change as you\n"
                   "# like (README.md, \"Rosters\")\n");
    fprintf(f, "name = %s\nplayoff_teams = %d\n", name, s->playoff_teams);
    if (memcmp(s->hiscore_at, rosters_cartridge.hiscore_at, sizeof s->hiscore_at))
        fprintf(f, "# where the screen after the EA logo puts its two pictures (column,row): its logo, then HiScoreImg\n"
                   "hiscore_screen = %d,%d %d,%d\n", s->hiscore_at[0][0], s->hiscore_at[0][1], s->hiscore_at[1][0], s->hiscore_at[1][1]);
    fclose(f);
    if (!(f = create(dir, "teams.csv"))) return -1;
    fputs("team,city,nickname,arena,conference,art,rating,offense,defense,pp,pk,home,away,songs,banner\n", f);
    for (int t = 0; t < s->teams; t++) {
        const struct team *X = &T[t];
        field(f, X->abbr, 0); field(f, X->city, 0); field(f, X->nickname, 0); field(f, X->arena, 0); field(f, X->conference, 0);
        field(f, cart_abbr[X->art], 0);
        fprintf(f, "%d,%d,%d,%d,%d,%d,%d,%d %d %d %d %d %d,\n", X->rating, X->offense, X->defense, X->pp, X->pk, X->home, X->away,
                X->songs[0], X->songs[1], X->songs[2], X->songs[3], X->songs[4], X->songs[5]);
    }
    fclose(f);
    for (int sk = 0; sk < 2; sk++) {
        if (!(f = create(dir, sk ? "players.csv" : "goalies.csv"))) return -1;
        fputs(sk ? "team,pos,number,name" : "team,number,name", f);
        for (int k = 0; k < 14; k++) fprintf(f, ",%s", sk ? skater_cols[k] : goalie_cols[k]);
        fputs(sk ? "\n" : ",start\n", f);
        for (int t = 0; t < s->teams; t++)
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
    for (int t = 0; t < s->teams; t++)
        for (int l = 0; l < 8; l++) {
            field(f, T[t].abbr, 0);
            fputs(line_names[l], f);
            for (int k = 0; k < 7; k++) {
                int slot = T[t].line[l][k];
                fputc(',', f);
                if (slot && slot <= T[t].players) field(f, player_cell(&T[t], slot - 1, cell, sizeof cell), 2);
            }
            fputc('\n', f);
        }
    fclose(f);
    int own_sprites = 0;                                  /* (a hack's player sprites its own: its jerseys' colours are for them) */
    for (int i = 0; s->hack && i < art_shared(); i++)
        if (art_shared_asset(i) == ART_Sprites) {
            uint8_t *mine, *theirs;
            size_t n = art_shared_copy(s, i, s->asset[ART_Sprites], &mine), cn = art_shared_copy(cart_src(), i, SYM_Sprites, &theirs);
            own_sprites = n && (n != cn || memcmp(mine, theirs, n));
            free(mine);
            free(theirs);
        }
    if (!(f = create(dir, "colours.csv"))) return -1;
    fputs("team,palette,colours\n", f);
    for (int t = 0; t < s->teams; t++)
        for (int p = 0; p < PALS; p++) {
            if (s->hack && (p == PAL_LOGO || (p <= PAL_AWAY && !own_sprites))) continue;   /* (a logo's colours are its PNG's; jerseys go with their sprites) */
            field(f, T[t].abbr, 0);
            fprintf(f, "%s,", pal_names[p]);
            for (int k = 0; k < 16; k++) fprintf(f, "%04X%c", T[t].pal[p][k], k < 15 ? ' ' : '\n');
        }
    fclose(f);
    /* the pictures: the cartridge's by their labels; a hack's by the players they are of (its own
     * may sit where the cartridge's were), each once */
    static struct { uint32_t pic; char name[LEN_PHOTO + 1]; } named[PHOTOS_MAX];
    int nnamed = 0;
    if (!(f = create(dir, "pictures.csv"))) return -1;
    fputs("team,number,picture\n", f);
    for (int t = 0; t < s->teams; t++)
        for (int i = 0; i < T[t].pics; i++) {
            uint32_t pic = T[t].pic[i].pic;
            if (T[t].pic[i].slot >= T[t].players) continue;
            int k = 0;
            while (k < nnamed && named[k].pic != pic) k++;
            if (k == nnamed) {
                char nm[LEN_PHOTO + 1];
                const char *label = s->hack ? NULL : picture_name(pic);
                if (label) snprintf(nm, sizeof nm, "%s", label);
                else {
                    int n = snprintf(nm, sizeof nm, "Card");
                    for (const char *q = T[t].pl[T[t].pic[i].slot].name; *q && n < LEN_PHOTO - 2; q++)
                        if ((*q >= 'A' && *q <= 'Z') || (*q >= 'a' && *q <= 'z') || (*q >= '0' && *q <= '9')) nm[n++] = *q;
                    nm[n] = 0;
                    for (int again = 2, clash = 1; clash; again++) {   /* (another's of that name: 2, 3 ...) */
                        clash = 0;
                        for (int j = 0; j < nnamed; j++) clash |= !strcmp(named[j].name, nm);
                        if (clash) snprintf(nm + n, sizeof nm - (size_t)n, "%d", again);
                    }
                }
                if (nnamed == PHOTOS_MAX) continue;
                named[nnamed].pic = pic;
                snprintf(named[nnamed++].name, sizeof named[0].name, "%s", nm);
            }
            field(f, T[t].abbr, 0);
            field(f, player_cell(&T[t], T[t].pic[i].slot, cell, sizeof cell), 0);
            fprintf(f, "%s\n", named[k].name);
        }
    fclose(f);
    /* the art: each team's logo, banners and centre ice, each picture's photo (art/) */
    char sub[1100];
    static const char *const kinds[5] = { "logos", "banners", "ice", "photos", "game" };
    for (int k = 0; k < 5; k++) {
        snprintf(sub, sizeof sub, "%.900s/art/%s", dir, kinds[k]);
        if (make_dirs(sub) < 0) return -1;
    }
    /* the setup screen's banners: a team's own, unless every team has the same one (the NHL 2026
     * edition's HOME, which its own code, not taken, draws as a label beside one it makes):
     * then its name in the cartridge's letters and colours, as a team the cartridge has no
     * banner for gets */
    static uint8_t first[ART_BANNER_H * ART_BANNER_W], apx[ICE_MAX];
    uint16_t apal[ART_BANNER_COLOURS], cpal[ART_BANNER_COLOURS];
    int same = s->hack && art_banners(s, 0, first, apal) == 0;
    for (int t = 1; t < s->teams && same; t++)
        same = art_banners(s, t, apx, apal) == 0 && !memcmp(apx + 16 * ART_BANNER_W, first + 16 * ART_BANNER_W, 16 * ART_BANNER_W);
    if (same && banner_prepare(rom_cart) < 0) return fail("the cartridge's team banners cannot be read");
    if (same) printf("  banners: the setup screen's the same for every team (a label): each team's name there instead\n");
    for (int t = 0; t < s->teams; t++) {
        int w, h;
        char path[1200];
        snprintf(path, sizeof path, "%.900s/art/logos/%.3s.png", dir, T[t].abbr);
        if (art_logo(s, t, apx, apal) < 0) return fail("%s's logo cannot be read", T[t].abbr);
        if (art_save(path, apx, ART_LOGO, ART_LOGO, apal, 16, 1) < 0) return fail("%s", art_error());
        snprintf(path, sizeof path, "%.900s/art/banners/%.3s.png", dir, T[t].abbr);
        if (art_banners(s, t, apx, apal) < 0) return fail("%s's banners cannot be read", T[t].abbr);
        if (same) {
            uint8_t label[16][88];
            char text[LEN_CITY + 1], why[120];
            for (size_t k = 0; k <= strlen(T[t].city); k++) text[k] = T[t].city[k] >= 'a' && T[t].city[k] <= 'z' ? (char)(T[t].city[k] - 32) : T[t].city[k];
            banner_pixels(BANNER_BITMAPS, T[t].art, banner_check(text, why, sizeof why) == 0 ? text : NULL, label);   /* (else its art team's) */
            for (int y = 0; y < 16; y++)
                for (int x = 0; x < ART_BANNER_W; x++) apx[(16 + y) * ART_BANNER_W + x] = (uint8_t)(ART_BANNER_SETUP + label[y][x]);
            if (art_banners(cart_src(), T[t].art, first, cpal) < 0) return fail("the cartridge's team banners cannot be read");
            for (int k = 0; k < 16; k++) if (k < 1 || k > 4) apal[ART_BANNER_SETUP + k] = cpal[ART_BANNER_SETUP + k];   /* (1-4: the team's) */
        }
        if (art_save(path, apx, ART_BANNER_W, ART_BANNER_H, apal, ART_BANNER_COLOURS, 0) < 0) return fail("%s", art_error());
        snprintf(path, sizeof path, "%.900s/art/ice/%.3s.png", dir, T[t].abbr);
        if (art_ice(s, t, apx, &w, &h, apal) < 0) return fail("%s's centre ice cannot be read", T[t].abbr);
        if (art_save(path, apx, w, h, apal, 16, 0) < 0) return fail("%s", art_error());
    }
    if (shared_extract(s, dir) < 0) return -1;
    for (int k = 0; k < nnamed; k++) {
        uint8_t px[ART_LOGO * ART_LOGO];
        uint16_t pal[16];
        char path[1200];
        art_photo_palette(s, pal);
        snprintf(path, sizeof path, "%.900s/art/photos/%.40s.png", dir, named[k].name);
        if (art_photo(s, named[k].pic, px) < 0) return fail("the photo %s cannot be read", named[k].name);
        if (art_save(path, px, ART_LOGO, ART_LOGO, pal, 16, 0) < 0) return fail("%s", art_error());
    }
    char path[1100];
    snprintf(path, sizeof path, "%s/playoffs.csv", dir);
    remove(path);
    if (draws) {
        if (!(f = create(dir, "playoffs.csv"))) return -1;
        for (int k = 1; k <= 16; k++) fprintf(f, "%d%c", k, k < 16 ? ',' : '\n');
        for (int r = 0; r < s->rows; r++)
            for (int k = 0; k < 16; k++) field(f, T[seats[r][k]].abbr, k == 15);
        fclose(f);
    }
    return 0;
}

/* ---- a roster set as a package: its files in a zip, to keep or send ------------------------------------ */

/* the files of dir a roster set reads, into w: 0, -1 */
static int pack_files(const char *dir, mdrt_zip_writer *w, int *count)
{
    static const char *const files[] = { "rosters.ini", "teams.csv", "goalies.csv", "players.csv", "lines.csv", "colours.csv", "pictures.csv", "playoffs.csv" };
    static const char *const kinds[] = { "logos", "banners", "ice", "photos", "game" };
    char path[1200], rel[400];
    uint8_t *b;
    for (size_t k = 0; k < sizeof files / sizeof files[0]; k++) {
        snprintf(path, sizeof path, "%s/%s", dir, files[k]);
        long n = file_bytes(path, &b, 4 << 20);
        if (n < 0) return -1;
        if (!b) continue;
        int r = mdrt_zip_add(w, files[k], b, (size_t)n);
        free(b);
        if (r < 0) return fail("%s cannot be packed", path);
        ++*count;
    }
    for (size_t k = 0; k < sizeof kinds / sizeof kinds[0]; k++) {
        snprintf(path, sizeof path, "%s/art/%s", dir, kinds[k]);
        DIR *d = opendir(path);
        struct dirent *e;
        while (d && (e = readdir(d))) {
            size_t n = strlen(e->d_name);
            if (e->d_name[0] == '.' || n < 5 || (!same_word(e->d_name + n - 4, ".png") && !same_word(e->d_name + n - 4, ".bin"))) continue;
            snprintf(rel, sizeof rel, "art/%s/%.300s", kinds[k], e->d_name);
            snprintf(path, sizeof path, "%s/%s", dir, rel);
            long len = file_bytes(path, &b, 16u << 20);
            if (len < 0 || !b) { closedir(d); return len < 0 ? -1 : fail("%s cannot be read", path); }
            int r = mdrt_zip_add(w, rel, b, (size_t)len);
            free(b);
            if (r < 0) { closedir(d); return fail("%s cannot be packed (its name: letters, digits, . _ - only)", path); }
            ++*count;
        }
        if (d) closedir(d);
    }
    return 0;
}

int rosters_pack(const char *dir, const char *zip_path)
{
    struct built B;
    char name[64];
    err_file = NULL;
    err_line = 0;
    if (is_package(dir)) return fail("%s is a package already", dir);
    if (!is_package(zip_path)) return fail("%s: a package's name ends in .zip", zip_path);
    if (build_dir(dir, &B, name, sizeof name) < 0) return -1;   /* (it builds: else what is wrong) */
    uint64_t hash = B.hash;
    built_free(&B);
    mdrt_zip_writer w = { 0 };
    int count = 0;
    if (pack_files(dir, &w, &count) < 0) { mdrt_zip_writer_free(&w); return -1; }
    uint8_t *zip;
    size_t n = mdrt_zip_finish(&w, &zip);
    if (!n) return fail("%s cannot be made", zip_path);
    FILE *f = fopen(zip_path, "wb");
    int ok = f && fwrite(zip, 1, n, f) == n;
    if (f && fclose(f)) ok = 0;
    uint64_t pack_hash = fnv64(0xCBF29CE484222325ull, zip, n);
    free(zip);
    if (!ok) return fail("cannot write %s", zip_path);
    if (build_dir(zip_path, &B, NULL, 0) < 0) return -1;  /* (and the package builds the same roster set) */
    built_free(&B);
    if (B.hash != hash) return fail("%s builds another roster set than %s", zip_path, dir);
    printf("%s: %s, %d files, %u KB; the package's checksum %016llx, its roster set's %016llx\n", zip_path, name, count,
           (unsigned)((n + 1023) >> 10), (unsigned long long)pack_hash, (unsigned long long)hash);
    return 0;
}
