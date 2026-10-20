/* States of the native game: pulseman's whole console (mdrt_get_state) with the game's own
 * parts, work RAM ("RAM "), the Z80 relay ("Z80 ": its RAM and its place in its program) and
 * "NHL " (the C port's registers kept across routines: elapsed, the assignment dispatch's d2, the
 * line editor's d6 / d7; and the HAL's clock). A state is taken at the game loop's wait (setvideo's,
 * in DoGameFrame from Gameloop): the C stack there holds nothing native_resume_game_loop does not
 * redo, which is how lockstep goes on after it takes the oracle's state. Loading one restarts the
 * game there (restart_at), from wherever it was.
 *
 * In the window: the menu saves a state into pulseman's slot and loads one (the file the request
 * names: mdrt_request_path; states/slotN.state, or the Autosave's), F4 held steps back (a state every
 * third frame of play, ten seconds of them). Without a named file, --state FILE. A save asked for
 * outside play is not taken (the platform keeps the slot as it was). Online the window takes the
 * session's states itself (one every machine played, the host's loaded for everyone: they are the
 * console's, as these are), asking native_state_check whether the session may load one. Headless:
 * --save-state FRAME:FILE, --load-state FILE, --load-at FRAME:FILE (main.c)
 * (tools/golden.py checks that a loaded state goes on bit for bit as the run it was taken from). */
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "game.h"
#include "native.h"
#include "z80.h"

static mdrt *m;
static const char *file;                /* --state: a save's or load's when the platform names none */
static long save_frame = -1;            /* --save-state */
static const char *save_path;
static long seen;                       /* the handshake whose requests were taken */

/* ---- the game's parts ------------------------------------------------------------------------ */

/* work RAM as the 68000 has it (the state's bytes are the console's, whatever the host) */
static size_t ram_save(void *user, uint8_t *out)
{
    (void)user;
    if (out) ram_pack(out);
    return RAM_SIZE;
}
static int ram_load(void *user, const uint8_t *in, size_t size)
{
    (void)user;
    if (size != RAM_SIZE) return -1;
    ram_unpack(in);
    return 0;
}

/* the relay: Z80 RAM, then the relay's struct up to its steps, the steps' counters, and the
 * steps of the routine it is in (the rest are never read). Field by field, little-endian, so a
 * state is the same bytes on every host and compiler (netplay compares the simulation's hash):
 * as the struct was on a little-endian host, a zero where it had a byte of padding */
#define Z80_HEAD 58
#define Z80_OP 6
static uint8_t *put16(uint8_t *o, uint16_t v) { o[0] = (uint8_t)v; o[1] = (uint8_t)(v >> 8); return o + 2; }
static uint8_t *put64(uint8_t *o, uint64_t v) { for (int k = 0; k < 8; k++) o[k] = (uint8_t)(v >> 8 * k); return o + 8; }
static uint16_t get16(const uint8_t **i) { uint16_t v = (uint16_t)((*i)[0] | (*i)[1] << 8); *i += 2; return v; }
static uint64_t get64(const uint8_t **i) { uint64_t v = 0; for (int k = 0; k < 8; k++) v |= (uint64_t)(*i)[k] << 8 * k; *i += 8; return v; }

static size_t z80_save(void *user, uint8_t *out)
{
    (void)user;
    size_t n = 0x2000 + Z80_HEAD + 7 + (size_t)z80.nops * Z80_OP;
    if (out) {
        memcpy(out, z80_ram, 0x2000); out += 0x2000;
        out = put64(out, z80.now); out = put64(out, z80.zc); out = put64(out, z80.frame); out = put64(out, z80.ym_free);
        *out++ = z80.running; *out++ = z80.bus_taken; *out++ = z80.prog; *out++ = 0;
        out = put16(out, z80.pc); out = put16(out, z80.hl); out = put16(out, z80.iy); out = put16(out, z80.sp);
        *out++ = z80.a; *out++ = z80.b; *out++ = z80.c; *out++ = z80.d; *out++ = z80.e; *out++ = z80.carry;
        out = put16(out, z80.bc2); out = put16(out, z80.de2); out = put16(out, z80.hl2);
        *out++ = z80.bank; *out++ = z80.n;
        out = put16(out, z80.nops); out = put16(out, z80.op); out = put16(out, z80.ret); *out++ = z80.poll;
        for (int k = 0; k < z80.nops; k++) {
            const struct Z80Op *o = &z80.ops[k];
            *out++ = o->kind; *out++ = o->part; *out++ = o->reg; *out++ = o->val;
            out = put16(out, o->cycles);
        }
    }
    return n;
}
static int z80_load(void *user, const uint8_t *in, size_t size)
{
    (void)user;
    if (size < 0x2000 + Z80_HEAD + 7) return -1;
    const uint8_t *at = in + 0x2000 + Z80_HEAD;
    uint16_t nops = get16(&at);
    if (nops > Z80_OPS || size != 0x2000 + Z80_HEAD + 7 + (size_t)nops * Z80_OP) return -1;
    memcpy(z80_ram, in, 0x2000); in += 0x2000;
    z80.now = get64(&in); z80.zc = get64(&in); z80.frame = get64(&in); z80.ym_free = get64(&in);
    z80.running = in[0]; z80.bus_taken = in[1]; z80.prog = in[2]; in += 4;
    z80.pc = get16(&in); z80.hl = get16(&in); z80.iy = get16(&in); z80.sp = get16(&in);
    z80.a = in[0]; z80.b = in[1]; z80.c = in[2]; z80.d = in[3]; z80.e = in[4]; z80.carry = in[5]; in += 6;
    z80.bc2 = get16(&in); z80.de2 = get16(&in); z80.hl2 = get16(&in);
    z80.bank = in[0]; z80.n = in[1]; in += 2;
    z80.nops = get16(&in); z80.op = get16(&in); z80.ret = get16(&in); z80.poll = *in++;
    for (int k = 0; k < nops; k++, in += Z80_OP)
        z80.ops[k] = (struct Z80Op){ in[0], in[1], in[2], in[3], (uint16_t)(in[4] | in[5] << 8) };
    return 0;
}

/* the C port's registers kept across routines, then the HAL's clock (little-endian) */
static size_t nhl_save(void *user, uint8_t *out)
{
    (void)user;
    if (out) {
        uint8_t *o = put16(out, (uint16_t)elapsed);
        o = put16(o, (uint16_t)assign_d2); o = put16(o, (uint16_t)(assign_d2 >> 16));
        o = put16(o, le_player); put16(o, le_attr);
    }
    return 10 + native_hal_save(out ? out + 10 : NULL);
}
static int nhl_load(void *user, const uint8_t *in, size_t size)
{
    (void)user;
    if (size < 10 || native_hal_load(in + 10, size - 10)) return -1;
    elapsed = (int16_t)get16(&in);
    assign_d2 = get16(&in); assign_d2 |= (uint32_t)get16(&in) << 16;
    le_player = get16(&in); le_attr = get16(&in);
    return 0;
}

/* ---- rewind: the last ten seconds of play, a state every third frame ---------------------------- */

#define REWIND_EVERY 3
#define REWIND_STATES 200
static struct { uint8_t *buf; size_t len, cap; } ring[REWIND_STATES];
static int ring_first, ring_n, rewind_on;

static void ring_push(void)
{
    int i = (ring_first + ring_n) % REWIND_STATES;
    if (ring_n == REWIND_STATES) { ring_first = (ring_first + 1) % REWIND_STATES; ring_n--; }
    size_t n = mdrt_get_state(m, NULL, 0);
    if (ring[i].cap < n) {
        uint8_t *b = realloc(ring[i].buf, n);
        if (!b) return;
        ring[i].buf = b;
        ring[i].cap = n;
    }
    ring[i].len = mdrt_get_state(m, ring[i].buf, ring[i].cap);
    ring_n++;
}

/* ---- saving and loading ----------------------------------------------------------------------- */

void native_states_init(mdrt *console, const char *path, int rewind)
{
    m = console;
    file = path;
    rewind_on = rewind;
    ring_first = ring_n = 0;                              /* (a new game: the last one's states are not its own) */
    mdrt_add_state_part(m, "RAM ", ram_save, ram_load, NULL);
    mdrt_add_state_part(m, "Z80 ", z80_save, z80_load, NULL);
    mdrt_add_state_part(m, "NHL ", nhl_save, nhl_load, NULL);
    mdrt_add_state_part(m, "RULE", rules_state_save, rules_state_load, NULL);   /* (the game's rules: a state plays by its own) */
    native_track_routines();
}

static int save(const char *path)
{
    size_t n = mdrt_get_state(m, NULL, 0);
    uint8_t *b = malloc(n);
    FILE *f = b ? fopen(path, "wb") : NULL;
    int ok = f && mdrt_get_state(m, b, n) == n && fwrite(b, 1, n, f) == n;
    if (f && fclose(f)) ok = 0;
    free(b);
    return ok;
}

int native_load_state_file(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "nhl94: cannot open %s\n", path); return -1; }
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *b = n > 0 ? malloc((size_t)n) : NULL;
    int bad = !b || fread(b, 1, (size_t)n, f) != (size_t)n || mdrt_set_state(m, b, (size_t)n);
    fclose(f);
    free(b);
    if (bad) fprintf(stderr, "nhl94: %s is not a state of this build\n", path);
    return bad ? -1 : 0;
}

void native_save_state_at(long frame, const char *path) { save_frame = frame; save_path = path; }

const char *native_state_check(const void *state, size_t size, void *user)
{
    (void)user;
    size_t n = 0;
    const uint8_t *rule = mdrt_state_part(state, size, "RULE", &n);
    uint64_t hash;
    if (!rule || rules_state_rosters(rule, n, &hash) < 0) return "that state is not one of this game's";
    if (hash != rosters_hash()) return "that state's rosters are not the session's";   /* (a guest has the host's alone, by their checksum) */
    return NULL;
}

void native_states_wait(void)
{
    if (!m) return;
    unsigned req = 0;
    if (native_handshakes() != seen) { seen = native_handshakes(); req = mdrt_requests(m); }
    int at = native_at_game_loop_wait();
    if (save_frame >= 0 && at && native_frames() >= save_frame) {
        save_frame = -1;
        if (save(save_path)) printf("nhl94: state saved at frame %ld (golden line %ld) into %s\n", native_frames(), native_golden_lines(), save_path);
        else fprintf(stderr, "nhl94: cannot write %s\n", save_path);
    }
    const char *path = mdrt_request_path(m) ? mdrt_request_path(m) : file;   /* (the platform's slot, or --state) */
    if ((req & MDRT_REQUEST_SAVE) && path) {
        if (!at) printf("nhl94: a state is saved in play (not in the menus or a pause)\n");
        else if (save(path)) printf("nhl94: state saved into %s\n", path);
        else fprintf(stderr, "nhl94: cannot write %s\n", path);
    }
    if ((req & MDRT_REQUEST_LOAD) && path && native_load_state_file(path) == 0) {
        printf("nhl94: state loaded from %s\n", path);
        ring_n = 0;
        restart_at(native_resume_game_loop);
    }
    if (!rewind_on || !at || native_net_on()) return;    /* (netplay: no rewind, and the session keeps its own states) */
    if ((req & MDRT_REQUEST_REWIND) && ring_n) {          /* a step back: the newest, kept when it is the last */
        int i = (ring_first + ring_n - 1) % REWIND_STATES;
        if (ring_n > 1) ring_n--;
        if (mdrt_set_state(m, ring[i].buf, ring[i].len) == 0) restart_at(native_resume_game_loop);
    } else if (!(req & MDRT_REQUEST_REWIND) && native_frames() % REWIND_EVERY == 0) {
        ring_push();
    }
}
