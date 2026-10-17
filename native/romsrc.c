/* Where a ROM keeps its league: the cartridge's own addresses, or those of a hack made on the
 * community's 32-team template (NHL 94 2025, NHL 2026 94 Edition, 30th Anniversary ...). Those
 * leave the cartridge's code where it is and point it at tables of their own, moved past the 1 MB
 * or into the room of the cartridge's team blocks: so a table is where the hack's code, at the
 * place where the cartridge's reads it, says it is. A hack is only read here, as data, to write a
 * league folder from (league_extract): it is never run. Everything read from it is checked. */
#include <stdio.h>
#include <string.h>
#include "native.h"
#include "rom.h"
#include "symbols_gen.h"

uint8_t rs8(const struct romsrc *s, uint32_t a) { return a < s->size ? s->b[a] : 0; }
uint16_t rs16(const struct romsrc *s, uint32_t a) { return (uint16_t)(rs8(s, a) << 8 | rs8(s, a + 1)); }
uint32_t rs32(const struct romsrc *s, uint32_t a) { return (uint32_t)rs16(s, a) << 16 | rs16(s, a + 2); }

void romsrc_cartridge(struct romsrc *s, const uint8_t *cart)
{
    static const uint32_t retail[LG_TABLES] = { SYM_TeamList, SYM_TeamPalettes, SYM_TeamLogoBitmaps, SYM_TeamGfxList, SYM_TeamSongs,
                                                SYM_TeamRatings, SYM_TeamLogoPalettes, SYM_FeaturedPictures, SYM_playoffseats,
                                                SYM_PlayerRecordOffsets, SYM_Teamblocksmap, 0 };
    memset(s, 0, sizeof *s);
    s->b = cart;
    s->size = ROM_SIZE;
    memcpy(s->table, retail, sizeof retail);
    s->teams = SYM_NumTeams;
    s->playoff_teams = SYM_NumPlayoffTeams;
    s->rows = SYM_PlayoffRows;
}

/* "movea.l #address,An" or "lea address,An": the instructions the cartridge's code reads its
 * tables' addresses with */
static int takes_address(uint16_t op) { return (op & 0xF1FF) == 0x207C || (op & 0xF1FF) == 0x41F9; }

/* the address the hack's code holds where the cartridge's holds addr (the most of its places
 * that agree, the same instruction at each); 0 when none */
static uint32_t moved(const struct romsrc *s, const uint8_t *cart, uint32_t addr)
{
    uint32_t seen[16], votes[16];
    int n = 0;
    for (uint32_t p = 0x202; p + 4 <= ROM_SIZE; p += 2) {
        if (!takes_address((uint16_t)(cart[p - 2] << 8 | cart[p - 1]))) continue;
        if ((uint32_t)(cart[p] << 24 | cart[p + 1] << 16 | cart[p + 2] << 8 | cart[p + 3]) != addr) continue;
        if (rs16(s, p - 2) != (uint16_t)(cart[p - 2] << 8 | cart[p - 1])) continue;    /* (code of its own there) */
        uint32_t v = rs32(s, p);
        int k = 0;
        while (k < n && seen[k] != v) k++;
        if (k == n && n < 16) { seen[n] = v; votes[n++] = 0; }
        if (k < n) votes[k]++;
    }
    int best = -1;
    for (int k = 0; k < n; k++)
        if (best < 0 || votes[k] > votes[best]) best = k;
    return best < 0 ? 0 : seen[best];
}

/* a String at *a (its length word with the pad): plain text of at most max, *a past it */
static int string_ok(const struct romsrc *s, uint32_t *a, int max)
{
    uint16_t n = rs16(s, *a);
    if (n < 2 || n > 2 + max + 1 || *a + n > s->size) return 0;
    for (uint32_t i = 2; i < n; i++) {
        uint8_t ch = s->b[*a + i];
        if (ch && (ch < 0x20 || ch > 0x7E)) return 0;
    }
    *a += n;
    return 1;
}

/* team t's block, readable as the game reads it: its offsets inside the ROM, its players (as
 * many as the game's roster has room for) to the end word, its four names */
int romsrc_block_ok(const struct romsrc *s, int t)
{
    uint32_t b = rs32(s, s->table[LG_TeamList] + 4u * (uint32_t)t);
    if (b & 1 || b < 0x200 || b + 160 > s->size) return 0;
    for (int k = 0; k < 6; k++)
        if (rs16(s, b + 2u * (uint32_t)k) & 0x8000 || b + rs16(s, b + 2u * (uint32_t)k) + 8 > s->size) return 0;
    uint32_t p = b + rs16(s, b);
    int n = 0;
    while (rs16(s, p) != 2) {
        if (++n > 26 || !string_ok(s, &p, 30)) return 0;
        p += 8;
    }
    if (n < 17 || p + 2 != b + rs16(s, b + 4)) return 0;
    p += 2;
    for (int k = 0; k < 4; k++)
        if (!string_ok(s, &p, 30)) return 0;
    return 1;
}

int romsrc_hack(struct romsrc *s, const uint8_t *rom, uint32_t size, const uint8_t *cart, char *why, size_t why_size)
{
    memset(s, 0, sizeof *s);
    s->b = rom;
    s->size = size;
    s->hack = 1;
    if (size <= ROM_SIZE || size > 0x400000) {
        snprintf(why, why_size, "%u KB: a hack on the 32-team template is bigger than the cartridge's 1 MB, at most 4 MB", (unsigned)(size >> 10));
        return -1;
    }
    static const struct { int table; uint32_t addr; } find[] = {
        { LG_TeamList, SYM_TeamList }, { LG_TeamPalettes, SYM_TeamPalettes }, { LG_TeamLogoBitmaps, SYM_TeamLogoBitmaps },
        { LG_TeamGfxList, SYM_TeamGfxList }, { LG_TeamSongs, SYM_TeamSongs }, { LG_TeamRatings, SYM_TeamRatings },
        { LG_TeamLogoPalettes, SYM_TeamLogoPalettes }, { LG_FeaturedPictures, SYM_FeaturedPictures },
        { LG_playoffseats, SYM_playoffseats }, { LG_Teamblocksmap, SYM_Teamblocksmap },
    };
    for (size_t k = 0; k < sizeof find / sizeof find[0]; k++) {
        uint32_t a = moved(s, cart, find[k].addr);
        if (!a || a & 1 || a >= size) {
            snprintf(why, why_size, "not a hack on the 32-team template: its code does not read table %d where the cartridge's does", find[k].table);
            return -1;
        }
        s->table[find[k].table] = a;
    }
    /* the counts: its teams, as many blocks as read well; its playoff teams and draws, from the
     * playoff code (NewPO: moveq #PlayoffRows,d0 ... cmp.w #NumPlayoffTeams-1,d2) */
    while (s->teams < LEAGUE_MAX && romsrc_block_ok(s, s->teams)) s->teams++;
    if (rs8(s, SYM_NewPO) != 0x70 || rs16(s, SYM_NewPO + 0x1E) != 0xB47C || cart[SYM_NewPO] != 0x70) {
        snprintf(why, why_size, "not a hack on the 32-team template: its playoff code is its own");
        return -1;
    }
    s->rows = rs8(s, SYM_NewPO + 1);
    s->playoff_teams = rs16(s, SYM_NewPO + 0x20) + 1;
    if (s->teams < 18) { snprintf(why, why_size, "only %d teams read well (a league has 18 to 32)", s->teams); return -1; }
    if (s->rows < 1 || s->rows > 32 || (s->rows & (s->rows - 1))) { snprintf(why, why_size, "%d playoff draws (a power of two, at most 32)", s->rows); return -1; }
    if (s->playoff_teams > s->teams) s->playoff_teams = s->teams;
    if (s->table[LG_playoffseats] + 16u * (uint32_t)s->rows > size || s->table[LG_TeamPalettes] + 64u * (uint32_t)s->teams > size ||
        s->table[LG_TeamLogoPalettes] + 32u * (uint32_t)s->teams > size || s->table[LG_TeamRatings] + (uint32_t)s->teams > size ||
        s->table[LG_TeamSongs] + 6u * (uint32_t)s->teams > size) {
        snprintf(why, why_size, "its tables run past its end");
        return -1;
    }
    return 0;
}
