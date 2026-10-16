/* The native game: NHL '94 on md-runtime (../md-runtime), with the Z80's sound relay (src/z80.c)
 * feeding its FM chip. */
#pragma once
#include <stdint.h>
#include "mdrt/mdrt.h"

extern mdrt *native_m;                  /* the console */

/* the game's session: from the snapshot into the game loop, until it stops */
void native_game(mdrt *m, void *user);
/* stop the session (the window closed, the front end is not ported): back out of the game */
void native_quit(void);
/* after each frame: the frame count, for shots and the stop (main.c); wait: at one of the game's
 * vblank waits (else a vblank lockstep placed in the middle of its work) */
void native_frame_done(long frame, int wait);
/* a vblank now, in the middle of the game's work (hal_native.c) */
void native_vblank_now(void);
int native_in_vblank(void);              /* running the vblank's code */
void native_set_accesses(uint32_t n);    /* lockstep: the count of the game's port accesses (the oracle's) */
/* the HAL from master clock `start` of the relay's time (hal_native.c) */
void native_hal_start(mdrt *m, uint64_t start);

/* the game loop from setvideo's wait at a frame's end (main.c) */
void native_resume_game_loop(void);

/* lockstep against the oracle (lockstep.c) */
int lockstep_load(const char *path, mdrt *m);
void lockstep_check(long frame);
void lockstep_report(void);              /* (at the session's end) */
void lockstep_access(uint32_t n);        /* the game's port access n (hal_native.c: not the vblank code's) */
void lockstep_before_access(void);       /* before the game's next one */
void lockstep_unmask(uint32_t n);        /* the game unmasked the interrupts after port access n */
