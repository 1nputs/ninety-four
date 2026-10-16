/* States of the native game: md-runtime's whole console (mdrt_get_state) with the game's own
 * parts, work RAM ("RAM "), the Z80 relay ("Z80 ": its RAM and its place in its program) and
 * "NHL " (the C port's registers kept across routines: elapsed, the assignment dispatch's d2, the
 * line editor's d6 / d7; and the HAL's clock). A state is taken at the game loop's wait (setvideo's,
 * in DoGameFrame from Gameloop): the C stack there holds nothing native_resume_game_loop does not
 * redo, which is how lockstep goes on after it takes the oracle's state. Loading one restarts the
 * game there (restart_at), from wherever it was.
 *
 * In the window: F5 saves a state to a file, F9 loads it, Backspace held steps back (a state every
 * third frame of play, ten seconds of them). Headless: --save-state FRAME:FILE, --load-state FILE
 * (tools/golden.py checks that a loaded state goes on bit for bit as the run it was taken from). */
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "game.h"
#include "native.h"
#include "z80.h"

static mdrt *m;
static const char *file;                /* F5 / F9 */
static long save_frame = -1;            /* --save-state */
static const char *save_path;
static long seen;                       /* the handshake whose requests were taken */

/* ---- the game's parts ------------------------------------------------------------------------ */

/* the relay: Z80 RAM, then the relay's struct up to its steps, the steps' counters, and the
 * steps of the routine it is in (the rest are never read) */
#define Z80_HEAD offsetof(struct Z80Relay, ops)
static size_t z80_save(void *user, uint8_t *out)
{
    (void)user;
    size_t n = 0x2000 + Z80_HEAD + 7 + (size_t)z80.nops * sizeof *z80.ops;
    if (out) {
        memcpy(out, z80_ram, 0x2000); out += 0x2000;
        memcpy(out, &z80, Z80_HEAD); out += Z80_HEAD;
        memcpy(out, &z80.nops, 2); memcpy(out + 2, &z80.op, 2); memcpy(out + 4, &z80.ret, 2); out[6] = z80.poll; out += 7;
        memcpy(out, z80.ops, (size_t)z80.nops * sizeof *z80.ops);
    }
    return n;
}
static int z80_load(void *user, const uint8_t *in, size_t size)
{
    (void)user;
    uint16_t nops;
    if (size < 0x2000 + Z80_HEAD + 7) return -1;
    memcpy(&nops, in + 0x2000 + Z80_HEAD, 2);
    if (nops > Z80_OPS || size != 0x2000 + Z80_HEAD + 7 + (size_t)nops * sizeof *z80.ops) return -1;
    memcpy(z80_ram, in, 0x2000); in += 0x2000;
    memcpy(&z80, in, Z80_HEAD); in += Z80_HEAD;
    memcpy(&z80.nops, in, 2); memcpy(&z80.op, in + 2, 2); memcpy(&z80.ret, in + 4, 2); z80.poll = in[6]; in += 7;
    memcpy(z80.ops, in, (size_t)nops * sizeof *z80.ops);
    return 0;
}

/* the C port's registers kept across routines, then the HAL's clock */
static size_t nhl_save(void *user, uint8_t *out)
{
    (void)user;
    if (out) {
        uint16_t e = (uint16_t)elapsed;
        memcpy(out, &e, 2); memcpy(out + 2, &assign_d2, 4); memcpy(out + 6, &le_player, 2); memcpy(out + 8, &le_attr, 2);
    }
    return 10 + native_hal_save(out ? out + 10 : NULL);
}
static int nhl_load(void *user, const uint8_t *in, size_t size)
{
    (void)user;
    if (size < 10 || native_hal_load(in + 10, size - 10)) return -1;
    uint16_t e;
    memcpy(&e, in, 2); elapsed = (int16_t)e;
    memcpy(&assign_d2, in + 2, 4); memcpy(&le_player, in + 6, 2); memcpy(&le_attr, in + 8, 2);
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
    mdrt_add_state_memory(m, "RAM ", ram_ptr(RAM_BASE), 0x10000);
    mdrt_add_state_part(m, "Z80 ", z80_save, z80_load, NULL);
    mdrt_add_state_part(m, "NHL ", nhl_save, nhl_load, NULL);
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
    if ((req & MDRT_REQUEST_SAVE) && file) {
        if (!at) printf("nhl94: a state is saved in play (not in the menus or a pause)\n");
        else if (save(file)) printf("nhl94: state saved into %s\n", file);
        else fprintf(stderr, "nhl94: cannot write %s\n", file);
    }
    if ((req & MDRT_REQUEST_LOAD) && file && native_load_state_file(file) == 0) {
        printf("nhl94: state loaded from %s\n", file);
        ring_n = 0;
        restart_at(native_resume_game_loop);
    }
    if (!rewind_on || !at) return;
    if ((req & MDRT_REQUEST_REWIND) && ring_n) {          /* a step back: the newest, kept when it is the last */
        int i = (ring_first + ring_n - 1) % REWIND_STATES;
        if (ring_n > 1) ring_n--;
        if (mdrt_set_state(m, ring[i].buf, ring[i].len) == 0) restart_at(native_resume_game_loop);
    } else if (!(req & MDRT_REQUEST_REWIND) && native_frames() % REWIND_EVERY == 0) {
        ring_push();
    }
}
