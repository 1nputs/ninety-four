/* The league: the teams a game is played with. The cartridge's own (26 clubs and the two All-Star
 * teams) unless the native side builds another (native/league.c) past the cartridge's 1 MB, in the
 * ROM's own formats: a team list of team blocks, and the tables by team. The game reads every
 * table by team, and the counts, through here; with the cartridge's league they are its own
 * addresses and numbers, so it plays exactly as the original. A league is part of a game's
 * session, like its rules. */
#pragma once
#include <stdint.h>

enum { LG_TeamList, LG_TeamPalettes, LG_TeamLogoBitmaps, LG_TeamGfxList, LG_TeamSongs, LG_TeamRatings,
       LG_TeamLogoPalettes, LG_FeaturedPictures, LG_playoffseats, LG_PlayerRecordOffsets, LG_TABLES };
#define LEAGUE_MAX 32                                   /* (a team is a byte in the playoff tree and 5 bits elsewhere) */

struct league {
    uint32_t table[LG_TABLES];  /* each table's ROM address */
    uint16_t teams;             /* NumTeams */
    uint16_t playoff_teams;     /* NumPlayoffTeams: those that play for the cup (the first ones) */
    uint16_t playoff_rows;      /* PlayoffRows: playoffseats' rows of 16 teams (a power of two) */
    uint8_t art[LEAGUE_MAX];    /* the cartridge's team whose team block and bitmap each team shows */
};
extern struct league league;
#define LG(name) (league.table[LG_##name])

void league_retail(void);       /* the cartridge's */
static inline uint16_t league_art(uint16_t team) { return team < LEAGUE_MAX ? league.art[team] : team; }
/* save RAM for the teams past the cartridge's 28 (at most 4), in space its layout leaves free
 * below LineDataBase: their crowd records (16 bytes each), then their player records (26 roster
 * slots of 4 bytes; PlayerRecordOffsets, in words of 4 bytes) */
#define LEAGUE_SRAM_CROWD 0xE20u
#define LEAGUE_SRAM_PLAYERS (LEAGUE_SRAM_CROWD + 4 * 16)
/* a team's crowd record (16 bytes) in save RAM: the cartridge's 28 at CrowdRecBase, more at
 * LEAGUE_SRAM_CROWD */
uint32_t league_crowd_record(uint16_t team);
