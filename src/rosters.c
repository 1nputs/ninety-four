/* The roster set (rosters.h): the cartridge's until another is built. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "rosters.h"
#include "rom.h"
#include "symbols_gen.h"

#define RETAIL { { SYM_TeamList, SYM_TeamPalettes, SYM_TeamLogoBitmaps, SYM_TeamGfxList, SYM_TeamSongs, SYM_TeamRatings, \
                   SYM_TeamLogoPalettes, SYM_FeaturedPictures, SYM_playoffseats, SYM_PlayerRecordOffsets, SYM_Teamblocksmap, 0, 0 }, \
                 SYM_NumTeams, SYM_NumPlayoffTeams, SYM_PlayoffRows, \
                 { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, \
                   28, 29, 30, 31 }, \
                 { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, \
                   28, 29, 30, 31 }, { 0 }, { 0 }, \
                 { SYM_GameSetUpMap, SYM_GameSetUpMap2, SYM_TitleScreenImg, SYM_NHLShieldImg, \
                   SYM_PAlogoImg, SYM_TitleImg, SYM_ScoutMap, SYM_FramerMap, SYM_FaceOffMap, \
                   SYM_Rinktilelist, SYM_Rinktiles, SYM_RefsMap, SYM_RefMap2, SYM_Sprites, \
                   SYM_Spritetiles, SYM_CrowdFrameList, SYM_FaceOffSprites, SYM_ZamFrameList, \
                   SYM_BigFontMap, SYM_SmallFontMap, SYM_EnergyBarMap, SYM_EASNmap, SYM_Arrowsmap, \
                   SYM_RonBarrMap, SYM_ScoresMap, SYM_EASportsMap, SYM_RevRinkTilelist, \
                   SYM_RevRinkTiles, SYM_ReplayMap, SYM_HorRinkMap, SYM_HorRinkTiles, \
                   SYM_PrintFont2Map, SYM_SetupMenuMap, SYM_SetupFramerMap, SYM_LogoBoxMap, \
                   SYM_PicturePalette, SYM_NoPicSkater1, SYM_NoPicSkater2, SYM_NoPicGoalie1, \
                   SYM_NoPicGoalie2, SYM_PlayerCardsScreenPic, SYM_CornerLogoMap, SYM_ArenaGfxBank, \
                   SYM_PlayoffSprite, SYM_HiScoreImg, SYM_HotIconMap, SYM_ColdIconMap, \
                   SYM_TeamBitmaps }, \
                 { { 14, 3 }, { 16, 16 } } }              /* (title94.asm's HiScoreScreen places) */

struct rosters rosters = RETAIL;                           /* (the cartridge's from the start) */
const struct rosters rosters_cartridge = RETAIL;

void rosters_retail(void) { rosters = rosters_cartridge; rosters_read(); }

uint32_t rosters_crowd_record(uint16_t team)
{
    if (team < SYM_NumTeams) return SYM_CrowdRecBase + (uint32_t)team * SYM_CrowdRecBytes;
    return ROSTERS_SRAM_CROWD + (uint32_t)(team - SYM_NumTeams) * SYM_CrowdRecBytes;
}

/* ---- the teams, natively (rosters.h) ------------------------------------------------------------ */

struct RosterTeam roster_team[ROSTERS_MAX];
uint8_t roster_seats[32][16];

static uint16_t be16(uint32_t a) { const uint8_t *p = rom_ptr(a); return (uint16_t)(p[0] << 8 | p[1]); }
static uint32_t be32(uint32_t a) { return (uint32_t)be16(a) << 16 | be16(a + 2); }

void rosters_bad_team(uint16_t team)
{
    fprintf(stderr, "roster set: team %u is past the roster set's %d\n", team, ROSTERS_MAX);
    abort();
}

/* the String at a (as it is: its length word, bytes and pad), on past it */
static void read_text(uint32_t *a, struct RosterText *t)
{
    uint16_t n = be16(*a);
    if (n > RS_TEXT) { fprintf(stderr, "roster set: a String of %u bytes at $%06X\n", n, *a); abort(); }
    t->at = *a;
    memcpy(t->s, rom_ptr(*a), n);
    *a += n;
}

/* the tables from the cartridge's formats, at the roster set's addresses (rosters.table: the
 * cartridge's, or a roster set built past it) */
void rosters_read(void)
{
    memset(roster_team, 0, sizeof roster_team);
    for (uint16_t t = 0; t < rosters.teams && t < ROSTERS_MAX; t++) {
        struct RosterTeam *T = &roster_team[t];
        uint32_t b = be32(RS(TeamList) + 4u * t);
        T->data = b;
        for (int k = 0; k < 16; k++) {
            T->colours[0][k] = be16(b + be16(b + 2) + 2u * k);
            T->colours[1][k] = be16(b + be16(b + 2) + 32 + 2u * k);
            T->matchup[0][k] = be16(RS(TeamPalettes) + 64u * t + 2u * k);
            T->matchup[1][k] = be16(RS(TeamPalettes) + 64u * t + 32 + 2u * k);
        }
        const uint8_t *sr = rom_ptr(b + be16(b + 8));
        T->offense = sr[0] >> 4; T->defense = sr[0] & 15;
        T->pk = sr[1] >> 4; T->pp = sr[1] & 15;
        T->home = sr[2] >> 4; T->away = sr[2] & 15;
        T->forwards = sr[3] >> 4; T->defensemen = sr[3] & 15;
        T->starts = be16(b + be16(b + 10));
        uint16_t w = T->starts;                         /* (as GetGoalieCount counts them: to its last nibble not 0) */
        do T->goalies++; while ((w = (uint16_t)(w << 4)) != 0);
        memcpy(T->lines, rom_ptr(b + be16(b + 6)), sizeof T->lines);
        uint32_t p = b + be16(b);
        for (; be16(p) != 2; T->players++) {
            if (T->players == RS_ROSTER) { fprintf(stderr, "roster set: team %u has more than %d players\n", t, RS_ROSTER); abort(); }
            struct RosterPlayer *P = &T->player[T->players];
            read_text(&p, &P->name);
            const uint8_t *a = rom_ptr(p);
            P->number = a[0];
            for (int k = 0; k < LR_RATINGS; k++) P->rating[k] = (uint8_t)(a[1 + k / 2] >> (k & 1 ? 0 : 4) & 15);
            p += 8;
        }
        p = b + be16(b + 4);
        read_text(&p, &T->city);
        read_text(&p, &T->abbr);
        read_text(&p, &T->nickname);
        read_text(&p, &T->arena);
        T->rating = *rom_ptr(RS(TeamRatings) + t);
        memcpy(T->songs, rom_ptr(RS(TeamSongs) + 6u * t), 6);
        T->records = be16(RS(PlayerRecordOffsets) + 2u * t);
        T->pictures_at = be32(RS(FeaturedPictures) + 4u * t);
        for (uint32_t a = T->pictures_at; be32(a) != 0; a += 6) {
            if (T->pictures == RS_PICTURES) { fprintf(stderr, "roster set: team %u has more than %d pictures\n", t, RS_PICTURES); abort(); }
            T->picture[T->pictures].pic = be32(a);
            T->picture[T->pictures++].slot = be16(a + 4);
        }
        if (RS(OptionTeamNames)) { uint32_t a = be32(RS(OptionTeamNames) + 4u * t); read_text(&a, &T->option_name); }
        T->logo = be32(RS(TeamLogoBitmaps) + 4u * t);
        T->ice = be32(RS(TeamGfxList) + 4u * t);
        for (int k = 0; k < 16; k++) T->logo_colours[k] = be16(RS(TeamLogoPalettes) + 32u * t + 2u * k);
    }
    memset(roster_seats, 0, sizeof roster_seats);
    memcpy(roster_seats, rom_ptr(RS(playoffseats)), 16u * rosters.playoff_rows);
}
