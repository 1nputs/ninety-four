/* Lockstep against the oracle (nhl94 --lockstep FILE, tools/oracle/lockstep.py): the game reads the
 * oracle's pads and HV counter values, and at each of its vblank waits its work RAM (below the
 * stack) and VDP state must hash as the oracle's did at the matching vblank interrupt.
 *
 * The native game runs its vblank code only where it waits, the way the 68k does when its frame's
 * work fits in a frame. When it does not (a lag frame: the oracle's interrupt lands in the game's
 * logic), the oracle runs the vblank code an extra time mid-logic and the two part. The
 * recording marks those interrupts and keeps the oracle's whole state at the next one in a wait
 * loop. When that one is the game loop's (setvideo's wait for dfok, called from DoGameFrame from
 * Gameloop: its stack says), the player takes the state (work RAM, the VDP, and `elapsed`, the d7
 * the game loop keeps) and restarts the native game there (restart_at, as the 68k's own restarts
 * do), wherever the native game was: then it checks again. Other resync points in the game are
 * passed over. Outside the game's logic (DoGameFrame not on the native function stack: the front
 * end, whose screens load for frames with the vblank on, and the intermission) the player compares the native state with the
 * oracle's there, but for what the oracle's vblank code changed in the lag frames (the
 * recording's lag mask, version 4), then takes the oracle's state in place and goes on.
 * Better, from version 5 the recording counts the game's port accesses (the C port makes the same
 * accesses in the same order): outside the game's logic the player runs each of the oracle's lag
 * interrupts in the native game's work right after the same access (or, when it came while the
 * interrupts were masked, at the unmask that follows), so the waits after it are checked exactly.
 * A difference while in step stops the run, with work RAM in out/lockstep-native.ram for diffing
 * against the oracle's (lockstep.py --dump). */
#define _GNU_SOURCE                                       /* dladdr */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "game.h"
#include "native.h"
#include "symbols_gen.h"
#include "z80.h"

#define STATE_SIZE (0x10000 + 24 + 0x10000 + 128 + 80 + 64 + 0x2000 + 8)
#define MASK_SIZE (0x2000 + 4 + 0x2000 + 16 + 16)
typedef struct record {
    uint8_t flags;              /* 1: in a wait loop, 2: the whole state follows, 4: at an unmask */
    uint32_t frame, at, writes;
    uint32_t routine;           /* the 68k routine (the last global label) at */
    uint8_t past_dfng;          /* the routine sets dfng (bset #2,(disflags).w) between its entry and at */
    uint8_t at_rts;             /* at an rts (version 6): the return into ret_routine */
    uint32_t ret_routine;       /* the routine of the long above the exception frame */
    uint16_t buttons, nhv;
    const uint8_t *hv, *state, *mask;
    uint64_t ram, vdp;
} record;
static int version;
static record *recs;
static size_t nrecs, next;              /* the oracle's next record */
static size_t span_lo, span_hi;         /* the records whose HV reads this span replays */
static size_t hv_rec, hv_used;
static int hv_over;
static long checked, lagged, resynced, skipped, masked, placed;
static int insync = 1;                  /* checking: the native game and the oracle are in step */
static int restarted;                   /* the wait after a restart is the same moment again */
static int tracking;                    /* following the routines the game is in: lockstep, or states */

static uint64_t fnv64(uint64_t h, const uint8_t *p, size_t n)
{
    for (size_t i = 0; i < n; i++) h = (h ^ p[i]) * 0x100000001B3ull;
    return h;
}
static uint16_t le16(const uint8_t *p) { return (uint16_t)(p[0] | p[1] << 8); }
static uint32_t le32(const uint8_t *p) { return (uint32_t)(p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24); }
static uint64_t le64(const uint8_t *p) { return le32(p) | (uint64_t)le32(p + 4) << 32; }

/* the oracle's HV values, in order, through the span's records: a byte read gets its byte in the
 * half it read */
static uint16_t hv_source(void *user)
{
    (void)user;
    while (hv_rec < span_hi && hv_used >= recs[hv_rec].nhv) { hv_rec++; hv_used = 0; }
    if (hv_rec >= span_hi) { hv_over = 1; return 0; }
    const uint8_t *e = recs[hv_rec].hv + 3 * hv_used++;
    uint16_t v = le16(e + 1);
    return e[0] == 0 ? (uint16_t)(v << 8) : v;
}

/* the 68k routine holding address a: the last global label at or below it */
static uint32_t routine_at(uint32_t a)
{
    uint32_t best = 0;
    for (size_t i = 0; i < sizeof rom_syms / sizeof *rom_syms; i++)
        if (rom_syms[i].addr <= a && rom_syms[i].addr > best) best = rom_syms[i].addr;
    return best;
}

static uint16_t buttons;
static void pad_source(void *user, uint16_t b[2]) { (void)user; b[0] = buttons; b[1] = 0; }

static size_t lag_next;                 /* the span's next lag interrupt to place */

/* the span up to the oracle's next interrupt in a wait loop */
static int logic_lags;                  /* lag frames of this span placed in the game's logic */
static int span_end_logic;              /* the span ends with the oracle in the game's logic (-1: not known yet) */
static void next_span(void)
{
    logic_lags = 0;
    span_end_logic = -1;
    lag_next = span_lo = hv_rec = next;
    hv_used = 0;
    hv_over = 0;
    while (next < nrecs && !(recs[next].flags & 1)) next++;
    span_hi = next < nrecs ? next + 1 : nrecs;
}

int lockstep_load(const char *path, mdrt *m)
{
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "cannot open %s\n", path); return 0; }
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *d = malloc((size_t)len);
    if (!d || fread(d, 1, (size_t)len, f) != (size_t)len || len < 12 || memcmp(d, "MDLS", 4) || le32(d + 4) < 3 || le32(d + 4) > 7) {
        fclose(f); fprintf(stderr, "%s: not a version 3 to 7 lockstep file\n", path); return 0;
    }
    fclose(f);
    version = (int)le32(d + 4);
    size_t o = 12, cap = 1024;
    recs = malloc(cap * sizeof *recs);
    while (o + 14 <= (size_t)len && d[o] == 1) {
        if (nrecs == cap) recs = realloc(recs, (cap *= 2) * sizeof *recs);
        record *r = &recs[nrecs++];
        r->flags = d[o + 1]; r->frame = le32(d + o + 2); r->at = le32(d + o + 6);
        r->buttons = le16(d + o + 10);
        r->writes = 0xFFFFFFFFu;
        r->ret_routine = 0;
        if (version >= 5) { r->writes = le32(d + o + 12); o += 4; }
        if (version >= 6) { r->ret_routine = routine_at(le32(d + o + 12) & 0xFFFFFF); o += 4; }
        r->nhv = le16(d + o + 12);
        r->hv = d + o + 14;
        o += 14 + 3 * (size_t)r->nhv;
        r->ram = le64(d + o); r->vdp = le64(d + o + 8);
        o += 16;
        r->routine = routine_at(r->at);
        r->at_rts = version >= 6 && rd16(rom_ptr(r->at)) == 0x4E75;
        r->past_dfng = 0;
        for (uint32_t a = r->routine; a + 6 <= r->at && a < r->routine + 0x40; a += 2)
            if (rd16(rom_ptr(a)) == 0x08F8 && rd16(rom_ptr(a + 2)) == 2 && rd16(rom_ptr(a + 4)) == (uint16_t)RAM_OFF_disflags) { r->past_dfng = 1; break; }
        r->state = r->mask = NULL;
        if (r->flags & 2) { r->state = d + o; o += STATE_SIZE; }
        if ((r->flags & 2) && version >= 4) { r->mask = d + o; o += MASK_SIZE; }
    }
    mdrt_set_hv_source(m, hv_source, NULL);
    mdrt_set_pad_source(m, pad_source, NULL);
    tracking = 1;
    next_span();
    printf("%s: %zu vblanks\n", path, nrecs);
    return 1;
}

static uint64_t vdp_hash(void)
{
    mdrt_vdp_state s;
    mdrt_vdp_get_state(mdrt_get_vdp(native_m), &s);
    uint8_t c[128], v[80];
    for (int i = 0; i < 64; i++) { c[2 * i] = (uint8_t)(s.cram[i] >> 8); c[2 * i + 1] = (uint8_t)s.cram[i]; }
    for (int i = 0; i < 40; i++) { v[2 * i] = (uint8_t)(s.vsram[i] >> 8); v[2 * i + 1] = (uint8_t)s.vsram[i]; }
    return fnv64(fnv64(fnv64(fnv64(0xCBF29CE484222325ull, s.reg, 24), s.vram, 0x10000), c, 128), v, 80);
}

static uint32_t be32(const uint8_t *p) { return (uint32_t)p[0] << 24 | p[1] << 16 | p[2] << 8 | p[3]; }

/* the oracle's 68k in setvideo's wait for dfok, reached from DoGameFrame (its jmp) called from
 * Gameloop: above the exception frame (SR, PC) setvideo's movem of d0-d7 / a0-a6 (60 bytes), then
 * the return into Gameloop */
static int at_game_loop(const record *r)
{
    if (!r->state || r->at < SYM_setvideo || r->at >= SYM_setvideo + 12) return 0;
    const uint8_t *regs = r->state + 0x10000 + 24 + 0x10000 + 128 + 80;   /* (D0-D7, A0-A7) */
    uint32_t sp = le32(regs + 15 * 4) & 0xFFFF;
    if (sp + 70 > 0x10000) return 0;
    uint32_t ret = be32(r->state + sp + 66) & 0xFFFFFF;
    return ret > SYM_Gameloop && ret < SYM_DoGameFrame;
}

/* the end of routine a's code: the next routine's address */
static uint32_t routine_end(uint32_t a)
{
    uint32_t best = 0xFFFFFFFFu;
    for (size_t i = 0; i < sizeof rom_syms / sizeof *rom_syms; i++)
        if (rom_syms[i].addr > a && rom_syms[i].addr < best) best = rom_syms[i].addr;
    return best;
}

/* the oracle at record r (its whole state) out of the game's logic: in Pausemode or
 * IntermissionStart, or on its stack, from the top, a return into one of them before one into
 * DoGameFrame (any long word there that is such an address counts); else (DoGameFrame, the game
 * loop between two frames, no state) in it */
static int oracle_out_of_game_logic(const record *r)
{
    static uint32_t dgf_end, int_end, pause_end;
    if (!dgf_end) {
        dgf_end = routine_end(SYM_DoGameFrame);
        int_end = routine_end(SYM_IntermissionStart);
        pause_end = routine_end(SYM_Pausemode);
    }
    if (!r->state) return 0;
    if (r->routine == SYM_Pausemode || r->routine == SYM_IntermissionStart) return 1;
    const uint8_t *regs = r->state + 0x10000 + 24 + 0x10000 + 128 + 80;   /* (D0-D7, A0-A7) */
    for (uint32_t a = le32(regs + 15 * 4) & 0xFFFF; a + 4 <= 0x10000; a += 2) {   /* (the stack: up to the top of RAM) */
        uint32_t v = be32(r->state + a);
        if ((v > SYM_IntermissionStart && v < int_end) || (v > SYM_Pausemode && v < pause_end)) return 1;
        if (v > SYM_DoGameFrame && v < dgf_end) return 0;
    }
    return 0;
}

/* the native state against the oracle's whole state at record r, but for the lag mask: the first
 * difference's description, or NULL */
static const char *masked_diff(const record *r)
{
    static char what[160];
    const uint8_t *p = r->state, *m = r->mask, *ram = ram_ptr(RAM_BASE);
    #define MASKED(mask, i) ((mask)[(i) >> 3] & (1 << ((i) & 7)))
    /* the game's copy of vcount (its waits keep it: oldvcount = vcount) differs with it */
    int vc = MASKED(m, RAM_OFF_vcount) || MASKED(m, RAM_OFF_vcount + 1);
    int n = 0, first = -1;
    for (uint32_t i = 0; i < RAM_VARS_END; i++) {
        if (vc && (i == RAM_OFF_oldvcount || i == RAM_OFF_oldvcount + 1)) continue;
        if (ram[i] != p[i] && !MASKED(m, i)) { if (first < 0) first = i; n++; }
    }
    if (n) { snprintf(what, sizeof what, "work RAM (%d bytes, the first at %04x: %02x, the oracle's %02x)", n, first, ram[first], p[first]); return what; }
    mdrt_vdp_state s;
    mdrt_vdp_get_state(mdrt_get_vdp(native_m), &s);
    p += 0x10000; m += 0x2000;
    for (int i = 0; i < 24; i++) if (s.reg[i] != p[i] && !MASKED(m, i)) { snprintf(what, sizeof what, "VDP register %d (%02x, the oracle's %02x)", i, s.reg[i], p[i]); return what; }
    p += 24; m += 4;
    for (int i = 0; i < 0x10000; i++) if (s.vram[i] != p[i] && !MASKED(m, i)) { if (first < 0) first = i; n++; }
    if (n) { snprintf(what, sizeof what, "VRAM (%d bytes, the first at %04x: %02x, the oracle's %02x)", n, first, s.vram[first], p[first]); return what; }
    p += 0x10000; m += 0x2000;
    for (int i = 0; i < 128; i++)
        if ((uint8_t)(s.cram[i >> 1] >> (i & 1 ? 0 : 8)) != p[i] && !MASKED(m, i)) { snprintf(what, sizeof what, "CRAM colour %d (%03x, the oracle's %02x%02x)", i >> 1, s.cram[i >> 1], p[i & ~1], p[i | 1]); return what; }
    p += 128; m += 16;
    for (int i = 0; i < 80; i++)
        if ((uint8_t)(s.vsram[i >> 1] >> (i & 1 ? 0 : 8)) != p[i] && !MASKED(m, i)) { snprintf(what, sizeof what, "VSRAM word %d", i >> 1); return what; }
    return NULL;
}

/* the oracle's whole state at its record: work RAM and the VDP (and d7 as the game loop's
 * `elapsed`, in the game loop) */
static void take_state(const record *r, int game_loop)
{
    const uint8_t *p = r->state;
    memcpy(ram_ptr(RAM_BASE), p, 0x10000); p += 0x10000;
    mdrt_vdp_state s;
    memcpy(s.reg, p, 24); p += 24;
    memcpy(s.vram, p, 0x10000); p += 0x10000;
    for (int i = 0; i < 64; i++, p += 2) s.cram[i] = (uint16_t)(p[0] << 8 | p[1]);
    for (int i = 0; i < 40; i++, p += 2) s.vsram[i] = (uint16_t)(p[0] << 8 | p[1]);
    mdrt_vdp_set_state(mdrt_get_vdp(native_m), &s);
    if (game_loop) elapsed = (int16_t)le32(p + 7 * 4);    /* d7: the frames the game loop's pass covers */
    p += 64;
    memcpy(z80_ram, p, 0x2000); p += 0x2000;              /* the Z80: its RAM and where it was */
    if (p[7] & 1) {
        uint64_t now = z80.now;
        z80_relay_seed((uint16_t)(p[0] | p[1] << 8), (uint16_t)(p[2] | p[3] << 8), p[4], p[5]);
        z80.now = z80.zc = now;
        z80.bus_taken = (p[7] & 2) ? 1 : 0;
    } else z80_relay_reset(0);
}

static int failed;                      /* a check differed */
static void differs(const char *what, long frame, const record *r)
{
    failed = 1;
    printf("lockstep: %s differs at the game's vblank wait %ld (the oracle's frame %u, the 68k at %06x); "
           "%ld waits checked, %ld lag frames before\n", what, frame, r->frame, r->at, checked, lagged);
    FILE *f = fopen("out/lockstep-native.ram", "wb");
    if (f) { fwrite(ram_ptr(RAM_BASE), 1, 0x10000, f); fclose(f); }
    if ((f = fopen("out/lockstep-native.ram.vdp", "wb"))) {   /* as lockstep.py --dump writes the oracle's */
        mdrt_vdp_state s;
        mdrt_vdp_get_state(mdrt_get_vdp(native_m), &s);
        fwrite(s.reg, 1, 24, f);
        fwrite(s.vram, 1, 0x10000, f);
        for (int i = 0; i < 64; i++) { fputc(s.cram[i] >> 8, f); fputc(s.cram[i] & 0xFF, f); }
        for (int i = 0; i < 40; i++) { fputc(s.vsram[i] >> 8, f); fputc(s.vsram[i] & 0xFF, f); }
        fclose(f);
    }
    native_quit();
}

/* the next interrupt came at the rte of the oracle's vblank code, which ran longer than a frame
 * (version 7, flags bit 3): the native game, whose DMA takes no time, runs that vblank at once
 * too, before any of its own work */
int lockstep_vblank_again(void)
{
    return recs && span_lo < nrecs && span_hi - span_lo == 1 && (recs[span_lo].flags & 9) == 9;
}

/* 0: every vblank of the recording passed, 1: a check differed, 2: the run stopped first */
int lockstep_result(void)
{
    if (!recs) return 0;
    return failed ? 1 : span_lo < nrecs ? 2 : 0;
}

static int reported;
void lockstep_report(void)
{
    if (!recs || reported) return;
    reported = 1;
    printf("lockstep: %zu of %zu vblanks: %ld of the game's vblank waits checked, all the same as the oracle's; %ld lag "
           "frames (the oracle's vblank in the game's logic); the oracle's state taken %ld times in the game loop "
           "(%ld times out of step until a point in the game loop) and %ld times elsewhere after a check but for "
           "what its lag frames' vblank code changed; %ld lag frames placed in the game's work (by its port accesses)\n",
           span_lo, nrecs, checked, lagged, resynced, skipped, masked, placed);
}

static uint32_t accesses_now;            /* the game's port accesses so far */

static int due(uint32_t n, int unmask);
static int armed;                       /* the next lag frame's access count has come: waiting for its routine */

/* the span done: on to the next; the game's port access count is the oracle's from here (it
 * drifts in the game's logic, where lag frames are not placed, and over resyncs) */
static void advance(const record *r)
{
    if (version >= 5) {
        if (getenv("LOCKSTEP_DEBUG") && accesses_now != r->writes)
            printf("  the game's port accesses %u, the oracle's %u at its frame %u\n", accesses_now, r->writes, r->frame);
        native_set_accesses(r->writes);
        accesses_now = r->writes;
    }
    lagged += (long)(span_hi - span_lo - 1);
    buttons = r->buttons;
    next = span_hi;
    next_span();
    armed = due(accesses_now, 0);                         /* (a lag frame due with no access first: in its routine) */
}

/* ---- where the native game is: the routine it is in --------------------------------------------
 * build/nhl94 compiles src/ with -finstrument-functions: each of its functions' entries and exits
 * comes here. The C functions have the 68k routines' names (rom_syms gives their addresses), so
 * the top of this stack is the 68k routine the native game is in (0: a C helper of its own). */
#include <dlfcn.h>
#define NI __attribute__((no_instrument_function))
static struct { void *fn; uint32_t routine; } fncache[4096];
static uint32_t where[512];
static int depth, run68k_depth;

static NI uint32_t routine_of(void *fn)
{
    size_t h = ((uintptr_t)fn >> 4) & 4095;
    while (fncache[h].fn && fncache[h].fn != fn) h = (h + 1) & 4095;
    if (fncache[h].fn) return fncache[h].routine;
    uint32_t r = 0;
    Dl_info info;
    if (dladdr(fn, &info) && info.dli_saddr == fn && info.dli_sname)
        for (size_t i = 0; i < sizeof rom_syms / sizeof *rom_syms; i++)
            if (!strcmp(rom_syms[i].name, info.dli_sname)) { r = rom_syms[i].addr; break; }
    fncache[h].fn = fn;
    fncache[h].routine = r;
    return r;
}

static void try_place(void);
static void place_next(const char *how);
static int in_game_logic(void);

void NI __cyg_profile_func_enter(void *fn, void *site)
{
    (void)site;
    if (!tracking || native_in_vblank()) return;
    if (depth < 512) where[depth] = routine_of(fn);
    depth++;
    if (fn == (void *)run68k) run68k_depth = depth;
    if (armed) try_place();
}

/* restart_at's longjmp back into run68k: the frames above it are gone */
void NI run68k_unwind(void) { depth = run68k_depth; }

void NI __cyg_profile_func_exit(void *fn, void *site)
{
    (void)fn; (void)site;
    if (!tracking || native_in_vblank()) return;
    if (depth > 0) depth--;
    while (armed && recs[lag_next].at_rts && depth > 0 && depth <= 512 && where[depth - 1] == recs[lag_next].ret_routine) {
        place_next("at the return");                      /* the oracle was at an rts: the return into its caller */
        armed = due(accesses_now, 0);
    }
    if (armed) try_place();
}

/* the oracle's lag interrupts, placed in the native game's work outside the game (version 5): once
 * the game has made as many of the counted port accesses as the oracle had, in the routine the
 * oracle was interrupted in (or, failing that, before the next access or at the next wait); one
 * that came while the interrupts were masked at the unmask after as many */
static NI void place_next(const char *how)
{
    const record *l = &recs[lag_next++];
    buttons = l->buttons;
    placed++;
    if (in_game_logic()) logic_lags = 1;
    armed = 0;
    if (getenv("LOCKSTEP_DEBUG")) printf("  lag frame %u (68k at %06x, %u accesses) placed %s%s\n", l->frame, l->at, l->writes, how,
                                         in_game_logic() ? " (in the game's logic)" : "");
    native_vblank_now();
}
/* follow the routines the native game is in from now on (states need to know where it waits;
 * call it before the game starts) */
void NI native_track_routines(void) { tracking = 1; }

/* the native game is at the game loop's wait: setvideo's, in DoGameFrame from Gameloop (the C
 * helpers between them aside). native_resume_game_loop goes on from there, so states are taken
 * there, as lockstep takes the oracle's. */
int NI native_at_game_loop_wait(void)
{
    static const uint32_t want[3] = { SYM_setvideo, SYM_DoGameFrame, SYM_Gameloop };
    int k = 0;
    for (int i = (depth < 512 ? depth : 512) - 1; i >= 0 && k < 3; i--) {
        if (!where[i]) continue;
        if (where[i] != want[k]) return 0;
        k++;
    }
    return k == 3;
}

/* the native game is in a frame's game logic (DoGameFrame on its function stack, and not the
 * intermission or the game over a period's end runs from inside it, nor the pause menu and its
 * screens): the oracle's lag frames there fall in long stretches without port accesses, where a
 * routine runs many times a frame, so they are not placed unless their span ends with the oracle
 * out of it (the pause menu's fade, which Gameloop runs after a frame of the game's logic): there
 * is no point in the game loop to come for the player to take the oracle's state at, as it does
 * for the others */
static NI int in_game_logic(void)
{
    for (int i = (depth < 512 ? depth : 512) - 1; i >= 0; i--) {
        if (where[i] == SYM_IntermissionStart || where[i] == SYM_Pausemode) return 0;
        if (where[i] == SYM_DoGameFrame) return 1;
    }
    return 0;
}
static NI int due(uint32_t n, int unmask)
{
    if (!recs || lag_next + 1 >= span_hi) return 0;
    if (in_game_logic()) {                                /* the span ends where the oracle is still in it: not placed */
        if (span_end_logic < 0) span_end_logic = !oracle_out_of_game_logic(&recs[span_hi - 1]);
        if (span_end_logic) return 0;
    }
    const record *l = &recs[lag_next];
    return !!(l->flags & 4) == unmask && l->writes <= n;
}
static NI void try_place(void)
{
    /* in its routine: at once (at the routine's entry, or after an access in it), unless the
     * routine had already set dfng there (then before its next access, past the bset) */
    while (armed && depth > 0 && depth <= 512 && where[depth - 1] == recs[lag_next].routine && !recs[lag_next].past_dfng
           && !recs[lag_next].at_rts) {
        place_next("in its routine");
        armed = due(accesses_now, 0);
    }
}
void NI lockstep_access(uint32_t n)
{
    accesses_now = n;
    if (!armed) armed = due(n, 0);
    try_place();
}
void NI lockstep_before_access(void)
{
    while (armed) {                                       /* not in its routine before the next access: here */
        place_next("before the next access");
        armed = due(accesses_now, 0);
    }
}
void NI lockstep_unmask(uint32_t n)
{
    accesses_now = n;
    if (getenv("LOCKSTEP_DEBUG") && recs && lag_next + 1 < span_hi && (recs[lag_next].flags & 4) && !due(n, 1))
        printf("  an unmask at %u accesses: lag frame %u (68k at %06x, %u accesses) not due%s\n", n, recs[lag_next].frame,
               recs[lag_next].at, recs[lag_next].writes, in_game_logic() ? " (in the game logic)" : "");
    while (due(n, 1)) place_next("at an unmask");
    if (!armed) armed = due(n, 0);
    try_place();
}

/* at the game's vblank wait: the check against the oracle's interrupt the span ended at */
void lockstep_check(long frame)
{
    if (!recs) return;
    while (armed) {                                       /* not placed before the wait: at it */
        place_next("at the wait");
        armed = due(accesses_now, 0);
    }
    /* the game at the wait the span ends at (in its routine), off the game loop, short of the port
     * accesses the oracle's last lag frames had, after lag frames placed in the game's logic (the count
     * drifts there): those, here */
    if (logic_lags && span_lo < nrecs && lag_next + 1 < span_hi && !at_game_loop(&recs[span_hi - 1]) && depth > 0 && depth <= 512
        && where[depth - 1] == recs[span_hi - 1].routine && recs[span_hi - 1].routine && !in_game_logic())
        while (lag_next + 1 < span_hi) place_next("at the wait (the access count short)");
    const record *r = span_lo < nrecs ? &recs[span_hi - 1] : NULL;
    if (!r || !(r->flags & 1)) {                          /* the recording's end (perhaps in lag frames) */
        if (r) lagged += (long)(span_hi - span_lo);
        span_lo = nrecs;
        lockstep_report();
        native_quit();
    }
    if (restarted) { restarted = 0; return; }             /* the same wait, from the restart */
    if (getenv("LOCKSTEP_DEBUG"))
        printf("  check: the oracle's frame %u (68k at %06x), the game in %06x, vcount %04x oldvcount %04x disflags %04x vbint %06x\n",
               r->frame, r->at, depth > 0 && depth <= 512 ? where[depth - 1] : 0, g.vcount, g.oldvcount, g.disflags, g.vbint);
    if (logic_lags && r->state && !at_game_loop(r) && depth > 0 && depth <= 512 && where[depth - 1] == r->routine
        && r->routine && !in_game_logic()) {
        /* the span's lag frames fell in the game's logic, where their places are not exact, and it
         * ends off the game loop (the pause menu's fade): the oracle's state at this wait, as at the
         * game loop, the native game at the same wait */
        advance(r);
        take_state(r, 0);
        resynced++;
        insync = 1;
        return;
    }
    int lag = span_hi - span_lo > 1 && lag_next + 1 < span_hi;   /* lag frames not placed */
    if (lag || !insync) {
        if (at_game_loop(r)) {                            /* take the oracle's state and go on from there */
            advance(r);
            take_state(r, 1);
            resynced++;
            insync = 1;
            restarted = 1;
            restart_at(native_resume_game_loop);
        }
        if (r->mask && !in_game_logic()) {                /* outside the game: check, then take it here */
            if (getenv("LOCKSTEP_DEBUG"))
                for (size_t i = lag_next; i + 1 < span_hi; i++)
                    printf("  lag frame %u (68k at %06x, %u accesses) not placed (the game at %u accesses, routine %06x)\n",
                           recs[i].frame, recs[i].at, recs[i].writes, accesses_now, depth > 0 && depth <= 512 ? where[depth - 1] : 0);
            const char *bad = masked_diff(r);
            if (bad) differs(bad, frame, r);
            advance(r);
            take_state(r, 0);
            masked++;
            insync = 1;
            return;
        }
        advance(r);
        if (insync) skipped++;
        insync = 0;
        return;
    }
    uint64_t rh = fnv64(0xCBF29CE484222325ull, ram_ptr(RAM_BASE), RAM_VARS_END), vh = vdp_hash();
    size_t want = 0, used = hv_used;                      /* the HV reads of the whole span (its lag frames' too) */
    for (size_t i = span_lo; i < span_hi; i++) want += recs[i].nhv;
    for (size_t i = span_lo; i < hv_rec && i < span_hi; i++) used += recs[i].nhv;
    const char *bad = rh != r->ram ? "work RAM" : vh != r->vdp ? "VDP state" : hv_over || used != want ? "HV reads" : NULL;
    if (bad) differs(bad, frame, r);
    checked++;
    advance(r);
}
