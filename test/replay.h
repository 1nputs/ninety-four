/* Replay of recorded callee behaviour for function-level tests (see tools/hybrid/oracle_shim.c
 * for the capture format). A stub for an unported routine takes the next recorded call of that
 * routine, applies the RAM bytes it changed, and returns its register results. */
#pragma once
#include <stdint.h>
#include <stddef.h>

#define NREGS 18
enum { R_D0, R_D1, R_D2, R_D3, R_D4, R_D5, R_D6, R_D7, R_A0, R_A1, R_A2, R_A3, R_A4, R_A5, R_A6, R_A7, R_PC, R_SR };

typedef struct {
    uint32_t pc, depth, seq;
    uint32_t regs_in[NREGS], regs_out[NREGS];
    const uint8_t *stack;         /* 16 bytes at sp on entry: return address, then stacked arguments */
    uint32_t n_diff;
    const uint8_t *diff;          /* n_diff x {u16 off le, u8 old, u8 new} */
    int consumed;
} Call;

typedef struct {
    uint32_t pc, frame, flags, n_calls;
    uint32_t regs_in[NREGS], regs_out[NREGS];
    const uint8_t *ram_in, *ram_out;
    Call *calls;                  /* sorted by seq */
} Record;

/* capture file */
int cap_load(const char *path, Record **recs, size_t *n);

/* replay state for the record under test */
void replay_begin(Record *r);
const Call *replay_next(uint32_t pc);         /* next unconsumed call of routine pc; NULL = missing */
void replay_apply(const Call *c);             /* write the call's RAM changes into g */
int replay_unconsumed(const Record *r, int (*is_ported)(uint32_t));  /* count of leftover stub calls */
#define IO_READ_FLAG 0x80000000u
uint32_t replay_io_read(uint32_t addr, int width);   /* next recorded hardware read of addr */
const uint8_t *cur_ram_in(void);
const char *sym_name(uint32_t addr);
int sym_trivial(uint32_t addr);               /* a one-instruction routine (shared rts / jump) */
const char *ram_name(uint32_t off, uint32_t *base);

/* stubs report mismatched arguments here */
void replay_fail(const char *fmt, ...);
extern int replay_failures;
