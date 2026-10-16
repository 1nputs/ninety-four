/* The native game: NHL '94 on pulseman (ext/pulseman), with the Z80's sound relay (src/z80.c)
 * feeding its FM chip. */
#pragma once
#include <stdint.h>
#include "mdrt/mdrt.h"

extern mdrt *native_m;                  /* the console */

/* the game's session: from the snapshot into the game loop, until it stops */
void native_game(mdrt *m, void *user);
/* stop the session (the window closed, the run's frames done, lockstep finished): back out of the game */
void native_quit(void);
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
void native_states_init(mdrt *m, const char *path, int rewind);   /* path: the F5 / F9 file */
int native_load_state_file(const char *path);       /* 0: loaded (then run68k(native_resume_game_loop)) */
void native_save_state_at(long frame, const char *path);   /* headless: at the first game loop wait from frame on */
void native_states_wait(void);                       /* at each of the game's vblank waits, before the handshake */
int native_at_game_loop_wait(void);                  /* (lockstep.c: the routines the game is in) */
void native_track_routines(void);
long native_frames(void);                            /* the game's vblank waits so far (hal_native.c) */
long native_handshakes(void);                        /* the handshakes this process made */
size_t native_hal_save(uint8_t *out);                /* the HAL's part of a state */
int native_hal_load(const uint8_t *in, size_t size);
long native_golden_lines(void);                      /* the golden run's lines so far (main.c) */

/* lockstep against the oracle (lockstep.c) */
int lockstep_load(const char *path, mdrt *m);
void lockstep_check(long frame);
void lockstep_report(void);              /* (at the session's end) */
int lockstep_vblank_again(void);         /* the oracle's next vblank came at the rte of this one's code: run it now */
int lockstep_result(void);               /* 0 all passed, 1 a check differed, 2 stopped before the recording's end */
void lockstep_access(uint32_t n);        /* the game's port access n (hal_native.c: not the vblank code's) */
void lockstep_before_access(void);       /* before the game's next one */
void lockstep_unmask(uint32_t n);        /* the game unmasked the interrupts after port access n */
