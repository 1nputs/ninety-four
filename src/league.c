/* The league (league.h): the cartridge's until another is built. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "league.h"
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

struct league league = RETAIL;                           /* (the cartridge's from the start) */
const struct league league_cartridge = RETAIL;

void league_retail(void) { league = league_cartridge; league_read(); }

uint32_t league_crowd_record(uint16_t team)
{
    if (team < SYM_NumTeams) return SYM_CrowdRecBase + (uint32_t)team * SYM_CrowdRecBytes;
    return LEAGUE_SRAM_CROWD + (uint32_t)(team - SYM_NumTeams) * SYM_CrowdRecBytes;
}

/* ---- the teams, natively (league.h) ------------------------------------------------------------ */

struct LgTeam lg_team[LEAGUE_MAX];
uint8_t lg_seats[32][16];

static uint16_t be16(uint32_t a) { const uint8_t *p = rom_ptr(a); return (uint16_t)(p[0] << 8 | p[1]); }
static uint32_t be32(uint32_t a) { return (uint32_t)be16(a) << 16 | be16(a + 2); }

void league_bad_team(uint16_t team)
{
    fprintf(stderr, "league: team %u is past the league's %d\n", team, LEAGUE_MAX);
    abort();
}

/* the String at a (as it is: its length word, bytes and pad), on past it */
static void read_text(uint32_t *a, struct LgText *t)
{
    uint16_t n = be16(*a);
    if (n > LG_TEXT) { fprintf(stderr, "league: a String of %u bytes at $%06X\n", n, *a); abort(); }
    t->at = *a;
    memcpy(t->s, rom_ptr(*a), n);
    *a += n;
}

/* the tables from the cartridge's formats, at the league's addresses (league.table: the
 * cartridge's, or a league built past it) */
void league_read(void)
{
    memset(lg_team, 0, sizeof lg_team);
    for (uint16_t t = 0; t < league.teams && t < LEAGUE_MAX; t++) {
        struct LgTeam *T = &lg_team[t];
        uint32_t b = be32(LG(TeamList) + 4u * t);
        T->data = b;
        for (int k = 0; k < 16; k++) {
            T->colours[0][k] = be16(b + be16(b + 2) + 2u * k);
            T->colours[1][k] = be16(b + be16(b + 2) + 32 + 2u * k);
            T->matchup[0][k] = be16(LG(TeamPalettes) + 64u * t + 2u * k);
            T->matchup[1][k] = be16(LG(TeamPalettes) + 64u * t + 32 + 2u * k);
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
            if (T->players == LG_ROSTER) { fprintf(stderr, "league: team %u has more than %d players\n", t, LG_ROSTER); abort(); }
            struct LgPlayer *P = &T->player[T->players];
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
        T->rating = *rom_ptr(LG(TeamRatings) + t);
        memcpy(T->songs, rom_ptr(LG(TeamSongs) + 6u * t), 6);
        T->records = be16(LG(PlayerRecordOffsets) + 2u * t);
        T->pictures_at = be32(LG(FeaturedPictures) + 4u * t);
        for (uint32_t a = T->pictures_at; be32(a) != 0; a += 6) {
            if (T->pictures == LG_PICTURES) { fprintf(stderr, "league: team %u has more than %d pictures\n", t, LG_PICTURES); abort(); }
            T->picture[T->pictures].pic = be32(a);
            T->picture[T->pictures++].slot = be16(a + 4);
        }
        if (LG(OptionTeamNames)) { uint32_t a = be32(LG(OptionTeamNames) + 4u * t); read_text(&a, &T->option_name); }
    }
    memset(lg_seats, 0, sizeof lg_seats);
    memcpy(lg_seats, rom_ptr(LG(playoffseats)), 16u * league.playoff_rows);
}
