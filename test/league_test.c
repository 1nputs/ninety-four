/* Leagues (native/league.c), built on the cartridge:
 *
 *   league_test ROM CARTRIDGE_DIR [OTHER_DIR]
 *
 * CARTRIDGE_DIR is the cartridge's league as nhl94 --export-league writes it: built again it must
 * be the cartridge's, byte for byte (every team block and every entry of every table, the counts,
 * the playoff draws), on an untouched cartridge. OTHER_DIR (tools/leaguetest.py: 32 teams) must
 * build with its save RAM records apart from each other and from the cartridge's, and with every
 * playoff team in a draw of 16 different ones. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "native.h"
#include "league.h"
#include "rom.h"
#include "symbols_gen.h"

static int checks, failed;
#define CHECK(c, ...) do { checks++; if (!(c)) { failed++; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static uint16_t r16(const uint8_t *m, uint32_t a) { return (uint16_t)(m[a] << 8 | m[a + 1]); }
static uint32_t r32(const uint8_t *m, uint32_t a) { return (uint32_t)r16(m, a) << 16 | r16(m, a + 2); }

/* a team block's size: to the end of its arena String */
static uint32_t block_size(const uint8_t *m, uint32_t b)
{
    uint32_t p = b + r16(m, b + 4);
    for (int k = 0; k < 4; k++) p += r16(m, p);
    return p - b;
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
    int blocks = 0, tables = 0;
    for (uint32_t t = 0; t < SYM_NumTeams; t++) {
        uint32_t cb = r32(c, SYM_TeamList + 4 * t), mb = r32(m, LG(TeamList) + 4 * t), n = block_size(c, cb);
        CHECK(mb >= ROM_SIZE && block_size(m, mb) == n && memcmp(m + mb, c + cb, n) == 0, "team %u's block (%u bytes)", t, n);
        blocks += mb >= ROM_SIZE && block_size(m, mb) == n && memcmp(m + mb, c + cb, n) == 0;
        static const struct { int table; uint32_t cart, size; } tab[] = {
            { LG_TeamPalettes, SYM_TeamPalettes, 64 }, { LG_TeamLogoBitmaps, SYM_TeamLogoBitmaps, 4 }, { LG_TeamGfxList, SYM_TeamGfxList, 4 },
            { LG_TeamSongs, SYM_TeamSongs, 6 }, { LG_TeamRatings, SYM_TeamRatings, 1 }, { LG_TeamLogoPalettes, SYM_TeamLogoPalettes, 32 },
            { LG_PlayerRecordOffsets, SYM_PlayerRecordOffsets, 2 },
        };
        for (size_t k = 0; k < sizeof tab / sizeof tab[0]; k++) {
            int same = memcmp(m + league.table[tab[k].table] + t * tab[k].size, c + tab[k].cart + t * tab[k].size, tab[k].size) == 0;
            CHECK(same, "team %u's entry of table %d", t, tab[k].table);
            tables += same;
        }
        uint32_t cp = r32(c, SYM_FeaturedPictures + 4 * t), mp = r32(m, LG(FeaturedPictures) + 4 * t);
        for (;; cp += 6, mp += 6) {                       /* (the lists, entry by entry) */
            CHECK(r32(m, mp) == r32(c, cp) && (!r32(c, cp) || r16(m, mp + 4) == r16(c, cp + 4)), "team %u's pictures", t);
            if (!r32(c, cp) || r32(m, mp) != r32(c, cp)) break;
        }
        CHECK(league.art[t] == t, "team %u shows team %d's art", t, league.art[t]);
    }
    CHECK(memcmp(m + LG(playoffseats), c + SYM_playoffseats, 16 * SYM_PlayoffRows) == 0, "the playoff draws");
    printf("  %s: %d of %d team blocks and %d of %d table entries the cartridge's\n", dir, blocks, SYM_NumTeams, tables, 7 * SYM_NumTeams);
    league_next(NULL);
    league_power_on();
    CHECK(rom_image == rom_cart && rom_limit == ROM_SIZE && league_hash() == 0 && LG(TeamList) == SYM_TeamList, "back to the cartridge's");
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
        uint32_t p = (uint32_t)r16(m, LG(PlayerRecordOffsets) + 2u * (uint32_t)t) << 2, cr = league_crowd_record((uint16_t)t);
        for (uint32_t a = p; a < p + 26 * 4; a++) clash += a >= 0x2000 || used[a]++;
        for (uint32_t a = cr; a < cr + 16; a++) clash += a >= 0x2000 || used[a]++;
    }
    CHECK(clash == 0, "%d save RAM bytes of records overlap", clash);
    int missing = 0, twice = 0;
    for (int t = 0; t < league.playoff_teams; t++) {
        int seen = 0;
        for (int i = 0; i < rows * 16; i++) seen += m[LG(playoffseats) + (uint32_t)i] == t;
        missing += !seen;
    }
    for (int r = 0; r < rows; r++) {
        uint32_t in = 0;
        for (int k = 0; k < 16; k++) {
            int t = m[LG(playoffseats) + 16u * (uint32_t)r + (uint32_t)k];
            twice += t >= league.playoff_teams || (in >> t & 1);
            in |= 1u << t;
        }
    }
    CHECK(missing == 0 && twice == 0, "the draws: %d playoff teams in none, %d places repeated or not playoff teams", missing, twice);
    for (int t = 0; t < teams; t++) {                     /* every block readable: its players to the end word */
        uint32_t b = r32(m, LG(TeamList) + 4u * (uint32_t)t), p = b + r16(m, b);
        int n = 0;
        while (r16(m, p) != 2 && n < 30) { p += r16(m, p) + 8; n++; }
        CHECK(n >= 17 && n <= 25 && p == b + r16(m, b + 4) - 2, "team %d's players (%d)", t, n);
    }
    league_next(NULL);
    league_power_on();
}

int main(int argc, char **argv)
{
    if (argc < 3) { fprintf(stderr, "usage: %s ROM CARTRIDGE_DIR [OTHER_DIR]\n", argv[0]); return 2; }
    if (!rom_load(argv[1])) { fprintf(stderr, "cannot load %s\n", argv[1]); return 1; }
    round_trip(argv[2]);
    if (argc > 3) other(argv[3]);
    printf("league_test: %d checks, %d failed\n", checks, failed);
    return failed != 0;
}
