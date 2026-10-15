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
 *   ($A11100, $A11200), Z80 RAM ($A00000-$A0FFFF) and save RAM ($200000-$20FFFF).
 *   An interrupt taken inside a captured call (not the bare-rte handler) is a block too, with
 *   pc = handler | 0x20000000, closed at its rte: its stack bytes hold the SR and the PC it
 *   interrupted, its diff everything the handler changed; the handler's own calls and port
 *   accesses nest under it. Frames with an interrupt get flags bit 0, and bit 2 when the
 *   interrupts were logged this way (so a test can replay them at the C code's vblank waits).
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
#define IRQ_FLAG 0x20000000u
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

/* A capture over a long stretch (a highlight game inside the intermission) logs every call of
 * thousands of frames; past this many bytes its log is dropped and the record is not written. */
static size_t log_limit = 128u << 20;
static uint32_t cap_dropped;

static void log_append(Frame *f, const void *p, size_t n) {
  if (f->flags & 8) return;
  if (f->log_len + n > log_limit) {
    f->flags |= 8;
    free(f->log); f->log = NULL; f->log_cap = 0; f->log_len = 0;
    return;
  }
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
         (a & ~0x1u) == 0xA11200 || (a >= 0xA00000 && a < 0xA10000) || (a >= 0x200000 && a < 0x210000);
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

/* an interrupt: its frame on the stack is SR (2 bytes) + PC (4); it ends at the rte */
static void open_irq(uint32_t pc) {
  if (n_open >= MAXOPEN) return;
  open_call(pc);
  Open *o = &open_calls[n_open - 1];
  uint32_t sp = o->regs[15];
  o->pc = pc | IRQ_FLAG;
  o->ret = ram_read32(sp + 2);
  o->sp_after = sp + 6;
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
  if (cap && (f->flags & 8)) cap_dropped++;
  if (cap && !(f->flags & 8)) {
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
  if (f->log_cap > (16u << 20)) { free(f->log); f->log = NULL; f->log_cap = 0; }   /* give a big log back */
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

static void vdptrace_access(int write, int width, uint32_t a, uint32_t value);
static void vdptrace_tick(void);
static FILE *vt;
static FILE *pt;
static void padtrace_access(int write, int width, uint32_t a, uint32_t value);
static FILE *st;
static void sndtrace_access(uint32_t a, uint32_t value);

static void hook(hook_type_t type, int width, unsigned int address, unsigned int value) {
  if (st && type == HOOK_M68K_W) {
    uint32_t a = address & 0xFFFFFF;
    if ((a >= 0xC00010 && a < 0xC00018) || (a >= 0xA00000 && a < 0xA02000) || (a & ~1u) == 0xA11100 || (a & ~1u) == 0xA11200)
      sndtrace_access(a, value);
  }
  if (pt && (type == HOOK_M68K_R || type == HOOK_M68K_W)) {
    uint32_t a = address & 0xFFFFFF;
    if (a >= 0xA10000 && a < 0xA10020) padtrace_access(type == HOOK_M68K_W, width, a, value);
  }
  if (vt) {
    if (type == HOOK_M68K_R || type == HOOK_M68K_W) {
      uint32_t a = address & 0xFFFFFF;
      if (a >= 0xC00000 && a < 0xC0000C) vdptrace_access(type == HOOK_M68K_W, width, a, value);
    } else if (type == HOOK_M68K_E) {
      vdptrace_tick();
    }
  }
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
      int irq = 0;
      for (int i = 0; i < n_irq_pcs; i++)
        if (pc == irq_pcs[i] && !(READ_BYTE(cart.rom, pc) == 0x4E && READ_BYTE(cart.rom, pc + 1) == 0x73)) {
          for (int d = 0; d < depth; d++) frames[d].flags |= 5;
          open_irq(pc);
          irq = 1;
          break;
        }
      if (!irq && map_test(fn_map, pc)) open_call(pc);
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
void oracle_capture_log_limit(uint32_t mb) { log_limit = (size_t)mb << 20; }
uint32_t oracle_capture_dropped(void) { return cap_dropped; }
uint32_t oracle_capture_close(void) { if (cap) { fclose(cap); cap = NULL; } depth = 0; n_open = 0; return cap_records; }
void oracle_capture_spread(int on) { cap_spread = on; }
uint32_t oracle_capture_records(void) { return cap_records; }
void oracle_coverage_enable(int on) { if (on && !coverage) coverage = calloc(ROM_SIZE, sizeof *coverage); coverage_on = on && coverage != NULL; }
void oracle_coverage_get(uint32_t *dst) { if (coverage) memcpy(dst, coverage, ROM_SIZE * sizeof *coverage); }
void oracle_coverage_reset(void) { if (coverage) memset(coverage, 0, ROM_SIZE * sizeof *coverage); }
void oracle_ram_get(uint8_t *dst) { ram_copy_be(dst); }
void oracle_regs_get(uint32_t *dst) { regs_get(dst); }

/* ---- snapshot for the native runtime's bring-up ------------------------------------------ */
/* VRAM in VDP byte order, CRAM / VSRAM as big-endian bus words (0BBB0GGG0RRR0, 11-bit), the 32
 * VDP registers, Z80 RAM */
void oracle_vdp_get(uint8_t *out_vram, uint8_t *out_cram, uint8_t *out_vsram, uint8_t *out_reg, uint8_t *out_zram) {
  for (uint32_t i = 0; i < 0x10000; i++) out_vram[i] = READ_BYTE(vram, i);
  for (int i = 0; i < 64; i++) {
    uint16_t v = *(uint16_t *)&cram[i * 2];
    uint16_t bus = (uint16_t)(((v & 0x1C0) << 3) | ((v & 0x038) << 2) | ((v & 0x007) << 1));
    out_cram[2 * i] = (uint8_t)(bus >> 8); out_cram[2 * i + 1] = (uint8_t)bus;
  }
  for (int i = 0; i < 40; i++) {
    uint16_t v = *(uint16_t *)&vsram[i * 2] & 0x7FF;
    out_vsram[2 * i] = (uint8_t)(v >> 8); out_vsram[2 * i + 1] = (uint8_t)v;
  }
  memcpy(out_reg, reg, 32);
  memcpy(out_zram, zram, 0x2000);
}

/* ---- VDP test vectors for md-runtime (.vdptrace) ------------------------------------------
 * Every VDP port access of the 68k in order, the 68k bytes each DMA from the 68k bus will read,
 * and at the start of each vblank (v_counter reaching the active height) a FRAME marker with the
 * VDP state and the frame just drawn in 9-bit colours. Format (integers little-endian unless
 * noted):
 *   "MDVT", version u32 = 1, region u8 (0 NTSC), 3 reserved bytes, state
 *   state:  regs[24], VRAM[65536] (VDP order), CRAM[64] u16 BE, VSRAM[40] u16 BE
 *           (CRAM as bus words 0000BBB0GGG0RRR0, VSRAM 11 bits)
 *   events: tag u8, then
 *     1 CTRL16 u16 | 2 CTRL32 u32 | 3 DATA16 u16 | 4 DATA32 u32 | 5 READ16 u16 (data port)
 *     6 STATUS u16 | 7 HV8 addr u8, value u8 | 8 HV16 u16
 *     9 DMASRC n u32, n bytes (the 68k memory the DMA started by the next CTRL reads, in order)
 *    10 FRAME state, width u16, height u16, width x height u16 9-bit colours (bus format)
 *    11 END
 */
static uint8_t vt_pending;
static uint16_t vt_first;
static uint16_t vt_last_vc;
static int vt_top;          /* a frame was begun under tracing: the next FRAME is whole */

static void vt_u8(uint8_t v) { fputc(v, vt); }
static void vt_u16(uint16_t v) { fputc(v & 0xFF, vt); fputc(v >> 8, vt); }
static void vt_u32(uint32_t v) { vt_u16((uint16_t)v); vt_u16((uint16_t)(v >> 16)); }

static void vt_state(void) {
  static uint8_t v[0x10000], c[128], s[80], r[32], z[0x2000];
  oracle_vdp_get(v, c, s, r, z);
  fwrite(r, 1, 24, vt);
  fwrite(v, 1, sizeof v, vt);
  fwrite(c, 1, sizeof c, vt);
  fwrite(s, 1, sizeof s, vt);
}

static uint8_t bus_byte(uint32_t a) {
  cpu_memory_map *m = &m68k.memory_map[(a >> 16) & 0xFF];
  if (m->read8) return (uint8_t)m->read8(a);
  return READ_BYTE(m->base, a & 0xFFFF);
}

/* the second command word of a DMA from the 68k bus: log what it will read */
static void vt_command(uint16_t w) {
  if (!vt_pending) {
    if ((w & 0xC000) != 0x8000) { vt_pending = reg[1] & 4; vt_first = w; }
    return;
  }
  vt_pending = 0;
  uint8_t code = (uint8_t)(((vt_first >> 14) & 3) | ((w >> 2) & 0x3C));
  if (!(code & 0x20) || !(reg[1] & 0x10) || (reg[23] >> 6) >= 2) return;
  uint32_t n = (uint32_t)(reg[20] << 8 | reg[19]);
  if (!n) n = 0x10000;
  uint32_t hi = (uint32_t)(reg[23] & 0x7F) << 17, src = (uint32_t)(reg[22] << 8 | reg[21]) << 1;
  vt_u8(9); vt_u32(n * 2);
  for (uint32_t i = 0; i < n; i++) {
    vt_u8(bus_byte(hi | src)); vt_u8(bus_byte(hi | (src + 1)));
    src = (src + 2) & 0x1FFFF;
  }
}

static void vdptrace_access(int write, int width, uint32_t a, uint32_t value) {
  uint32_t port = a & 0x1E;
  if (write) {
    if (port == 0 || port == 2) {                        /* data */
      vt_pending = 0;
      if (width == 4) { vt_u8(4); vt_u32(value); }
      else { uint16_t v = width == 1 ? (uint16_t)((value & 0xFF) << 8 | (value & 0xFF)) : (uint16_t)value; vt_u8(3); vt_u16(v); }
    } else if (port == 4 || port == 6) {                 /* control */
      if (width == 4) {
        vt_command((uint16_t)(value >> 16)); vt_command((uint16_t)value);
        vt_u8(2); vt_u32(value);
      } else {
        uint16_t v = width == 1 ? (uint16_t)((value & 0xFF) << 8 | (value & 0xFF)) : (uint16_t)value;
        vt_command(v);
        vt_u8(1); vt_u16(v);
      }
    }
    return;
  }
  if (port == 0 || port == 2) {
    vt_pending = 0;
    if (width == 4) { vt_u8(5); vt_u16((uint16_t)(value >> 16)); vt_u8(5); vt_u16((uint16_t)value); }
    else { vt_u8(5); vt_u16((uint16_t)value); }
  } else if (port == 4 || port == 6) {
    vt_pending = 0;
    vt_u8(6); vt_u16((uint16_t)value);
  } else if (width == 1) {
    vt_u8(7); vt_u8((uint8_t)a); vt_u8((uint8_t)value);
  } else {
    vt_u8(8); vt_u16((uint16_t)value);
  }
}

static void vdptrace_tick(void) {
  uint16_t vc = v_counter;
  uint16_t h = (uint16_t)bitmap.viewport.h;
  if (vc < vt_last_vc) vt_top = 1;                    /* the counter wrapped: a new frame */
  if (vt_top && vt_last_vc < h && vc >= h && vc < lines_per_frame) {
    uint16_t w = (uint16_t)bitmap.viewport.w;
    vt_u8(10);
    vt_state();
    vt_u16(w); vt_u16(h);
    for (int y = 0; y < h; y++) {
      const uint16_t *row = (const uint16_t *)(bitmap.data + (y + bitmap.viewport.y) * bitmap.pitch) + bitmap.viewport.x;
      for (int x = 0; x < w; x++) {
        uint16_t px = row[x];                            /* RGB565 from 4-bit levels, normal colours even */
        uint16_t r = (px >> 13) & 7, g = (px >> 8) & 7, b = (px >> 2) & 7;
        vt_u16((uint16_t)(b << 9 | g << 5 | r << 1));
      }
    }
  }
  vt_last_vc = vc;
}

int oracle_install(void);
int oracle_vdptrace_open(const char *path) {
  oracle_install();
  if (vt) fclose(vt);
  vt = fopen(path, "wb");
  if (!vt) return 0;
  fwrite("MDVT", 1, 4, vt); vt_u32(1); vt_u8(0); vt_u8(0); vt_u8(0); vt_u8(0);
  vt_state();
  vt_pending = 0; vt_last_vc = v_counter; vt_top = 0;
  return 1;
}

void oracle_vdptrace_close(void) {
  if (!vt) return;
  vt_u8(11);
  fclose(vt);
  vt = NULL;
}

/* ---- pad port test vectors for md-runtime (.padtrace) --------------------------------------
 * "MDPT", version u32 = 1, the 16 I/O registers at the start (io_reg: version, data 1-3, control
 * 1-3, serial ...); then records: tag u8
 *   1 FRAME  buttons u16 port 1, buttons u16 port 2 (bits: 0 up, 1 down, 2 left, 3 right, 4 B,
 *            5 C, 6 A, 7 start) held from here on
 *   2 WRITE  address u32, width u8, value u32 (a write to $A10000-$A1001F)
 *   3 READ   address u32, width u8, value u32 (the value the oracle returned)
 *   4 END
 */
static void pt_u8(uint8_t v) { fputc(v, pt); }
static void pt_u32(uint32_t v) { for (int i = 0; i < 4; i++) fputc((v >> (8 * i)) & 0xFF, pt); }

static void padtrace_access(int write, int width, uint32_t a, uint32_t value) {
  pt_u8(write ? 2 : 3); pt_u32(a); pt_u8((uint8_t)width); pt_u32(value);
}

int oracle_padtrace_open(const char *path) {
  oracle_install();
  if (pt) fclose(pt);
  pt = fopen(path, "wb");
  if (!pt) return 0;
  fwrite("MDPT", 1, 4, pt); pt_u32(1);
  fwrite(io_reg, 1, 16, pt);
  return 1;
}
void oracle_padtrace_frame(uint32_t b1, uint32_t b2) {
  if (!pt) return;
  pt_u8(1); fputc(b1 & 0xFF, pt); fputc((b1 >> 8) & 0xFF, pt); fputc(b2 & 0xFF, pt); fputc((b2 >> 8) & 0xFF, pt);
}
void oracle_padtrace_close(void) { if (pt) { pt_u8(4); fclose(pt); pt = NULL; } }

/* ---- sound test vectors for md-runtime and the Z80 relay (.sndtrace) -----------------------
 * "MDST", version u32 = 3, the oracle's audio rate u32 (Hz), its FM and PSG preamps u16 each (in
 * percent: GPGX's defaults are 100 and 150; 0 leaves that chip out of the audio), flags u32 (bit 0:
 * recorded from power on, so every write the chips had is in the trace; otherwise they start in a
 * state the trace does not hold; bit 1: the oracle's FM core was Nuked-OPN2, not MAME's YM2612,
 * which also gives the Z80 a different busy flag to poll), Z80 RAM at the start [8192], the Z80 at the start: PC u16, HL u16,
 * C u8, bank u16 (68k address >> 15), state u8 (bit 0 running, bit 1 bus taken by the 68k), time
 * u32 (master clocks from the trace's start to where the Z80 is: the oracle runs it whole
 * instructions at a time, so it can be a little ahead);
 * then events: tag u8, time u32 (master clocks since the trace started, modulo 2^32: 3420 a line,
 * 262 lines a frame, so it wraps after 4793 frames; events come in time order give or take a few
 * hundred clocks, so a reader unwraps it), and
 *   1 YM     address u8 (0 / 1 address / data of part I, 2 / 3 of part II), value u8
 *   2 PSG    value u8                     (68k writes to $C00011)
 *   3 Z80RAM address u16, value u8        (68k byte writes to $A00000-$A01FFF)
 *   4 BUSREQ value u16 | 5 Z80RESET value u16
 *   6 AUDIO  n u32, n stereo frames of s16 (the oracle's output for the frame just run)
 *   7 END
 */
static void (*st_fm_orig)(unsigned int, unsigned int, unsigned int);
static uint32_t st_base;

static void st_u8(uint8_t v) { fputc(v, st); }
static void st_u16(uint16_t v) { fputc(v & 0xFF, st); fputc(v >> 8, st); }
static void st_u32(uint32_t v) { st_u16((uint16_t)v); st_u16((uint16_t)(v >> 16)); }

static void st_fm(unsigned int cycles, unsigned int address, unsigned int data) {
  if (st) { st_u8(1); st_u32(st_base + cycles); st_u8((uint8_t)(address & 3)); st_u8((uint8_t)data); }
  st_fm_orig(cycles, address, data);
}

static void sndtrace_access(uint32_t a, uint32_t value) {
  uint32_t t = st_base + m68k.cycles;
  if (a >= 0xC00010 && a < 0xC00018) { st_u8(2); st_u32(t); st_u8((uint8_t)value); }
  else if (a < 0xA02000) { st_u8(3); st_u32(t); st_u16((uint16_t)(a & 0x1FFF)); st_u8((uint8_t)value); }
  else if ((a & ~1u) == 0xA11100) { st_u8(4); st_u32(t); st_u16((uint16_t)value); }
  else { st_u8(5); st_u32(t); st_u16((uint16_t)value); }
}

int oracle_sndtrace_open(const char *path, uint32_t rate, uint32_t fm_preamp, uint32_t psg_preamp, uint32_t flags) {
  oracle_install();
  if (st) fclose(st);
  st = fopen(path, "wb");
  if (!st) return 0;
  fwrite("MDST", 1, 4, st); st_u32(3); st_u32(rate); st_u16((uint16_t)fm_preamp); st_u16((uint16_t)psg_preamp); st_u32(flags);
  fwrite(zram, 1, 0x2000, st);
  st_u16((uint16_t)Z80.pc.w.l); st_u16((uint16_t)Z80.hl.w.l); st_u8((uint8_t)Z80.bc.b.l);
  st_u16((uint16_t)(zbank >> 15)); st_u8(zstate); st_u32(Z80.cycles);
  st_base = 0;
  st_fm_orig = fm_write;
  fm_write = st_fm;
  return 1;
}
/* after each frame: its audio, and the time base moves on a frame */
void oracle_sndtrace_frame(const int16_t *samples, uint32_t frames) {
  if (!st) return;
  st_u8(6); st_u32(st_base + lines_per_frame * MCYCLES_PER_LINE); st_u32(frames);
  fwrite(samples, 4, frames, st);
  st_base += lines_per_frame * MCYCLES_PER_LINE;
}
void oracle_sndtrace_close(void) {
  if (!st) return;
  st_u8(7);
  fclose(st);
  st = NULL;
  if (st_fm_orig) fm_write = st_fm_orig;
}
