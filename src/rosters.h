/* The roster set: the teams a game is played with. The cartridge's own (26 clubs and the two All-Star
 * teams) unless the native side builds another (native/rosters.c). The game reads its teams from
 * native tables (roster_team, below: read off the cartridge for its own roster set, made from a roster set's
 * folder for another) and its art by number: the cartridge's addresses, or a roster set's own art
 * past the cartridge's 1 MB (rom.h's rom_art), in the cartridge's formats; each team's logo and
 * centre ice by the number in its table, the art every team shares by rosters.asset (ART()), the
 * banners by RS(Teamblocksmap) and RS(BannerSlotMap). With the cartridge's roster set the tables are
 * its own numbers and its art its own addresses, so it plays exactly as the original. A roster set is
 * part of a game's session, like its rules. */
#pragma once
#include <stdint.h>

enum { RS_TeamList, RS_TeamPalettes, RS_TeamLogoBitmaps, RS_TeamGfxList, RS_TeamSongs, RS_TeamRatings,
       RS_TeamLogoPalettes, RS_FeaturedPictures, RS_playoffseats, RS_PlayerRecordOffsets, RS_Teamblocksmap,
       RS_BannerSlotMap, RS_OptionTeamNames, RS_TABLES };
#define ROSTERS_MAX 32                                   /* (a team is a byte in the playoff tree and 5 bits elsewhere) */
/* the game's art every team shares (titles, setup screens, the rink, the sprites, the fonts,
 * the stock photos ...), read through asset[] (ART()): the cartridge's own, or a roster set's (art/game) */
enum { ART_GameSetUpMap, ART_GameSetUpMap2, ART_TitleScreenImg, ART_NHLShieldImg, ART_PAlogoImg,
       ART_TitleImg, ART_ScoutMap, ART_FramerMap, ART_FaceOffMap, ART_Rinktilelist, ART_Rinktiles,
       ART_RefsMap, ART_RefMap2, ART_Sprites, ART_Spritetiles, ART_CrowdFrameList, ART_FaceOffSprites,
       ART_ZamFrameList, ART_BigFontMap, ART_SmallFontMap, ART_EnergyBarMap, ART_EASNmap, ART_Arrowsmap,
       ART_RonBarrMap, ART_ScoresMap, ART_EASportsMap, ART_RevRinkTilelist, ART_RevRinkTiles, ART_ReplayMap,
       ART_HorRinkMap, ART_HorRinkTiles, ART_PrintFont2Map, ART_SetupMenuMap, ART_SetupFramerMap,
       ART_LogoBoxMap, ART_PicturePalette, ART_NoPicSkater1, ART_NoPicSkater2, ART_NoPicGoalie1,
       ART_NoPicGoalie2, ART_PlayerCardsScreenPic, ART_CornerLogoMap, ART_ArenaGfxBank, ART_PlayoffSprite,
       ART_HiScoreImg, ART_HotIconMap, ART_ColdIconMap, ART_TeamBitmaps, ART_ASSETS };

struct rosters {
    uint32_t table[RS_TABLES];  /* each table's ROM address */
    uint16_t teams;             /* NumTeams */
    uint16_t playoff_teams;     /* NumPlayoffTeams: those that play for the cup (the first ones) */
    uint16_t playoff_rows;      /* PlayoffRows: playoffseats' rows of 16 teams (a power of two) */
    uint8_t art[ROSTERS_MAX];    /* the cartridge's team whose art each team shows (its logo, its banners unless it has its own) */
    /* the team name banners (native/banner.c). block: the team's row pair in Teamblocksmap's map,
     * its art team's, or its own past the cartridge's 28 (block_tiles: its 22 tiles, plain, row by
     * row). banner_tiles: in a roster set with banners of its own, each team's TeamBitmaps banner (22
     * tiles), drawn into the first 22 chars of its set (BannerSlotMap) */
    uint8_t block[ROSTERS_MAX];
    uint32_t block_tiles[ROSTERS_MAX], banner_tiles[ROSTERS_MAX];
    uint32_t asset[ART_ASSETS]; /* each shared asset's ROM address */
    /* HiScoreScreen's two pictures' places (column, row): the logo, then HiScoreImg (the
     * cartridge's 14, 3 and 16, 16; the hacks draw a whole screen of their own at 0, 0) */
    uint8_t hiscore_at[2][2];
};
extern struct rosters rosters;
extern const struct rosters rosters_cartridge;
#define RS(name) (rosters.table[RS_##name])
/* RS(OptionTeamNames): a built roster set's, the String the setup screen's Team lines show for each
 * team (its city; its whole name when another team has its city, as the cartridge's two New
 * York teams); 0, the cartridge's: its own code, which knows them as teams 13 and 14 */
#define ART(name) (rosters.asset[ART_##name])

void rosters_retail(void);       /* the cartridge's */
static inline uint16_t rosters_art(uint16_t team) { return team < ROSTERS_MAX ? rosters.art[team] : team; }
static inline uint16_t rosters_block(uint16_t team) { return team < ROSTERS_MAX ? rosters.block[team] : team; }
/* ---- the teams, natively ---------------------------------------------------------------------
 * What the game reads of each team, as plain tables (the playbook's NATIVE-STATE.md, step 4):
 * filled when a roster set is chosen, from the cartridge's own tables or a roster set's, and read by
 * name. A String the printers read stays a String (struct RosterText: as the cartridge keeps one).
 * at: where the cartridge keeps the thing, in its own roster set (the 68000's registers held those
 * addresses: the capture test compares them), else 0. */
#define RS_TEXT 30                                      /* a String's room: its length word, the longest name (26, an arena), a 0 */
#define RS_ROSTER 26                                    /* players a team (the game's room, and its save RAM's) */
#define RS_PICTURES 32
struct RosterText { uint32_t at; uint8_t s[RS_TEXT]; };     /* s: a length word (itself counted, made even), the bytes, a 0 to make it even */
/* a player's 14 ratings (0-15), in the order the cartridge keeps them, two to a byte after his
 * number: the roster screen's names for a skater, and what each is to a goalie */
enum { LR_WEIGHT, LR_AGILITY, LR_SPEED, LR_OFF_AWARENESS, LR_DEF_AWARENESS, LR_SHOT_POWER /* goalie: puck control */,
       LR_CHECKING /* unused */, LR_HANDED_FIGHTING /* goalie: glove hand */, LR_STICK_HANDLING /* unused */,
       LR_SHOT_ACCURACY /* unused */, LR_ENDURANCE /* goalie: stick right */, LR_SHOOT_OR_PASS /* stick left */,
       LR_PASS_ACCURACY /* glove right */, LR_AGGRESSIVENESS /* glove left */, LR_RATINGS };
struct RosterPlayer {
    struct RosterText name;
    uint8_t number;                                     /* BCD, as the game prints it */
    uint8_t rating[LR_RATINGS];
};
struct RosterTeam {
    uint32_t data;                                      /* what the team struct keeps (tmdata): in the cartridge's roster set, its address of the team's data */
    struct RosterText city, abbr, nickname, arena;
    uint16_t colours[2][16];                            /* its home and away palettes (setplayercolors) */
    uint16_t matchup[2][16];                            /* TeamPalettes: the setup screens' colours */
    uint8_t offense, defense;                           /* 0-7: its other playoff games, simulated (getscore) */
    uint8_t pp, pk, home, away;                         /* 0-15: its players' bonuses on the power play, killing a penalty, at home, away */
    uint8_t forwards, defensemen, goalies;              /* (the roster: its goalies, then forwards, then defensemen) */
    uint16_t starts;                                    /* .sodds: a nibble a goalie, how often he starts */
    uint8_t lines[8][8];                                /* the line sets (SC1 SC2 CHK PP1 PP2 PK1 PK2 L8): 7 roster slots + 1 each, then a 0 */
    uint8_t players;
    struct RosterPlayer player[RS_ROSTER];
    uint8_t rating;                                     /* TeamRatings */
    uint8_t songs[6];                                   /* the organ's (TeamSongs) */
    uint16_t records;                                   /* its players' records in save RAM (PlayerRecordOffsets: 4 bytes a word) */
    uint8_t pictures;                                   /* FeaturedPictures: a picture (its address) and the roster slot it is */
    struct { uint32_t pic; uint16_t slot; } picture[RS_PICTURES];
    uint32_t pictures_at;                               /* (the cartridge's address of the list) */
    struct RosterText option_name;                          /* the setup screen's Team line (s[0] == 0: the cartridge's code, by number) */
    /* its art, by number (the cartridge's address of it, or a roster set's own past the cartridge):
     * its logo (TeamLogoBitmaps), its centre ice (TeamGfxList), and the logo's colours */
    uint32_t logo, ice;
    uint16_t logo_colours[16];
};
extern struct RosterTeam roster_team[ROSTERS_MAX];
/* the data a roster set's own team keeps in its team struct: a number for it (the cartridge's teams
 * keep the cartridge's address of their data; a roster set's are in the tables only) */
#define RS_DATA(t) (0xF00000u | (uint32_t)(t))
extern uint8_t roster_seats[32][16];                        /* playoffseats: rows of 16 teams, the draws a playoff starts from */
void rosters_read(void);                                 /* the tables from the roster set's ROM image (rosters.table) */
__attribute__((noreturn, cold)) void rosters_bad_team(uint16_t team);
/* team t of the roster set (one past ROSTERS_MAX: a bug in the port) */
static inline struct RosterTeam *rosters_team(uint16_t t) { if (t >= ROSTERS_MAX) rosters_bad_team(t); return &roster_team[t]; }
/* byte k of a player's 8 the cartridge keeps after his name (0: his number; 1-7: his ratings, two
 * to a byte, the first high) */
static inline uint8_t roster_attr(const struct RosterPlayer *p, int k) { return k == 0 ? p->number : (uint8_t)(p->rating[2 * k - 2] << 4 | p->rating[2 * k - 1]); }

/* save RAM for the teams past the cartridge's 28 (at most 4), in space its layout leaves free
 * below LineDataBase: their crowd records (16 bytes each), then their player records (26 roster
 * slots of 4 bytes; PlayerRecordOffsets, in words of 4 bytes) */
#define ROSTERS_SRAM_CROWD 0xE20u
#define ROSTERS_SRAM_PLAYERS (ROSTERS_SRAM_CROWD + 4 * 16)
/* a team's crowd record (16 bytes) in save RAM: the cartridge's 28 at CrowdRecBase, more at
 * ROSTERS_SRAM_CROWD */
uint32_t rosters_crowd_record(uint16_t team);
