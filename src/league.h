/* The league: the teams a game is played with. The cartridge's own (26 clubs and the two All-Star
 * teams) unless the native side builds another (native/league.c) past the cartridge's 1 MB, in the
 * ROM's own formats: a team list of team blocks, and the tables by team. The game reads every
 * table by team, and the counts, through here; with the cartridge's league they are its own
 * addresses and numbers, so it plays exactly as the original. A league is part of a game's
 * session, like its rules. */
#pragma once
#include <stdint.h>

enum { LG_TeamList, LG_TeamPalettes, LG_TeamLogoBitmaps, LG_TeamGfxList, LG_TeamSongs, LG_TeamRatings,
       LG_TeamLogoPalettes, LG_FeaturedPictures, LG_playoffseats, LG_PlayerRecordOffsets, LG_Teamblocksmap,
       LG_BannerSlotMap, LG_OptionTeamNames, LG_TABLES };
#define LEAGUE_MAX 32                                   /* (a team is a byte in the playoff tree and 5 bits elsewhere) */
/* the game's art every team shares (titles, setup screens, the rink, the sprites, the fonts,
 * the stock photos ...), read through asset[] (ART()): the cartridge's own, or a league's (art/game) */
enum { ART_GameSetUpMap, ART_GameSetUpMap2, ART_TitleScreenImg, ART_NHLShieldImg, ART_PAlogoImg,
       ART_TitleImg, ART_ScoutMap, ART_FramerMap, ART_FaceOffMap, ART_Rinktilelist, ART_Rinktiles,
       ART_RefsMap, ART_RefMap2, ART_Sprites, ART_Spritetiles, ART_CrowdFrameList, ART_FaceOffSprites,
       ART_ZamFrameList, ART_BigFontMap, ART_SmallFontMap, ART_EnergyBarMap, ART_EASNmap, ART_Arrowsmap,
       ART_RonBarrMap, ART_ScoresMap, ART_EASportsMap, ART_RevRinkTilelist, ART_RevRinkTiles, ART_ReplayMap,
       ART_HorRinkMap, ART_HorRinkTiles, ART_PrintFont2Map, ART_SetupMenuMap, ART_SetupFramerMap,
       ART_LogoBoxMap, ART_PicturePalette, ART_NoPicSkater1, ART_NoPicSkater2, ART_NoPicGoalie1,
       ART_NoPicGoalie2, ART_PlayerCardsScreenPic, ART_CornerLogoMap, ART_ArenaGfxBank, ART_PlayoffSprite,
       ART_HiScoreImg, ART_HotIconMap, ART_ColdIconMap, ART_TeamBitmaps, ART_ASSETS };

struct league {
    uint32_t table[LG_TABLES];  /* each table's ROM address */
    uint16_t teams;             /* NumTeams */
    uint16_t playoff_teams;     /* NumPlayoffTeams: those that play for the cup (the first ones) */
    uint16_t playoff_rows;      /* PlayoffRows: playoffseats' rows of 16 teams (a power of two) */
    uint8_t art[LEAGUE_MAX];    /* the cartridge's team whose art each team shows (its logo, its banners unless it has its own) */
    /* the team name banners (native/banner.c). block: the team's row pair in Teamblocksmap's map,
     * its art team's, or its own past the cartridge's 28 (block_tiles: its 22 tiles, plain, row by
     * row). banner_tiles: in a league with banners of its own, each team's TeamBitmaps banner (22
     * tiles), drawn into the first 22 chars of its set (BannerSlotMap) */
    uint8_t block[LEAGUE_MAX];
    uint32_t block_tiles[LEAGUE_MAX], banner_tiles[LEAGUE_MAX];
    uint32_t asset[ART_ASSETS]; /* each shared asset's ROM address */
    /* HiScoreScreen's two pictures' places (column, row): the logo, then HiScoreImg (the
     * cartridge's 14, 3 and 16, 16; the hacks draw a whole screen of their own at 0, 0) */
    uint8_t hiscore_at[2][2];
};
extern struct league league;
extern const struct league league_cartridge;
#define LG(name) (league.table[LG_##name])
/* LG(OptionTeamNames): a built league's, the String the setup screen's Team lines show for each
 * team (its city; its whole name when another team has its city, as the cartridge's two New
 * York teams); 0, the cartridge's: its own code, which knows them as teams 13 and 14 */
#define ART(name) (league.asset[ART_##name])

void league_retail(void);       /* the cartridge's */
static inline uint16_t league_art(uint16_t team) { return team < LEAGUE_MAX ? league.art[team] : team; }
static inline uint16_t league_block(uint16_t team) { return team < LEAGUE_MAX ? league.block[team] : team; }
/* save RAM for the teams past the cartridge's 28 (at most 4), in space its layout leaves free
 * below LineDataBase: their crowd records (16 bytes each), then their player records (26 roster
 * slots of 4 bytes; PlayerRecordOffsets, in words of 4 bytes) */
#define LEAGUE_SRAM_CROWD 0xE20u
#define LEAGUE_SRAM_PLAYERS (LEAGUE_SRAM_CROWD + 4 * 16)
/* a team's crowd record (16 bytes) in save RAM: the cartridge's 28 at CrowdRecBase, more at
 * LEAGUE_SRAM_CROWD */
uint32_t league_crowd_record(uint16_t team);
