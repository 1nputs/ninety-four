/* The league (league.h): the cartridge's until another is built. */
#include "league.h"
#include "symbols_gen.h"

#define RETAIL { { SYM_TeamList, SYM_TeamPalettes, SYM_TeamLogoBitmaps, SYM_TeamGfxList, SYM_TeamSongs, SYM_TeamRatings, \
                   SYM_TeamLogoPalettes, SYM_FeaturedPictures, SYM_playoffseats, SYM_PlayerRecordOffsets, SYM_Teamblocksmap, 0 }, \
                 SYM_NumTeams, SYM_NumPlayoffTeams, SYM_PlayoffRows, \
                 { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, \
                   28, 29, 30, 31 }, \
                 { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, \
                   28, 29, 30, 31 }, { 0 }, { 0 } }

struct league league = RETAIL;                           /* (the cartridge's from the start) */

void league_retail(void)
{
    static const struct league retail = RETAIL;
    league = retail;
}

uint32_t league_crowd_record(uint16_t team)
{
    if (team < SYM_NumTeams) return SYM_CrowdRecBase + (uint32_t)team * SYM_CrowdRecBytes;
    return LEAGUE_SRAM_CROWD + (uint32_t)(team - SYM_NumTeams) * SYM_CrowdRecBytes;
}
