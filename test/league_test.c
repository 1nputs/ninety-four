/* Leagues (native/league.c), built on the cartridge:
 *
 *   league_test ROM CARTRIDGE_DIR [OTHER_DIR] [--hack HACK DIR ...]
 *
 * CARTRIDGE_DIR is the cartridge's league as nhl94 --export-league writes it: built again it must
 * be the cartridge's, its tables (league.h) as the game reads them off the cartridge, field for
 * field, and its art's tables entry for entry (the counts, the playoff draws too), on an untouched
 * cartridge. OTHER_DIR (tools/leaguetest.py: 32 teams) must
 * build with its save RAM records apart from each other and from the cartridge's, with every
 * playoff team in a draw of 16 different ones, and with banners of their own for the teams whose
 * cities the cartridge has none for. A hack on the 32-team template (HACK), extracted to DIR, must
 * build to its teams. The banners' letters (banner.c) must all be read off the cartridge, and its
 * banners drawn again from them as they are. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "native.h"
#include "league.h"
#include "rom.h"
#include "symbols_gen.h"

static int checks, failed;
#define CHECK(c, ...) do { checks++; if (!(c)) { failed++; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static uint16_t r16(const uint8_t *m, uint32_t a) { return (uint16_t)(m[a] << 8 | m[a + 1]); }
static uint32_t r32(const uint8_t *m, uint32_t a) { return (uint32_t)r16(m, a) << 16 | r16(m, a + 2); }

/* a String as text (its bytes but 0s) */
static const char *string_of(const uint8_t *s)
{
    static char t[4][64];
    static int k;
    char *o = t[k++ & 3];
    uint16_t n = r16(s, 0);
    int j = 0;
    for (uint32_t i = 2; i < n && j < 63; i++) if (s[i]) o[j++] = (char)s[i];
    o[j] = 0;
    return o;
}
static const char *text_of(const struct LgText *x) { return string_of(x->s); }

/* the tables (league.h) the game reads off ROM img (size bytes) by its tables' addresses, as
 * league_read reads them, into teams and seats (the game's own as they were) */
static struct LgTeam want[LEAGUE_MAX];
static uint8_t want_seats[32][16];
static void read_tables(const uint8_t *img, uint32_t size, const uint32_t table[LG_TABLES], int teams, int rows)
{
    static struct LgTeam keep[LEAGUE_MAX];
    static uint8_t keep_seats[32][16];
    memcpy(keep, lg_team, sizeof keep);
    memcpy(keep_seats, lg_seats, sizeof keep_seats);
    const uint8_t *ki = rom_image;
    uint32_t kl = rom_limit;
    struct league kg = league;
    rom_image = img;
    rom_limit = size;
    memcpy(league.table, table, sizeof league.table);
    league.teams = (uint16_t)teams;
    league.playoff_rows = (uint16_t)rows;
    league_read();
    memcpy(want, lg_team, sizeof want);
    memcpy(want_seats, lg_seats, sizeof want_seats);
    rom_image = ki;
    rom_limit = kl;
    league = kg;
    memcpy(lg_team, keep, sizeof keep);
    memcpy(lg_seats, keep_seats, sizeof keep_seats);
}

/* what of team a is not as team b: "" when all is (but its data and Strings' addresses; and the
 * parts SKIP_ leaves out) */
enum { SKIP_DEFENSEMEN = 1, SKIP_RECORDS = 2, SKIP_PICTURES = 4, SKIP_OPTION = 8 };
static const char *team_diff(const struct LgTeam *a, const struct LgTeam *b, int skip)
{
    static char why[200];
    why[0] = 0;
#define DIFF(c, what) do { if (c) { strcat(why, " "); strcat(why, what); } } while (0)
    DIFF(memcmp(a->city.s, b->city.s, LG_TEXT) || memcmp(a->abbr.s, b->abbr.s, LG_TEXT) || memcmp(a->nickname.s, b->nickname.s, LG_TEXT)
         || memcmp(a->arena.s, b->arena.s, LG_TEXT), "names");
    DIFF(memcmp(a->colours, b->colours, sizeof a->colours), "jerseys");
    DIFF(memcmp(a->matchup, b->matchup, sizeof a->matchup), "matchup-colours");
    DIFF(a->offense != b->offense || a->defense != b->defense || a->pp != b->pp || a->pk != b->pk || a->home != b->home || a->away != b->away, "bonuses");
    DIFF(a->forwards != b->forwards || a->goalies != b->goalies || a->starts != b->starts
         || (!(skip & SKIP_DEFENSEMEN) && a->defensemen != b->defensemen), "counts");
    DIFF(memcmp(a->lines, b->lines, sizeof a->lines), "lines");
    int players = a->players == b->players;
    for (int i = 0; players && i < a->players; i++)
        players = !memcmp(a->player[i].name.s, b->player[i].name.s, LG_TEXT) && a->player[i].number == b->player[i].number
                  && !memcmp(a->player[i].rating, b->player[i].rating, sizeof a->player[i].rating);
    DIFF(!players, "players");
    DIFF(a->rating != b->rating || memcmp(a->songs, b->songs, sizeof a->songs), "rating-songs");
    DIFF(!(skip & SKIP_RECORDS) && a->records != b->records, "records");
    int pictures = a->pictures == b->pictures;
    for (int i = 0; pictures && i < a->pictures; i++)
        pictures = a->picture[i].slot == b->picture[i].slot && ((skip & SKIP_PICTURES) || a->picture[i].pic == b->picture[i].pic);
    DIFF(!pictures, "pictures");
    DIFF(!(skip & SKIP_OPTION) && memcmp(a->option_name.s, b->option_name.s, LG_TEXT), "setup-name");
#undef DIFF
    return why;
}

/* the setup screen's names (option_name): each team's city, or its whole name when another team
 * has its city; the count of those that are right */
static int option_names(const char *dir)
{
    int right = 0;
    for (int t = 0; t < league.teams; t++) {
        char city[64], want_name[130];
        snprintf(city, sizeof city, "%s", text_of(&lg_team[t].city));
        int shared = 0;
        for (int k = 0; k < league.teams; k++) shared += k != t && !strcmp(text_of(&lg_team[k].city), city);
        snprintf(want_name, sizeof want_name, shared ? "%s %s" : "%s", city, text_of(&lg_team[t].nickname));
        int same = !strcmp(text_of(&lg_team[t].option_name), want_name);
        CHECK(same, "%s: team %d's name on the setup screen: \"%s\", not \"%s\"", dir, t, text_of(&lg_team[t].option_name), want_name);
        right += same;
    }
    return right;
}

static void round_trip(const char *dir)
{
    league_next(dir);
    league_power_on();
    const uint8_t *c = rom_cart, *m = rom_image;
    CHECK(league_hash() != 0 && m != c && rom_limit > ROM_SIZE, "%s did not build: %s", dir, league_error());
    if (m == c) return;
    CHECK(memcmp(m, c, ROM_SIZE) == 0, "the cartridge's own bytes changed");
    CHECK(league.teams == SYM_NumTeams && league.playoff_teams == SYM_NumPlayoffTeams && league.playoff_rows == SYM_PlayoffRows,
          "the counts: %d %d %d", league.teams, league.playoff_teams, league.playoff_rows);
    struct romsrc cs;                                     /* (the screen after the EA logo: the cartridge's places, read off it) */
    romsrc_cartridge(&cs, c);
    CHECK(!memcmp(league.hiscore_at, league_cartridge.hiscore_at, sizeof league.hiscore_at) && !memcmp(cs.hiscore_at, league_cartridge.hiscore_at, sizeof cs.hiscore_at),
          "the screen after the EA logo's places: %d,%d %d,%d", league.hiscore_at[0][0], league.hiscore_at[0][1], league.hiscore_at[1][0], league.hiscore_at[1][1]);
    read_tables(c, ROM_SIZE, league_cartridge.table, SYM_NumTeams, SYM_PlayoffRows);   /* (the cartridge's, as the game reads them) */
    int teams = 0, tables = 0;
    for (uint32_t t = 0; t < SYM_NumTeams; t++) {
        const char *why = team_diff(&lg_team[t], &want[t], SKIP_OPTION);
        CHECK(!*why && lg_team[t].data == LG_DATA(t), "team %u's tables:%s (data %06X)", t, why, lg_team[t].data);
        teams += !*why;
        static const struct { int table; uint32_t cart, size; } tab[] = {   /* (the art's: by address yet) */
            { LG_TeamLogoBitmaps, SYM_TeamLogoBitmaps, 4 }, { LG_TeamGfxList, SYM_TeamGfxList, 4 }, { LG_TeamLogoPalettes, SYM_TeamLogoPalettes, 32 },
        };
        for (size_t k = 0; k < sizeof tab / sizeof tab[0]; k++) {
            int same = memcmp(m + league.table[tab[k].table] + t * tab[k].size, c + tab[k].cart + t * tab[k].size, tab[k].size) == 0;
            CHECK(same, "team %u's entry of table %d", t, tab[k].table);
            tables += same;
        }
        CHECK(league.art[t] == t, "team %u shows team %d's art", t, league.art[t]);
    }
    CHECK(memcmp(lg_seats, want_seats, 16 * SYM_PlayoffRows) == 0, "the playoff draws");
    CHECK(!LG(TeamList) && !LG(TeamPalettes) && !LG(TeamSongs) && !LG(TeamRatings) && !LG(FeaturedPictures) && !LG(playoffseats)
          && !LG(PlayerRecordOffsets) && !LG(OptionTeamNames), "the teams' tables past the cartridge");
    int own = 0;
    for (int t = 0; t < league.teams; t++) own += league.block_tiles[t] != 0 || league.banner_tiles[t] != 0 || league.block[t] != t;
    CHECK(LG(Teamblocksmap) == SYM_Teamblocksmap && own == 0, "the cartridge's banners (%d teams with others)", own);
    int names = option_names(dir);                         /* (the two New York teams' as the cartridge's own) */
    CHECK(!strcmp(text_of(&lg_team[13].option_name), string_of(c + SYM_IslandersTxt))
          && !strcmp(text_of(&lg_team[14].option_name), string_of(c + SYM_RangersTxt)), "the New York teams' names");
    printf("  %s: %d of %d names on the setup screen as the cartridge shows them\n", dir, names, league.teams);
    printf("  %s: %d of %d teams' tables and %d of %d art table entries the cartridge's\n", dir, teams, SYM_NumTeams, tables, 3 * SYM_NumTeams);
    league_next(NULL);
    league_power_on();
    read_tables(rom_cart, ROM_SIZE, league_cartridge.table, SYM_NumTeams, SYM_PlayoffRows);
    CHECK(rom_image == rom_cart && rom_limit == ROM_SIZE && league_hash() == 0 && LG(TeamList) == SYM_TeamList && !memcmp(lg_team, want, sizeof want)
          && lg_team[0].data == r32(c, SYM_TeamList), "back to the cartridge's");
}

static void other(const char *dir)
{
    league_next(dir);
    league_power_on();
    const uint8_t *m = rom_image;
    CHECK(league_hash() != 0, "%s did not build: %s", dir, league_error());
    if (!league_hash()) return;
    int teams = league.teams, rows = league.playoff_rows;
    printf("  %s: %d teams, %d for the cup, %d draws\n", dir, teams, league.playoff_teams, rows);
    /* save RAM: each team's 26 player records and crowd record apart, below the line data */
    static uint8_t used[0x2000];
    memset(used, 0, sizeof used);
    for (uint32_t a = SYM_TeamRecBase; a < SYM_NameLogBase + SYM_NameLogBytes; a++) used[a] = 1;   /* (the users' records and names) */
    for (uint32_t a = SYM_LineDataBase; a < 0x2000; a++) used[a] = 1;
    int clash = 0;
    for (int t = 0; t < teams; t++) {
        uint32_t p = (uint32_t)lg_team[t].records << 2, cr = league_crowd_record((uint16_t)t);
        for (uint32_t a = p; a < p + 26 * 4; a++) clash += a >= 0x2000 || used[a]++;
        for (uint32_t a = cr; a < cr + 16; a++) clash += a >= 0x2000 || used[a]++;
    }
    CHECK(clash == 0, "%d save RAM bytes of records overlap", clash);
    int missing = 0, twice = 0;
    for (int t = 0; t < league.playoff_teams; t++) {
        int seen = 0;
        for (int i = 0; i < rows * 16; i++) seen += lg_seats[i / 16][i % 16] == t;
        missing += !seen;
    }
    for (int r = 0; r < rows; r++) {
        uint32_t in = 0;
        for (int k = 0; k < 16; k++) {
            int t = lg_seats[r][k];
            twice += t >= league.playoff_teams || (in >> t & 1);
            in |= 1u << t;
        }
    }
    CHECK(missing == 0 && twice == 0, "the draws: %d playoff teams in none, %d places repeated or not playoff teams", missing, twice);
    int own = 0, all = 0;
    for (int t = 0; t < teams; t++) {                     /* banners: their own for the new cities, every team's for the setup screen */
        own += league.block_tiles[t] != 0 && league.block[t] >= SYM_NumTeams;
        all += league.banner_tiles[t] != 0;
        CHECK((league.block_tiles[t] != 0) == (league.block[t] >= SYM_NumTeams), "team %d's block row %d", t, league.block[t]);
    }
    printf("  %s: %d teams with banners of their own\n", dir, own);
    CHECK(own == 4 && all == teams && LG(Teamblocksmap) >= ROM_SIZE && LG(BannerSlotMap) >= ROM_SIZE, "banners: %d own, %d drawn", own, all);
    uint32_t tb = LG(Teamblocksmap), count = r16(m, tb + 8);
    CHECK(!(count & 0x8000) && count == 501 + 22 * 4 && (tb + 10) >> 17 == (tb + 10 + 32 * count - 1) >> 17, "the league's Teamblocksmap: %u tiles", count);
    for (int t = 0; t < teams; t++) {                     /* every team's players, its own data */
        int n = lg_team[t].players;
        CHECK(n >= 17 && n <= 25 && lg_team[t].data == LG_DATA(t) && n == lg_team[t].goalies + lg_team[t].forwards + lg_team[t].defensemen,
              "team %d's players (%d)", t, n);
    }
    /* its own art (tools/leaguetest.py): HOU's logo in plain RGB (a corner transparent, then red,
     * then white: colours 0, 1, 2 in the order they come), BOS's with its first pixel changed,
     * KC's photo in colours (a ramp: black to white by brightness) */
    struct romsrc bs, cs;
    romsrc_cartridge(&cs, rom_cart);
    romsrc_cartridge(&bs, rom_image);
    bs.size = rom_limit;
    memcpy(bs.table, league.table, sizeof bs.table);
    uint8_t px[ART_LOGO * ART_LOGO], cart[ART_LOGO * ART_LOGO];
    uint16_t pal[16], cpal[16];
    int hou = 29, bos = 1, kc = 27, drawn = 0;
    CHECK(art_logo(&bs, hou, px, pal) == 0 && r32(m, LG(TeamLogoBitmaps) + 4u * (uint32_t)hou) >= ROM_SIZE, "HOU's logo its own");
    for (int i = 0; i < ART_LOGO * ART_LOGO; i++) drawn += px[i] == (i % 48 < 8 && i / 48 < 8 ? 0 : i % 48 < 24 ? 1 : 2);
    CHECK(drawn == ART_LOGO * ART_LOGO && pal[1] == art_word(0xFF0000) && pal[2] == art_word(0xFFFFFF), "HOU's logo as drawn (%d pixels)", drawn);
    CHECK(art_logo(&bs, bos, px, pal) == 0 && art_logo(&cs, bos, cart, cpal) == 0 && r32(m, LG(TeamLogoBitmaps) + 4u * (uint32_t)bos) >= ROM_SIZE
          && px[0] == 1 && cart[0] != 1 && !memcmp(px + 1, cart + 1, sizeof px - 1) && !memcmp(pal, cpal, sizeof pal), "BOS's logo, a pixel changed");
    uint32_t pic = lg_team[kc].pictures ? lg_team[kc].picture[lg_team[kc].pictures - 1].pic : 0;
    CHECK(pic >= ROM_SIZE && art_photo(&bs, pic, px) == 0 && px[0] == 6 && px[47] == 5, "KC's photo its own, its shades by brightness");
    /* the players' sprites with a pixel of their first tile changed (tools/leaguetest.py): their own,
     * the rest of them the cartridge's, their tiles where the game looks (Spritetiles) */
    int sp = -1;
    for (int i = 0; i < art_shared(); i++) if (art_shared_asset(i) == ART_Sprites) sp = i;
    uint8_t *mine = NULL, *theirs = NULL;
    size_t mn = art_shared_copy(&bs, sp, ART(Sprites), &mine), cn = art_shared_copy(&cs, sp, SYM_Sprites, &theirs);
    int changed = 0;
    for (size_t k = 0; mn == cn && k < mn; k++) changed += mine[k] != theirs[k];
    CHECK(ART(Sprites) >= ROM_SIZE && ART(Spritetiles) == ART(Sprites) + 10 && mn == cn && changed == 1 && mine[10] != theirs[10],
          "the sprites' first pixel changed (%d bytes changed)", changed);
    free(mine);
    free(theirs);
    printf("  %s: art of its own: a logo drawn, a logo changed, a photo in colours, a sprite's pixel\n", dir);
    league_next(NULL);
    league_power_on();
}

/* a hack's league (romsrc.c), extracted to dir and built: its teams as the hack has them (what the
 * game reads of each block, byte for byte, but for the count of defensemen, which the game counts
 * itself), their colours (jerseys, logo, matchups), its ratings and songs,
 * its playoff draws but for the places a team in none was given, and its art (art.c): each
 * logo and its colours, centre ice, both banners and photo the hack's, pixel for pixel */
static void hack(const char *path, const char *dir)
{
    static uint8_t h[0x400001];
    FILE *f = fopen(path, "rb");
    if (!f) { CHECK(0, "cannot read %s", path); return; }
    uint32_t n = (uint32_t)fread(h, 1, sizeof h, f);
    fclose(f);
    struct romsrc s;
    char why[300];
    CHECK(romsrc_hack(&s, h, n, rom_cart, why, sizeof why) == 0, "%s: %s", path, why);
    league_next(dir);
    league_power_on();
    const uint8_t *m = rom_image;
    CHECK(league_hash() != 0, "%s did not build: %s", dir, league_error());
    if (!league_hash()) return;
    CHECK(league.teams == s.teams && league.playoff_teams == s.playoff_teams && league.playoff_rows == s.rows,
          "the counts: %d %d %d, the hack's %d %d %d", league.teams, league.playoff_teams, league.playoff_rows, s.teams, s.playoff_teams, s.rows);
    read_tables(h, n, s.table, s.teams, s.rows);         /* (the hack's, as the game would read them off it) */
    int blocks = 0, moved = 0, colours = 0;
    for (int t = 0; t < league.teams && t < s.teams; t++) {
        /* (what the game reads of a team, but the count of defensemen, which the game counts
         * itself; its pictures' addresses, its records' places, its setup screen name: the
         * league's own) */
        const char *differ = team_diff(&lg_team[t], &want[t], SKIP_DEFENSEMEN | SKIP_RECORDS | SKIP_PICTURES | SKIP_OPTION);
        CHECK(!*differ, "team %d's tables:%s", t, differ);
        blocks += !*differ;
        /* its colours: the jerseys, the logo's and the matchup screens' (the logo's in the art's
         * TeamLogoPalettes) */
        int jerseys = !memcmp(lg_team[t].colours, want[t].colours, sizeof want[t].colours);
        int logo = memcmp(m + LG(TeamLogoPalettes) + 32u * (uint32_t)t, h + s.table[LG_TeamLogoPalettes] + 32u * (uint32_t)t, 32) == 0;
        int matchup = !memcmp(lg_team[t].matchup, want[t].matchup, sizeof want[t].matchup);
        CHECK(jerseys && logo && matchup, "team %d's colours: jerseys %s, logo %s, matchup %s", t, jerseys ? "same" : "DIFFER",
              logo ? "same" : "DIFFER", matchup ? "same" : "DIFFER");
        colours += jerseys && logo && matchup;
    }
    for (int i = 0; i < 16 * s.rows; i++) moved += lg_seats[i / 16][i % 16] != want_seats[i / 16][i % 16];
    int missing = 0;
    for (int t = 0; t < s.playoff_teams; t++) {
        int seen = 0;
        for (int i = 0; i < 16 * s.rows; i++) seen += h[s.table[LG_playoffseats] + (uint32_t)i] == t;
        missing += !seen;
    }
    CHECK(moved == missing, "the draws: %d places changed, for %d teams in none", moved, missing);
    printf("  %s: %d of %d teams' tables the hack's, %d of their colours (jerseys, logo, matchups), %d draw places given to teams in none\n",
           dir, blocks, s.teams, colours, moved);
    /* its art as the hack's: each logo (and its colours), centre ice, both banners, each photo */
    struct romsrc bs;
    romsrc_cartridge(&bs, rom_image);
    bs.size = rom_limit;
    memcpy(bs.table, league.table, sizeof bs.table);
    bs.teams = league.teams;
    int logos = 0, ices = 0, banners = 0, photos = 0, pics = 0, label = 1;
    static uint8_t a[64 * 64 * 4], b[64 * 64 * 4];
    for (int t = 1; t < s.teams && label; t++) {          /* (every team's setup banner the same: the template's label) */
        uint16_t pa[ART_BANNER_COLOURS];
        label = art_banners(&s, 0, a, pa) == 0 && art_banners(&s, t, b, pa) == 0 && !memcmp(a + 16 * ART_BANNER_W, b + 16 * ART_BANNER_W, 16 * ART_BANNER_W);
    }
    for (int t = 0; t < league.teams && t < s.teams; t++) {
        uint16_t pa[ART_BANNER_COLOURS], pb[ART_BANNER_COLOURS];
        int same = art_logo(&bs, t, a, pa) == 0 && art_logo(&s, t, b, pb) == 0 && !memcmp(a, b, ART_LOGO * ART_LOGO) && !memcmp(pa, pb, 32);
        CHECK(same, "team %d's logo", t);
        logos += same;
        uint8_t ta[24 * 32], tb[24 * 32];
        same = art_tiles(&bs, r32(m, LG(TeamGfxList) + 4u * (uint32_t)t) + 8, ta, 24) == 24
               && art_tiles(&s, rs32(&s, s.table[LG_TeamGfxList] + 4u * (uint32_t)t) + 8, tb, 24) == 24 && !memcmp(ta, tb, sizeof ta);
        CHECK(same, "team %d's centre ice", t);
        ices += same;
        /* (the top banner in its row of the league's Teamblocksmap; the setup screen's its 22 tiles,
         * the hack's, or when every team's is the template's label, its city's in the cartridge's letters) */
        same = art_banners(&s, t, b, pb) == 0 && art_banners(&bs, league_block((uint16_t)t), a, pa) == 0 && !memcmp(a, b, 16 * ART_BANNER_W);
        if (label) {
            char city[64], no[120];
            uint8_t name[16][88];
            snprintf(city, sizeof city, "%s", text_of(&lg_team[t].city));
            for (char *c = city; *c; c++) if (*c >= 'a' && *c <= 'z') *c = (char)(*c - 32);
            banner_pixels(BANNER_BITMAPS, league_art((uint16_t)t), banner_check(city, no, sizeof no) == 0 ? city : NULL, name);
            for (int i = 0; i < 16 * ART_BANNER_W; i++) b[16 * ART_BANNER_W + i] = (uint8_t)(ART_BANNER_SETUP + name[i / ART_BANNER_W][i % ART_BANNER_W]);
        }
        for (int i = 0; same && i < 22 * 64; i++) {
            int x = i % 88, y = i / 88, tile = (y / 8) * 11 + x / 8, v = rom_image[league.banner_tiles[t] + (uint32_t)(tile * 32 + (y % 8) * 4 + (x % 8) / 2)];
            same = league.banner_tiles[t] && (x % 2 ? v & 15 : v >> 4) == b[(16 + y) * ART_BANNER_W + x] - ART_BANNER_SETUP;
        }
        CHECK(same, "team %d's banners", t);
        banners += same;
        const struct LgTeam *T = &lg_team[t];
        int k = 0;
        for (; k < T->pictures && k < want[t].pictures; k++) {
            same = art_photo(&bs, T->picture[k].pic, a) == 0 && art_photo(&s, want[t].picture[k].pic, b) == 0 && !memcmp(a, b, ART_LOGO * ART_LOGO)
                   && T->picture[k].slot == want[t].picture[k].slot;
            CHECK(same, "team %d's photo of roster place %d", t, want[t].picture[k].slot);
            photos += same;
            pics++;
        }
        CHECK(T->pictures == want[t].pictures, "team %d's photos: %d, the hack's %d", t, T->pictures, want[t].pictures);
    }
    int set = r16(m, LG(Teamblocksmap) + 8) & 0x7FFF;      /* (the playoff screen loads them all: no more than the hack's 22 a team) */
    CHECK(set <= 22 * league.teams, "%s: %d banner tiles for %d teams (the playoff screen has room for 22 a team)", dir, set, league.teams);
    printf("  %s: the hack's art: %d logos, %d centre ices, %d teams' banners%s, %d of %d photos\n", dir, logos, ices, banners,
           label ? " (the setup screen's: names, for the template's label)" : "", photos, pics);
    CHECK(!memcmp(league.hiscore_at, s.hiscore_at, sizeof s.hiscore_at) && !s.hiscore_at[0][0] && !s.hiscore_at[1][1],
          "%s: the screen after the EA logo's places %d,%d %d,%d, not the hack's 0,0 0,0", dir,
          league.hiscore_at[0][0], league.hiscore_at[0][1], league.hiscore_at[1][0], league.hiscore_at[1][1]);
    printf("  %s: %d of %d names on the setup screen right (the shared cities' whole)\n", dir, option_names(dir), league.teams);
    /* its shared art: each of the hack's own (not the cartridge's) as the hack's, gathered; the rest the cartridge's */
    struct romsrc cs;
    romsrc_cartridge(&cs, rom_cart);
    int own = 0, kept = 0;
    for (int i = 0; i < art_shared(); i++) {
        int k = art_shared_asset(i), off, tk = art_shared_tiles(i, &off);
        uint8_t *hb = NULL, *cb = NULL, *mb = NULL;
        size_t hn = art_shared_copy(&s, i, s.asset[k], &hb), cn = art_shared_copy(&cs, i, cs.asset[k], &cb);
        int mine = hn && (hn != cn || memcmp(hb, cb, hn)), same;
        if (mine) {
            size_t mn = art_shared_copy(&bs, i, league.asset[k], &mb);
            same = league.asset[k] >= ROM_SIZE && mn == hn && !memcmp(mb, hb, hn);
        } else same = league.asset[k] == cs.asset[k];
        if (tk >= 0) same &= league.asset[tk] == league.asset[k] + (uint32_t)off;
        CHECK(same, "the shared %s (%s)", art_shared_name(i), mine ? "the hack's" : "the cartridge's");
        own += mine && same;
        kept += !mine && same;
        free(hb); free(cb); free(mb);
    }
    printf("  %s: its shared art: %d of its own as the hack's, %d the cartridge's\n", dir, own, kept);
    league_next(NULL);
    league_power_on();
}

/* the leagues the window's menu lists, before any cartridge is loaded (as at its start): in the
 * folder dir's leagues/, name among them */
static void listed(const char *dir, const char *name)
{
    char here[1024];
    const char *const *names, *const *labels;
    int found = 0, n = 0;
    if (getcwd(here, sizeof here) && chdir(dir) == 0) {
        n = league_list(&names, &labels);
        for (int i = 0; i < n; i++) found |= !strcmp(names[i], name);
        if (chdir(here) != 0) CHECK(0, "cannot go back to %s", here);
    }
    CHECK(found, "%s/leagues/%s not listed with no cartridge loaded (%d listed)", dir, name, n);
    printf("  %s/leagues: %d listed with no cartridge loaded, %s among them\n", dir, n, name);
}

/* what the League page says of each league in dir/leagues and of the cartridge's (the cartridge
 * loaded): printed, and its lines checked */
static void about(const char *dir)
{
    char here[1024], text[4096];
    const char *const *names, *const *labels;
    if (!getcwd(here, sizeof here) || chdir(dir) != 0) { CHECK(0, "cannot go to %s", dir); return; }
    int n = league_list(&names, &labels);
    CHECK(league_unread() == 1, "%s/leagues: %d left out of the list, not its one broken league", dir, league_unread());
    for (int i = 0; i < n; i++) {
        league_about(i, text, sizeof text);
        printf("  the League page, %s:\n", labels[i]);
        int lines = 0, best = 0, width = 1;
        for (char *l = text, *e; *l; l = e + 1) {
            if (!(e = strchr(l, '\n'))) break;
            *e = 0;
            char *tab = strchr(l, '\t');
            width &= (int)strlen(l) + 1 <= 49;            /* (the menu's room: label, two spaces, the right) */
            printf("    %-24s %s\n", tab ? (*tab = 0, l) : l, tab ? tab + 1 : "");
            best += !strcmp(l, "Best skaters");
            lines++;
        }
        CHECK(lines >= 7 && lines <= 14 && best == 1 && width, "%s: the League page's lines (%d)", labels[i], lines);
    }
    /* a league found elsewhere (the menu's Files line): made an entry of the list; one listed
     * already, by another name, is that entry; a folder that is no league says why */
    char why[300] = "", files[200], place[600];
    int found = league_add("../thirty-two", why, sizeof why), again = league_add("leagues/thirty-two", why, sizeof why), listed_at = -1;
    for (int i = 0; i < n; i++) if (!strcmp(names[i], "thirty-two")) listed_at = i;
    league_files(found, files, sizeof files);
    league_about(found, text, sizeof text);
    CHECK(found == n && !strcmp(labels[found], "Thirty-two teams") && !strcmp(league_list_dir(found, place, sizeof place), "../thirty-two")
          && strstr(files, "../thirty-two, a folder") && strstr(text, "Teams\t32 (30 cup)"), "a league found elsewhere: %d (%s; %s)", found, files, why);
    CHECK(again == listed_at && listed_at > 0, "a league listed already, found again: %d, not %d", again, listed_at);
    CHECK(league_add("nowhere", why, sizeof why) < 0 && strstr(why, "teams.csv"), "a folder that is no league: %s", why);
    CHECK(league_add("../thirty-two", why, sizeof why) == found, "a league found twice");
    printf("  a league found elsewhere: entry %d, %s; \"nowhere\": %s\n", found, files, why);
    if (chdir(here) != 0) CHECK(0, "cannot go back to %s", here);
}

int main(int argc, char **argv)
{
    if (argc < 3) { fprintf(stderr, "usage: %s ROM CARTRIDGE_DIR [OTHER_DIR] [--hack HACK DIR ...] [--listed DIR NAME]\n", argv[0]); return 2; }
    for (int i = 3; i + 2 < argc; i++)                    /* (first: before the cartridge is loaded) */
        if (!strcmp(argv[i], "--listed")) listed(argv[i + 1], argv[i + 2]);
    if (!rom_load(argv[1])) { fprintf(stderr, "cannot load %s\n", argv[1]); return 1; }
    int letters = 0, same = 0, banners = 0;
    CHECK(banner_prepare(rom_cart) == 0, "the cartridge's banners");
    banners = banner_selfcheck(&letters, &same);
    printf("  banners: %d letters; %d of %d cartridge banners drawn again from their letters as they are\n", letters, same, banners);
    CHECK(letters == 25 + 14 && same == banners, "the banners' letters");
    round_trip(argv[2]);
    for (int i = 3; i + 2 < argc; i++)
        if (!strcmp(argv[i], "--listed")) about(argv[i + 1]);
    for (int i = 3; i < argc; i++) {
        if (!strcmp(argv[i], "--hack") && i + 2 < argc) { hack(argv[i + 1], argv[i + 2]); i += 2; }
        else if (!strcmp(argv[i], "--listed") && i + 2 < argc) i += 2;
        else other(argv[i]);
    }
    printf("league_test: %d checks, %d failed\n", checks, failed);
    return failed != 0;
}
