/* The Z80 sound relay ported to C (sound94.asm:Z80_Program_Code; see z80.c). */
#pragma once
#include <stdint.h>

/* a step of a routine (the init, a command): its writes worked out when it starts */
struct Z80Op { uint8_t kind, part, reg, val; uint16_t cycles; };
#define Z80_OPS 8192
enum { Z80_STARTUP = 1, Z80_RELAY };

struct Z80Relay {
    /* time, in master clocks (15 a Z80 cycle) since the relay started */
    uint64_t now;           /* the client's time: how far z80_relay_run has been told */
    uint64_t zc;            /* the Z80's: the end of the instruction it last ran */
    uint64_t frame;         /* the start of the frame (the YM2612 busy flag's 42-clock grid) */
    uint64_t ym_free;       /* the end of the YM2612's busy flag */
    uint8_t running;        /* out of reset, with the program loaded */
    uint8_t bus_taken;      /* the 68k holds the bus: the Z80 waits */
    uint8_t prog;           /* the program the 68k loaded: Z80_STARTUP or Z80_RELAY (0: none to run) */
    /* the registers the programs use, at the instruction at pc */
    uint16_t pc, hl, iy, sp;
    uint8_t a, b, c, d, e, carry;
    uint16_t bc2, de2, hl2; /* the other set (EXX) */
    uint8_t bank;           /* the bank register: 68k address bits 15-22 of the window */
    uint8_t n;              /* the bank switch's bit count */
    /* a routine running ($0029 init or $00D1 command): its steps, then pc = ret */
    struct Z80Op ops[Z80_OPS];
    uint16_t nops, op, ret;
    uint8_t poll;           /* a BIT 7,(IX) done: 1 busy, 2 free (the JR next) */
};
extern uint8_t z80_ram[0x2000];
extern struct Z80Relay z80;

/* a YM2612 register write (part 0: $4000 / $4001, part 1: $4002 / $4003): the platform's chip;
 * z80.zc is its time */
void z80_ym_write(int part, uint8_t reg, uint8_t val);
void z80_ym_reset(void);                    /* the Z80 reset released: it resets the YM2612 too */

void z80_relay_run(uint32_t mclk);          /* time went by (master clocks; 15 a Z80 cycle) */
void z80_relay_reset(uint16_t v);           /* the 68k wrote IO_Z80RES: $100 runs the program */
void z80_relay_busreq(uint16_t v);          /* the 68k wrote IO_Z80BUS: $100 takes the bus (the Z80 waits), 0 gives it back */
void z80_relay_frame(void);                 /* a frame starts */
/* a Z80 running the program, caught at pc (a snapshot): its HL, C and bank register */
void z80_relay_seed(uint16_t pc, uint16_t hl, uint8_t c, uint8_t bank);
