/* The native runtime: the window, the frame loop and the pads. */
#pragma once
#include <stdint.h>

enum { NATIVE_UP = 1, NATIVE_DOWN = 2, NATIVE_LEFT = 4, NATIVE_RIGHT = 8,
       NATIVE_A = 0x10, NATIVE_B = 0x20, NATIVE_C = 0x40, NATIVE_START = 0x80 };
extern uint16_t native_pads[2];
extern uint8_t zram[0x2000];

/* the end of a frame: show it, read the pads, keep 60 frames a second, then the vblank interrupt */
void native_frame(void);
void native_sram_load(const char *path);
void native_sram_save(const char *path);
