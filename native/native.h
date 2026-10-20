/* The native game: NHL '94 on pulseman (ext/pulseman), with the Z80's sound relay (src/z80.c)
 * feeding its FM chip. */
#pragma once
#include <stddef.h>
#include <stdint.h>
#include "mdrt/mdrt.h"
#include "mdrt/platform_sdl2.h"
#include "mdrt/net_udp.h"
#include "league.h"

extern mdrt *native_m;                  /* the console */

/* the game's session: from the snapshot into the game loop, until it stops */
void native_game(mdrt *m, void *user);
/* stop the session (the window closed, the run's frames done, lockstep finished): back out of the game */
void native_quit(void);
void native_power_on(void);
void native_power_on_again(void);                      /* (restart_at it) the game from its power on, on the console mdrt_power_on made new */
void native_script_reset(void);                        /* at every wait: a script's RESET, alone (main.c) */
/* after each frame: the frame count, for shots and the stop (main.c); wait: at one of the game's
 * vblank waits (else a vblank lockstep placed in the middle of its work) */
void native_frame_done(long frame, int wait);
/* a vblank now, in the middle of the game's work (hal_native.c) */
void native_vblank_now(void);
int native_in_vblank(void);              /* running the vblank's code */
void native_set_accesses(uint32_t n);    /* lockstep: the count of the game's port accesses (the oracle's) */
void native_set_sr(uint16_t v);          /* the 68k's status register, set without the unmask hook (a snapshot's) */
/* the HAL from master clock `start` of the relay's time (hal_native.c) */
void native_hal_start(mdrt *m, uint64_t start);

/* the game loop from setvideo's wait at a frame's end (main.c) */
void native_resume_game_loop(void);

/* states (state.c): pulseman's console and the game's parts (work RAM, the Z80 relay, the C
 * port's registers kept across routines, the HAL's clock), taken at the game loop's wait */
void native_states_init(mdrt *m, const char *path, int rewind);   /* path: the F9 / F10 file */
int native_load_state_file(const char *path);       /* 0: loaded (then run68k(native_resume_game_loop)) */
void native_save_state_at(long frame, const char *path);   /* headless: at the first game loop wait from frame on */
void native_states_wait(void);                       /* at each of the game's vblank waits, before the handshake */
int native_at_game_loop_wait(void);                  /* (lockstep.c: the routines the game is in) */
void native_track_routines(void);
void native_tracking_reset(void);
long native_frames(void);                            /* the game's vblank waits so far (hal_native.c) */
long native_handshakes(void);                        /* the handshakes this process made */
size_t native_hal_save(uint8_t *out);                /* the HAL's part of a state */
int native_hal_load(const uint8_t *in, size_t size);
long native_golden_lines(void);                      /* the golden run's lines so far (main.c) */

/* netplay (net.c): two players over UDP, the host pad 1 and the guest pad 2, from power on; a
 * rollback goes on from the game loop's wait. The window's sessions are made by its Online page
 * (or from the command line), headless runs' by --host / --join / --peer (mdrt_net_parse) */
#define NATIVE_ROM_CRC32 0x9438F5DDu                      /* "NHL 94 (UE) [!].gen" (SHA-1 7b1489ab...) */
typedef uint16_t (*native_input_fn)(int pad, int k);     /* the k-th pad this player plays (pad p + 1): its buttons now */
const char *native_net_identity(const char *setup);     /* setup: the command line's options that change the game */
int native_net_open(const mdrt_net_options *o, const char *setup, const void *settings, size_t settings_size, native_input_fn input,
                    char *err, size_t errsize);
int native_net_connect(void);                           /* headless: connected (0: refused, or gone; why on stderr) */
int native_net_four_way(void);                          /* the session plays pads 3 or 4 (EA's 4 Way Play in the ports) */
void native_net_start(mdrt *m);                         /* at power on, the states' parts added */
void native_net_stop_at(long frame);                    /* headless: the session ends at --frames */
void native_net_wait(void);                             /* at each of the game's waits, before the handshake */
int native_net_on(void);                                /* in a session (headless: from before power on) */
/* the session's settings (the host's, sent to the guest before power on): the next game's rules,
 * its league as a package, and its save RAM (sram: the file, NULL or missing: blank). Made by the
 * player who may host (the window's Start, or the command line), applied by both at power on */
const void *native_net_settings(const char *sram, size_t *size);
void native_net_power_on(mdrt *m);                      /* in a session: its settings the next game's (before the rules and league power on) */
/* the session's rules (RULE_COUNT of them) into v, whether its league is one sent and whether this
 * player hosts it: 1, or 0 when the game has no session's settings (yet) */
int native_net_session(uint8_t *v, int *sent, int *host);
mdrt_net *native_net(void);                             /* the command line's session, and its link */
mdrt_udp *native_net_udp(void);
void native_net_close(void);

/* game rules (rules.c; src/rules.h): the menu's catalogue, the next game's rules (the platform's
 * options_use, or the command line's --rule NAME=VALUE), set at power on, and the state part */
extern const mdrt_sdl_option rule_options[];
void rules_next(const int *values, void *user);
void rules_next_get(uint8_t *v);                         /* the next game's rules' values (RULE_COUNT of them) */
int rules_next_set(const uint8_t *v, int count);         /* count of them (the rest Classic): 0, or -1 when one is out of range */
/* the menu's page: a preset, then the rules (its options_use and options_changed) */
const mdrt_sdl_option *rules_page(int *count);
/* the menu's pages of them: League (who plays, and what the league chosen holds), Game rules */
const mdrt_sdl_page *rules_pages(int *count);
void rules_page_text(int page, const int *values, char *text, size_t size, void *user);
int rules_page_pick(int which, const char *path, char *why, size_t why_size, void *user);
void rules_page_detail(int which, int value, char *text, size_t size, void *user);
void rules_page_use(const int *values, void *user);
int rules_page_changed(int which, int *values, void *user);
int rules_page_session(int *values, void *user);        /* the session's choices, for the pages (options_session) */
const char *rules_arg(const char *arg);   /* NULL, or what is wrong */
int rules_classic(void);                 /* the next game's are all the original's */
void rules_power_on(void);
size_t rules_state_save(void *user, uint8_t *out);
int rules_state_load(void *user, const uint8_t *in, size_t size);

/* leagues (league.c; src/league.h): a folder of text files (teams.csv, players.csv ...), made the
 * game's tables at power on (its art past the cartridge); the cartridge's own when none */
const char *league_error(void);         /* why the last call failed */
void league_next(const char *dir);      /* the next game's (NULL or "": the cartridge's) */
/* the next game's league as a package's bytes (*out malloc'd: the caller's; a folder's files
 * packed, a package's as they are): their size, 0 (*out NULL) for the cartridge's, -1 */
long league_next_package(uint8_t **out);
int league_next_bytes(const uint8_t *b, size_t n);   /* the next game's built from a package's bytes (0: the cartridge's): 0, -1 */
/* the league a host sent (kept for the run), one of the League page's: its index (listed when it
 * is not, read again for another), -1 when none was sent (or it is being read: ask again) */
int league_session_entry(void);
int league_next_is_cartridge(void);
void league_power_on(void);             /* (a league that will not build: the cartridge's, and why on stderr) */
uint64_t league_hash(void);             /* this game's league: its checksum (0 the cartridge's) */
const char *league_dir(void);           /* its folder ("" the cartridge's) */
const char *league_name(void);
int league_use(const char *dir, uint64_t hash);   /* a state's league (dir's, when its checksum is hash): 0, -1 */
int league_check(const char *dir, int describe);   /* it builds (described on stdout): 0, -1 */
int league_export(const char *dir);     /* the cartridge's league as a folder: 0, -1 */
/* dir's league as a package (zip_path: a zip of its files, to send or keep): 0, -1 */
int league_pack(const char *dir, const char *zip_path);
/* where a ROM keeps its league (romsrc.c): the cartridge, or a hack on the community's 32-team
 * template, whose tables its code points at where the cartridge's points at its own */
struct romsrc {
    const uint8_t *b;
    uint32_t size;
    int hack;                           /* 0: the cartridge */
    uint32_t table[LG_TABLES];          /* (league.h's tables; BannerSlotMap none) */
    int teams, playoff_teams, rows;
    uint32_t bitmaps, rink, rink_tiles, photos;   /* TeamBitmaps, Rinktilelist, Rinktiles, PicturePalette */
    uint32_t asset[ART_ASSETS];         /* the shared art's addresses (league.h's ART_) */
    uint8_t hiscore_at[2][2];           /* HiScoreScreen's pictures' places (league.h) */
};
void romsrc_cartridge(struct romsrc *s, const uint8_t *cart);
void romsrc_game(struct romsrc *s);     /* what the game reads by number now: the cartridge, the league in play's art */
/* rom (size bytes), a hack read against the cartridge cart: 0, else -1 and why */
int romsrc_hack(struct romsrc *s, const uint8_t *rom, uint32_t size, const uint8_t *cart, char *why, size_t why_size);
int romsrc_block_ok(const struct romsrc *s, int t);    /* team t's block reads as the game reads it */
uint8_t rs8(const struct romsrc *s, uint32_t a);        /* (0 past its end) */
uint16_t rs16(const struct romsrc *s, uint32_t a);
uint32_t rs32(const struct romsrc *s, uint32_t a);
/* s's league written as a folder named name (league_export: the cartridge's): 0, -1 */
int league_extract(const struct romsrc *s, const char *dir, const char *name);

/* team art as pictures (art.c): logos, banners, centre ice and photos out of a ROM as palette
 * indexes, PNGs of them, and the game's formats again from PNGs */
#include "mdrt/png.h"
#define ART_LOGO 48                     /* a logo's and a photo's side */
#define ART_LOGO_BYTES (8 + 2 + 36 * 32 + 4 + 72)
#define ART_PHOTO_BYTES (8 + 2 + 36 * 24)
#define ART_ICE_BYTES (8 + 2 + 24 * 32)
#define ART_BANNER_W 88
#define ART_BANNER_H 32
#define ART_BANNER_SETUP 64             /* (a banners picture's colours: the top's 0-63, the setup screen's from 64) */
#define ART_BANNER_COLOURS 80
const char *art_error(void);
uint32_t art_rgb(uint16_t word);
uint16_t art_word(uint32_t rgb);
int art_tiles(const struct romsrc *s, uint32_t a, uint8_t *out, int max);
int art_logo(const struct romsrc *s, int t, uint8_t px[ART_LOGO * ART_LOGO], uint16_t pal[16]);
int art_logo_at(const struct romsrc *s, uint32_t bmp, uint8_t px[ART_LOGO * ART_LOGO]);   /* (the logo bitmap at bmp) */
size_t art_logo_make(const uint8_t px[ART_LOGO * ART_LOGO], uint8_t *out);
void art_photo_palette(const struct romsrc *s, uint16_t pal[16]);
int art_photo(const struct romsrc *s, uint32_t pic, uint8_t px[ART_LOGO * ART_LOGO]);
int art_photo_make(const uint8_t px[ART_LOGO * ART_LOGO], uint8_t out[ART_PHOTO_BYTES]);
int art_ice(const struct romsrc *s, int t, uint8_t *px, int *w, int *h, uint16_t pal[16]);
int art_ice_make(const struct romsrc *s, const uint8_t *px, int w, int h, uint8_t out[ART_ICE_BYTES]);
int art_banners(const struct romsrc *s, int t, uint8_t px[ART_BANNER_H * ART_BANNER_W], uint16_t pal[ART_BANNER_COLOURS]);
int art_save(const char *path, const uint8_t *px, int w, int h, const uint16_t *pal, int n, int transparent0);
int art_load(const char *path, mdrt_png *p, int w, int h);
int art_load_bytes(const char *name, const uint8_t *b, size_t n, mdrt_png *p, int w, int h);
int art_indexes(const char *path, const mdrt_png *p, int x0, int y0, int w, int h, const uint16_t *pal, int n, int base, uint8_t *out);
int art_own_palette(const char *path, const mdrt_png *p, uint16_t pal[16], uint8_t *out);
/* the art every team shares (art.c: league.h's ART_ assets): their names, sizes in a ROM, views
 * (a picture, sprites' tiles as a sheet, a photo) and the assets again from changed views */
struct art_view { int w, h, n, own; uint8_t *px; uint16_t pal[64]; };   /* (own: a picture with colours of its own) */
int art_shared(void);                                   /* how many */
const char *art_shared_name(int i);
int art_shared_asset(int i);                            /* its ART_ */
int art_shared_tiles(int i, int *at);                   /* the ART_ of its tiles' address (at past its own), or -1 */
uint32_t art_shared_size(const struct romsrc *s, int i, uint32_t a);
int art_shared_view(const struct romsrc *s, int i, uint32_t a, const uint16_t screen[64], const uint16_t team[16], struct art_view *v);
int art_shared_read(const char *path, const mdrt_png *p, int i, const struct art_view *base, struct art_view *v);
int art_view_same(const struct art_view *a, const struct art_view *b);
void art_view_free(struct art_view *v);
size_t art_shared_make(const struct romsrc *s, int i, uint32_t a, const struct art_view *v, uint8_t **out);
size_t art_shared_copy(const struct romsrc *s, int i, uint32_t a, uint8_t **out);
/* the art map: every piece of art the game can read by number in the league in play (the
 * cartridge's and the league's own), what it is and how long: for a renderer that draws other
 * art in a piece's place (texture packs), and for --art-check (every byte the VDP takes by DMA
 * from the game's data in a piece) */
enum { PIECE_SHARED, PIECE_LOGO, PIECE_ICE, PIECE_PHOTO, PIECE_BANNERS, PIECE_BANNER_SLOTS, PIECE_TEAM_BANNERS,
       PIECE_ARENA, PIECES };                           /* (a team's own block tiles: in its league's Teamblocksmap) */
/* which: the ART_ of a shared one, a team's team, an arena animation's number (ArenaAnims), else 0 */
struct art_piece { uint32_t at, size; uint8_t kind, which; };
void art_map_make(void);                                  /* (league.c: when a league is put in play) */
const struct art_piece *art_map(int *n);                  /* sorted by at, none overlapping another */
const struct art_piece *art_piece_at(uint32_t number);    /* the piece number is in, or NULL */
const char *art_piece_name(const struct art_piece *p, char *out, size_t size);
int art_map_overlaps(void);                               /* pieces inside another, left out (a piece whose
                                                             length, reckoned, ran into the next ends where it begins) */
void art_map_report(void);                                /* (those, on stdout) */
/* the leagues/ folder's leagues for the menu (the cartridge's first): the count */
int league_list(const char *const **names, const char *const **labels);
/* what the League page says of league i of the list (0 the cartridge's): lines, a tab before each
 * one's right */
void league_about(int i, char *text, size_t size);
int league_unread(void);                /* the leagues in leagues/ the list left out (why: on stderr) */
/* list entry i's folder or package (NULL: the cartridge's) into out; its files as the League page
 * says them ("leagues/x.zip, 1652 KB"); a league the player found at path made an entry of the
 * list: its index (an entry already, by any name: that one's), or -1 and why */
const char *league_list_dir(int i, char *out, size_t size);
void league_files(int i, char *text, size_t size);
int league_add(const char *path, char *why, size_t why_size);
/* a player's overall rating as his card shows it, from his 14 ratings and number (ratings.c);
 * *sum (NULL: not wanted) the sum it comes from, past 99 */
int rating_overall(const uint8_t *cart, const uint8_t nib[14], int number, int goalie, int *sum);
/* a rating as the roster screen shows it: the column's mask's bits (PAttribColumns' high word) */
int rating_column(const uint8_t *cart, const uint8_t nib[14], int number, unsigned bits);
const char *league_sram(const char *base);   /* the next game's save RAM file (base: the cartridge's) */

/* team name banners (banner.c): the cartridge's two sets (Teamblocksmap, TeamBitmaps), 88 x 16
 * pixels a team, and new ones drawn in their letters for a league's teams */
enum { BANNER_BLOCKS, BANNER_BITMAPS, BANNER_SETS };
int banner_prepare(const uint8_t *cart);                 /* its sets and letters: 0, -1 */
int banner_check(const char *text, char *why, size_t n); /* a name a banner can show: 0, else -1 and why */
/* team art's banner in a set (text: with that name in place of its own) */
void banner_pixels(int set, int art, const char *text, uint8_t px[16][88]);
void banner_tiles(const uint8_t px[16][88], uint8_t out[22 * 32]);   /* its 22 tiles, row by row */
const uint8_t *banner_set(int set, int *count, uint32_t *map);       /* a set's tiles (unpacked) and map */
int banner_selfcheck(int *letters, int *same);           /* -> the banners checked (league_test) */

/* lockstep against the oracle (lockstep.c) */
int lockstep_load(const char *path, mdrt *m);
void lockstep_check(long frame);
void lockstep_report(void);              /* (at the session's end) */
int lockstep_vblank_again(void);         /* the oracle's next vblank came at the rte of this one's code: run it now */
int lockstep_result(void);               /* 0 all passed, 1 a check differed, 2 stopped before the recording's end */
void lockstep_access(uint32_t n);        /* the game's port access n (hal_native.c: not the vblank code's) */
void lockstep_before_access(void);       /* before the game's next one */
void lockstep_unmask(uint32_t n);        /* the game unmasked the interrupts after port access n */
