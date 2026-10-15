/* Oracle shim: linked into the Genesis Plus GX libretro core (built with HOOK_CPU) to give the
 * Python harness breakpoints, per-PC coverage and function capture records.
 *
 * A capture record is taken when the 68k reaches a breakpointed PC: registers and the whole
 * 64 KB work RAM at entry, then again when the matching rts lands on the return address with
 * the stack pointer at or above its entry value plus 4 (routines may pop stacked arguments).
 * Inside a captured call every entry into a known routine (the "fn" set, normally every
 * routine of the game) is logged too, with its registers at entry and return and the RAM
 * bytes it changed, and so is every read of a hardware port. Captured calls nest: a nested
 * call is logged into every frame that was open when it was entered, so each record is
 * complete on its own. The C port's tests replay those logs in place of callees that are not
 * ported yet. RAM is stored in 68k byte order (GPGX keeps it word-swapped on little-endian
 * hosts).
 *
 * Record layout (all little-endian u32 unless noted):
 *   magic 'HLCC', total_size, pc, frame_no, flags (bit0 irq, bit1 abnormal exit), n_calls,
 *   regs_in[18], regs_out[18], ram_in[65536], ram_out[65536],
 *   then n_calls blocks, each written when the call returns (so in close order; seq gives the
 *   entry order): pc, depth (bit31 = left by unwinding), seq, regs_in[18], regs_out[18],
 *   stack[16 bytes], n_diff, n_diff x (u16 off, u8 old, u8 new).
 *   A hardware read is a block with pc = address | 0x80000000, a hardware write one with
 *   pc = address | 0x40000000; regs_in[0] = value, regs_in[1] = width, no diff. Hardware is the
 *   VDP / PSG ($C00000-$C0001F), the I/O ports ($A10000-$A1001F), the Z80 bus and reset
 *   ($A11100, $A11200) and Z80 RAM ($A00000-$A0FFFF).
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "shared.h"
#include "debug/cpuhook.h"

#define ROM_SIZE 0x100000u
#define RAM_SIZE 0x10000u
#define NREGS 18                 /* d0-d7 a0-a7 pc sr */
#define MAXDEPTH 4               /* simultaneously open capture frames */
#define MAXOPEN 32               /* simultaneously open nested calls */
#define BPTAB 1024
#define CAP_MAGIC 0x43434C48u    /* 'HLCC' */
#define REC_HDR_WORDS 6
#define IO_READ_FLAG 0x80000000u
#define IO_WRITE_FLAG 0x40000000u
#define ABNORMAL_EXIT 0x80000000u

typedef struct {
  uint32_t pc, sp_after, ret, flags;
  uint32_t regs[NREGS];
  uint8_t ram[RAM_SIZE];
  uint8_t *log; size_t log_len, log_cap;
  uint32_t n_calls;              /* entry sequence counter */
  uint32_t n_closed;             /* blocks written */
  int n_open_at_entry;           /* nested calls already open when this frame opened */
} Frame;

typedef struct {
  uint32_t pc, sp_after, ret, depth;   /* depth: position in the open stack + 1 */
  uint32_t regs[NREGS];
  uint32_t seq[MAXDEPTH];              /* entry order within each frame it belongs to */
  uint8_t stk[16];                     /* bytes at sp..sp+15 at entry: return address, then stacked arguments */
  int frame;                           /* innermost frame when opened: it belongs to frames 0..frame */
  uint8_t ram[RAM_SIZE];               /* RAM at entry, for the diff */
} Open;

static uint8_t bp_map[ROM_SIZE / 8];     /* capture breakpoints */
static uint8_t fn_map[ROM_SIZE / 8];     /* routine entries logged as nested calls */
static struct { uint32_t pc, hits, captured; } bptab[BPTAB];
static uint32_t *coverage;
static int coverage_on;
static uint32_t frame_no;
static FILE *cap;
static uint32_t cap_max_per_bp = 32, cap_stride = 1, cap_records;
static int last_was_rts;
static uint32_t rts_seen;
uint32_t oracle_rts_seen(void) { return rts_seen; }
uint32_t oracle_rom_word(uint32_t a) { return ((uint32_t)READ_BYTE(cart.rom, a) << 8) | READ_BYTE(cart.rom, a + 1); }   /* the previous instruction was an rts */
static int cap_spread;   /* capture call numbers 1,2,3,4,6,8,12,16,... so frequent routines are sampled across the session */
static int geometric(uint32_t n) { while (n > 3 && (n & 1) == 0) n >>= 1; return n <= 3; }
static uint32_t irq_pcs[8];
static int n_irq_pcs;
static Frame frames[MAXDEPTH];
static int depth;
static Open open_calls[MAXOPEN];
static int n_open;
static int installed;
static uint8_t diffbuf[RAM_SIZE * 4];

static inline int map_test(const uint8_t *m, uint32_t pc) { return pc < ROM_SIZE && ((m[pc >> 3] >> (pc & 7)) & 1); }
static inline void map_set(uint8_t *m, uint32_t pc, int on) {
  if (pc >= ROM_SIZE) return;
  if (on) m[pc >> 3] |= (uint8_t)(1u << (pc & 7)); else m[pc >> 3] &= (uint8_t)~(1u << (pc & 7));
}

static void ram_copy_be(uint8_t *dst) { for (uint32_t i = 0; i < RAM_SIZE; i++) dst[i] = READ_BYTE(work_ram, i); }

static uint32_t ram_read32(uint32_t addr) {
  uint32_t o = addr & (RAM_SIZE - 1);
  return ((uint32_t)READ_BYTE(work_ram, o) << 24) | ((uint32_t)READ_BYTE(work_ram, o + 1) << 16) |
         ((uint32_t)READ_BYTE(work_ram, o + 2) << 8) | READ_BYTE(work_ram, o + 3);
}

static void regs_get(uint32_t *r) {
  for (int i = 0; i < 8; i++) r[i] = m68k_get_reg(M68K_REG_D0 + i);
  for (int i = 0; i < 8; i++) r[8 + i] = m68k_get_reg(M68K_REG_A0 + i);
  r[16] = m68k_get_reg(M68K_REG_PC);
  r[17] = m68k_get_reg(M68K_REG_SR);
}

static int bp_slot(uint32_t pc) {
  uint32_t h = (pc * 2654435761u >> 22) & (BPTAB - 1);
  for (int i = 0; i < BPTAB; i++) {
    uint32_t k = (h + i) & (BPTAB - 1);
    if (bptab[k].pc == pc || bptab[k].pc == 0) { bptab[k].pc = pc; return (int)k; }
  }
  return -1;
}

static void log_append(Frame *f, const void *p, size_t n) {
  if (f->log_len + n > f->log_cap) {
    size_t nc = f->log_cap ? f->log_cap * 2 : 65536;
    while (nc < f->log_len + n) nc *= 2;
    f->log = realloc(f->log, nc); f->log_cap = nc;
  }
  memcpy(f->log + f->log_len, p, n); f->log_len += n;
}

/* write one call block into frame k */
static void write_block(int k, uint32_t pc, uint32_t depth_word, uint32_t seq, const uint32_t *rin, const uint32_t *rout,
                        const uint8_t *stk, uint32_t n_diff, const uint8_t *diff) {
  Frame *f = &frames[k];
  uint32_t hdr[3] = { pc, depth_word, seq };
  log_append(f, hdr, sizeof hdr);
  log_append(f, rin, NREGS * 4);
  log_append(f, rout, NREGS * 4);
  log_append(f, stk, 16);
  log_append(f, &n_diff, 4);
  if (n_diff) log_append(f, diff, n_diff * 4);
  f->n_closed++;
}

/* ---- hardware reads and writes ------------------------------------------------------------ */
static int is_hw(uint32_t a) {
  return (a >= 0xC00000 && a < 0xC00020) || (a >= 0xA10000 && a < 0xA10020) || (a & ~0x1u) == 0xA11100 ||
         (a & ~0x1u) == 0xA11200 || (a >= 0xA00000 && a < 0xA10000);
}
static void log_io(uint32_t flag, int width, uint32_t address, uint32_t value) {
  uint32_t regs[NREGS] = { value, (uint32_t)width };
  uint32_t zero[NREGS] = {0};
  uint8_t stk[16] = {0};
  for (int k = 0; k < depth; k++)
    write_block(k, address | flag, (uint32_t)(n_open + 1 - frames[k].n_open_at_entry), frames[k].n_calls++, regs, zero, stk, 0, NULL);
}

/* ---- nested calls ----------------------------------------------------------------------- */
static void open_call(uint32_t pc) {
  if (n_open >= MAXOPEN) return;
  Open *o = &open_calls[n_open];
  o->pc = pc; o->frame = depth - 1; o->depth = (uint32_t)n_open + 1;
  regs_get(o->regs);
  uint32_t sp = o->regs[15];
  o->ret = ram_read32(sp); o->sp_after = sp + 4;
  for (int i = 0; i < 16; i++) o->stk[i] = READ_BYTE(work_ram, (sp + i) & (RAM_SIZE - 1));
  ram_copy_be(o->ram);
  for (int k = 0; k <= o->frame; k++) o->seq[k] = frames[k].n_calls++;
  n_open++;
}

static void close_call(Open *o, int abnormal) {
  uint32_t regs_out[NREGS];
  regs_get(regs_out);
  uint32_t n_diff = 0;
  for (uint32_t i = 0; i < RAM_SIZE; i++) {
    uint8_t now = READ_BYTE(work_ram, i);
    if (now != o->ram[i]) {
      uint8_t *e = diffbuf + 4 * n_diff++;
      e[0] = (uint8_t)i; e[1] = (uint8_t)(i >> 8); e[2] = o->ram[i]; e[3] = now;
    }
  }
  for (int k = 0; k <= o->frame && k < depth; k++) {
    uint32_t d = o->depth - (uint32_t)frames[k].n_open_at_entry;
    write_block(k, o->pc, d | (abnormal ? ABNORMAL_EXIT : 0), o->seq[k], o->regs, regs_out, o->stk, n_diff, diffbuf);
  }
}

/* close every open call (innermost first) whose return matches the current pc / sp */
static void close_matching(uint32_t pc, uint32_t sp) {
  while (n_open && open_calls[n_open - 1].ret == pc && sp >= open_calls[n_open - 1].sp_after) {
    close_call(&open_calls[n_open - 1], 0);
    n_open--;
  }
}

/* ---- capture frames ----------------------------------------------------------------------- */
static void finish_frame(void) {
  Frame *f = &frames[depth - 1];
  /* nested calls opened inside this frame that are still open (abnormal exits) are dropped */
  while (n_open && open_calls[n_open - 1].frame >= depth - 1) n_open--;
  depth--;
  if (cap) {
    uint32_t regs_out[NREGS];
    regs_get(regs_out);
    uint32_t total = REC_HDR_WORDS * 4 + 2 * NREGS * 4 + 2 * RAM_SIZE + (uint32_t)f->log_len;
    uint32_t hdr[REC_HDR_WORDS] = { CAP_MAGIC, total, f->pc, frame_no, f->flags, f->n_closed };
    fwrite(hdr, 4, REC_HDR_WORDS, cap);
    fwrite(f->regs, 4, NREGS, cap);
    fwrite(regs_out, 4, NREGS, cap);
    fwrite(f->ram, 1, RAM_SIZE, cap);
    uint8_t *ram_out = malloc(RAM_SIZE);
    ram_copy_be(ram_out);
    fwrite(ram_out, 1, RAM_SIZE, cap);
    free(ram_out);
    if (f->log_len) fwrite(f->log, 1, f->log_len, cap);
    cap_records++;
  }
  f->log_len = 0; f->n_calls = 0; f->n_closed = 0;
}

static void enter_frame(uint32_t pc) {
  int k = bp_slot(pc);
  if (k < 0) return;
  bptab[k].hits++;
  if (!cap || depth >= MAXDEPTH || bptab[k].captured >= cap_max_per_bp) return;
  if (cap_stride > 1 && (bptab[k].hits % cap_stride) != 0) return;
  if (cap_spread && !geometric(bptab[k].hits)) return;
  Frame *f = &frames[depth++];
  f->pc = pc; f->flags = 0; f->log_len = 0; f->n_calls = 0; f->n_closed = 0; f->n_open_at_entry = n_open;
  regs_get(f->regs);
  uint32_t sp = f->regs[15];
  f->ret = ram_read32(sp);
  f->sp_after = sp + 4;
  ram_copy_be(f->ram);
  bptab[k].captured++;
}

static void hook(hook_type_t type, int width, unsigned int address, unsigned int value) {
  if (type == HOOK_M68K_R || type == HOOK_M68K_W) {
    uint32_t a = address & 0xFFFFFF;
    if (depth && is_hw(a)) log_io(type == HOOK_M68K_R ? IO_READ_FLAG : IO_WRITE_FLAG, width, a, value);
    return;
  }
  if (type != HOOK_M68K_E) return;
  uint32_t pc = address;
  if (coverage_on && pc < ROM_SIZE) coverage[pc]++;
  /* An rts that lands with sp at or past a frame's return slot returned from it, whatever the
   * address: printz and friends rewrite their return address to skip inline data, and CompShoot
   * pops its caller's return to end the caller too. */
  int after_rts = last_was_rts;
  last_was_rts = pc < ROM_SIZE && READ_BYTE(cart.rom, pc) == 0x4E && (READ_BYTE(cart.rom, pc + 1) == 0x75 || READ_BYTE(cart.rom, pc + 1) == 0x73);   /* rts or rte; READ_WORD does not unswap the ROM */
  if (after_rts) rts_seen++;
  if (depth) {
    uint32_t sp = m68k_get_reg(M68K_REG_A7);
    if (after_rts) {
      while (n_open && sp >= open_calls[n_open - 1].sp_after) { close_call(&open_calls[n_open - 1], 0); n_open--; }
      while (depth && sp >= frames[depth - 1].sp_after) finish_frame();
      if (!depth) return;
    }
    if (n_open) close_matching(pc, sp);
    /* a call that unwound the stack past its own return slot without returning to it */
    while (n_open && sp > open_calls[n_open - 1].sp_after) { close_call(&open_calls[n_open - 1], 1); n_open--; }
    /* frames that tail-jumped into each other share a return: close every one that matches */
    int closed = 0;
    while (depth && pc == frames[depth - 1].ret && sp >= frames[depth - 1].sp_after) { finish_frame(); closed = 1; }
    while (depth && sp > frames[depth - 1].sp_after) { frames[depth - 1].flags |= 2; finish_frame(); closed = 1; }
    if (depth && !closed) {
      for (int i = 0; i < n_irq_pcs; i++) if (pc == irq_pcs[i]) { for (int d = 0; d < depth; d++) frames[d].flags |= 1; break; }
      if (map_test(fn_map, pc)) open_call(pc);
    }
  }
  if (map_test(bp_map, pc)) enter_frame(pc);
}

/* ---- exported API ---------------------------------------------------------------------- */
int oracle_install(void) { if (!installed) { set_cpu_hook(hook); installed = 1; } return 1; }
void oracle_bp_set(uint32_t pc, int on) { map_set(bp_map, pc, on); }
void oracle_bp_clear_all(void) { memset(bp_map, 0, sizeof bp_map); memset(bptab, 0, sizeof bptab); }
void oracle_fn_set(uint32_t pc, int on) { map_set(fn_map, pc, on); }
void oracle_fn_set_many(const uint32_t *pcs, int n) { for (int i = 0; i < n; i++) map_set(fn_map, pcs[i], 1); }
void oracle_fn_clear_all(void) { memset(fn_map, 0, sizeof fn_map); }
uint32_t oracle_bp_hits(uint32_t pc) { int k = bp_slot(pc); return k < 0 ? 0 : bptab[k].hits; }
uint32_t oracle_bp_captured(uint32_t pc) { int k = bp_slot(pc); return k < 0 ? 0 : bptab[k].captured; }
void oracle_set_frame(uint32_t n) { frame_no = n; }
void oracle_set_irq_pcs(const uint32_t *pcs, int n) { n_irq_pcs = n > 8 ? 8 : n; memcpy(irq_pcs, pcs, n_irq_pcs * 4); }
int oracle_capture_open(const char *path, uint32_t max_per_bp, uint32_t stride) {
  if (cap) fclose(cap);
  cap = fopen(path, "wb");
  cap_max_per_bp = max_per_bp; cap_stride = stride ? stride : 1; cap_records = 0; depth = 0; n_open = 0;
  for (int i = 0; i < BPTAB; i++) bptab[i].captured = 0;
  return cap != NULL;
}
uint32_t oracle_capture_close(void) { if (cap) { fclose(cap); cap = NULL; } depth = 0; n_open = 0; return cap_records; }
void oracle_capture_spread(int on) { cap_spread = on; }
uint32_t oracle_capture_records(void) { return cap_records; }
void oracle_coverage_enable(int on) { if (on && !coverage) coverage = calloc(ROM_SIZE, sizeof *coverage); coverage_on = on && coverage != NULL; }
void oracle_coverage_get(uint32_t *dst) { if (coverage) memcpy(dst, coverage, ROM_SIZE * sizeof *coverage); }
void oracle_coverage_reset(void) { if (coverage) memset(coverage, 0, ROM_SIZE * sizeof *coverage); }
void oracle_ram_get(uint8_t *dst) { ram_copy_be(dst); }
void oracle_regs_get(uint32_t *dst) { regs_get(dst); }
