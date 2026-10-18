/* The league (league.h): the cartridge's until another is built. */
#include "league.h"
#include "symbols_gen.h"

#define RETAIL { { SYM_TeamList, SYM_TeamPalettes, SYM_TeamLogoBitmaps, SYM_TeamGfxList, SYM_TeamSongs, SYM_TeamRatings, \
                   SYM_TeamLogoPalettes, SYM_FeaturedPictures, SYM_playoffseats, SYM_PlayerRecordOffsets, SYM_Teamblocksmap, 0 }, \
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
                   SYM_TeamBitmaps } }

struct league league = RETAIL;                           /* (the cartridge's from the start) */
const struct league league_cartridge = RETAIL;

void league_retail(void) { league = league_cartridge; }

uint32_t league_crowd_record(uint16_t team)
{
    if (team < SYM_NumTeams) return SYM_CrowdRecBase + (uint32_t)team * SYM_CrowdRecBytes;
    return LEAGUE_SRAM_CROWD + (uint32_t)(team - SYM_NumTeams) * SYM_CrowdRecBytes;
}
