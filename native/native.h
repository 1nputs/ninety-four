/* The native game: NHL '94 on pulseman (ext/pulseman), with the Z80's sound relay (src/z80.c)
 * feeding its FM chip. */
#pragma once
#include <stddef.h>
#include <stdint.h>
#include "mdrt/mdrt.h"
#include "mdrt/platform_sdl2.h"
#include "league.h"

extern mdrt *native_m;                  /* the console */

/* the game's session: from the snapshot into the game loop, until it stops */
void native_game(mdrt *m, void *user);
/* stop the session (the window closed, the run's frames done, lockstep finished): back out of the game */
void native_quit(void);
void native_power_on(void);
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

/* game rules (rules.c; src/rules.h): the menu's catalogue, the next game's rules (the platform's
 * options_use, or the command line's --rule NAME=VALUE), set at power on, and the state part */
extern const mdrt_sdl_option rule_options[];
void rules_next(const int *values, void *user);
/* the menu's page: a preset, then the rules (its options_use and options_changed) */
const mdrt_sdl_option *rules_page(int *count);
void rules_page_use(const int *values, void *user);
int rules_page_changed(int which, int *values, void *user);
int rules_arg(const char *arg);
int rules_classic(void);                 /* the next game's are all the original's */
void rules_power_on(void);
size_t rules_state_save(void *user, uint8_t *out);
int rules_state_load(void *user, const uint8_t *in, size_t size);

/* leagues (league.c; src/league.h): a folder of text files (teams.csv, players.csv ...), built
 * past the cartridge at power on; the cartridge's own when none */
const char *league_error(void);         /* why the last call failed */
void league_next(const char *dir);      /* the next game's (NULL or "": the cartridge's) */
int league_next_is_cartridge(void);
void league_power_on(void);             /* (a league that will not build: the cartridge's, and why on stderr) */
uint64_t league_hash(void);             /* this game's league: its checksum (0 the cartridge's) */
const char *league_dir(void);           /* its folder ("" the cartridge's) */
const char *league_name(void);
int league_use(const char *dir, uint64_t hash);   /* a state's league (dir's, when its checksum is hash): 0, -1 */
int league_check(const char *dir, int describe);   /* it builds (described on stdout): 0, -1 */
int league_export(const char *dir);     /* the cartridge's league as a folder: 0, -1 */
/* where a ROM keeps its league (romsrc.c): the cartridge, or a hack on the community's 32-team
 * template, whose tables its code points at where the cartridge's points at its own */
struct romsrc {
    const uint8_t *b;
    uint32_t size;
    int hack;                           /* 0: the cartridge */
    uint32_t table[LG_TABLES];          /* (league.h's tables; BannerSlotMap none) */
    int teams, playoff_teams, rows;
};
void romsrc_cartridge(struct romsrc *s, const uint8_t *cart);
/* rom (size bytes), a hack read against the cartridge cart: 0, else -1 and why */
int romsrc_hack(struct romsrc *s, const uint8_t *rom, uint32_t size, const uint8_t *cart, char *why, size_t why_size);
int romsrc_block_ok(const struct romsrc *s, int t);    /* team t's block reads as the game reads it */
uint8_t rs8(const struct romsrc *s, uint32_t a);        /* (0 past its end) */
uint16_t rs16(const struct romsrc *s, uint32_t a);
uint32_t rs32(const struct romsrc *s, uint32_t a);
/* s's league written as a folder named name (league_export: the cartridge's): 0, -1 */
int league_extract(const struct romsrc *s, const char *dir, const char *name);
/* the leagues/ folder's leagues for the menu (the cartridge's first): the count */
int league_list(const char *const **names, const char *const **labels);
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
