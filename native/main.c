/* NHL '94 on Linux: the C game on pulseman (ext/pulseman) in an SDL2 window, with sound. It
 * starts from power on (Start: the EA screen, the title, the setup screen ...); --snap FILE starts
 * from a snapshot of the oracle instead (tools/oracle/snapshot.py: RAM, VDP, Z80 RAM, the sound
 * chips' registers, the Z80's place, taken at a game-loop wait such as a faceoff's) and enters the
 * game loop. --boot (power on) is the default and still accepted.
 *
 * usage: nhl94 [--rom FILE] [--snap FILE | --boot] [--scale N] [--aspect] [--sram FILE]
 *              [--headless] [--frames N] [--shot FILE.ppm] [--shots DIR --every N] [--wav FILE]
 *              [--input FILE] [--state FILE] [--load-state FILE] [--save-state FRAME:FILE]
 *              [--lockstep FILE | --golden FILE] [--record DIR] [--fm-core nuked|ymfm]
 *              [--fm-model ym2612|ym3438] [--watch ADDR,...]
 *
 * --headless runs without a window or pacing (as fast as it goes): with --frames it stops, --shot
 * writes the last picture, --wav the sound, --input plays pad 1 from a script (tools/oracle/scripts)
 * and --golden writes a line of hashes a frame (make golden). Keys: pulseman's
 * (include/mdrt/platform_sdl2.h, all of them changeable in its settings file): pad 1 arrows,
 * Z / X / C = A / B / C, Enter = Start; pad 2 I J K L, Q / W / E, Tab; two game controllers; Esc
 * quits, P pauses, F11 full screen, Space held fast-forward, "," slow motion, "." a frame while
 * paused, F12 a screenshot, "-" / "=" volume, M mute, F6 / F7 scaling and aspect; F5 saves a state
 * (into --state FILE, out/nhl94.state), F9 loads it, Backspace held steps back (state.c). --scale
 * and --aspect set the window for this run (otherwise the settings' size and aspect).
 * --load-state FILE starts from a native state; --save-state FRAME:FILE (headless) saves one at
 * the first game loop wait from that frame on. --record DIR has pulseman write the run's
 * .vdptrace, .padtrace and .sndtrace into DIR (make runtime-selfcheck replays them). --fm-core and
 * --fm-model choose pulseman's FM core (Nuked-OPN2 by default) and the chip (the YM2612 of the
 * first consoles by default, or the later YM3438). --watch prints work RAM words (hex addresses,
 * e.g. FFC472) at frame 1 and whenever they change. */
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <direct.h>
#define make_dir(d) _mkdir(d)
#else
#include <sys/stat.h>
#define make_dir(d) mkdir(d, 0777)
#endif
#include "game.h"
#include "hal.h"
#include "rom.h"
#include "symbols_gen.h"
#include "z80.h"
#include "native.h"
#include "mdrt/platform_sdl2.h"

static const char *snap, *shot_path, *shots_dir, *wav_path, *lockstep_path, *golden_path, *input_path;
static const char *state_file = "out/nhl94.state", *load_state_path, *save_state_path;
static const char *record_dir, *fm_core;
static uint32_t watch[16];              /* --watch: work RAM words printed when they change */
static uint16_t watched[16];
static int nwatch;
static int fm_model;
static FILE *traces[3];                 /* --record: pulseman's .vdptrace, .padtrace, .sndtrace of the run */
static long save_state_frame = -1;
static int states, rewind_states;       /* states on (the window, or --save-state / --load-state); rewind (the window) */
static long stop_after = -1, shots_every = 600;
static int boot = 1;                    /* power on, unless --snap */
static jmp_buf out;

/* ---- the snapshot --------------------------------------------------------------------------- */

/* "NHLS", version u32; work RAM, VRAM, CRAM, VSRAM, the VDP registers (32), Z80 RAM; version 2
 * adds a flags word (bit 0: the sound chips' registers follow, a snapshot from power on), the
 * YM2612's 512 and the PSG's 17 register bytes, the Z80's place (8), the 68k's PC (4) and the
 * I/O chip's 16 registers (tools/oracle/snapshot.py writes it) */
#define SNAP_BASE (8 + 0x10000 + 0x10000 + 128 + 80 + 32 + 0x2000)
static uint8_t snapshot[SNAP_BASE + 4 + 512 + 17 + 8 + 4 + 16];
static size_t snapshot_len;
static int snapshot_full;                                 /* version 2 */

static void read_snapshot(void)
{
    FILE *f = fopen(snap, "rb");
    if (!f) { fprintf(stderr, "cannot open %s (make one with tools/oracle/snapshot.py)\n", snap); exit(1); }
    snapshot_len = fread(snapshot, 1, sizeof snapshot, f);
    fclose(f);
    if (snapshot_len < 8 || memcmp(snapshot, "NHLS", 4)) { fprintf(stderr, "%s: not a snapshot\n", snap); exit(1); }
    uint32_t version = (uint32_t)(snapshot[4] | snapshot[5] << 8 | snapshot[6] << 16 | (uint32_t)snapshot[7] << 24);
    size_t want = version == 1 ? SNAP_BASE : version == 2 ? sizeof snapshot : 0;
    if (!want) { fprintf(stderr, "%s: snapshot version %u (this build reads 1 and 2)\n", snap, version); exit(1); }
    if (snapshot_len != want) { fprintf(stderr, "%s: %zu bytes, a version %u snapshot has %zu\n", snap, snapshot_len, version, want); exit(1); }
    snapshot_full = version == 2;
}

static void record(mdrt *m, int i);

/* the snapshot into the console: RAM, the VDP, Z80 RAM; the sound chips' registers written back
 * over a frame of their own, with the Z80 still; then the Z80 where it was */
static void load_snapshot(mdrt *m)
{
    const uint8_t *p = snapshot + 8;
    memcpy(ram_ptr(RAM_BASE), p, 0x10000); p += 0x10000;
    mdrt_vdp_state s;
    memcpy(s.vram, p, 0x10000); p += 0x10000;
    for (int i = 0; i < 64; i++, p += 2) s.cram[i] = (uint16_t)(p[0] << 8 | p[1]);
    for (int i = 0; i < 40; i++, p += 2) s.vsram[i] = (uint16_t)(p[0] << 8 | p[1]);
    memcpy(s.reg, p, 24); p += 32;
    mdrt_vdp_set_state(mdrt_get_vdp(m), &s);
    memcpy(z80_ram, p, 0x2000); p += 0x2000;
    if (snapshot_full) {                                  /* the I/O chip: the pads' TH direction and the rest */
        const uint8_t *io = snapshot + sizeof snapshot - 16;
        for (int i = 1; i < 16; i++) mdrt_pad_write(mdrt_get_pads(m), 0xA10001u + 2u * (unsigned)i, io[i]);
    }
    record(m, MDRT_TRACE_VDP);                            /* (from the snapshot's picture: the trace's start) */
    record(m, MDRT_TRACE_PADS);
    native_hal_start(m, 0);
    int full = snapshot_full, chips = full && (p[0] & 1);
    const uint8_t *ym = p + 4, *psg = p + 4 + 512, *zs = p + 4 + 512 + 17;
    if (full && !chips) fprintf(stderr, "%s: no sound chip registers (a snapshot from a saved state): the FM sounds start blank\n", snap);
    if (chips) {
        uint32_t at = 0;
        for (int part = 0; part < 2; part++) {            /* every register but the key and DAC ones, A4 before A0 */
            static const uint8_t order[] = { 0xA4, 0xA5, 0xA6, 0xA0, 0xA1, 0xA2, 0xAC, 0xAD, 0xAE, 0xA8, 0xA9, 0xAA };
            for (int r = 0x21; r <= 0xB6; r++) {
                if (r == 0x28 || r == 0x2A || (part && r < 0x30) || (r >= 0xA0 && r < 0xB0)) continue;
                mdrt_ym_write(m, at, part * 2, (uint8_t)r);
                mdrt_ym_write(m, at + 600, part * 2 + 1, ym[part * 256 + r]);
                at += 1400;                               /* past the chip's busy time */
            }
            for (size_t i = 0; i < sizeof order; i++) {
                mdrt_ym_write(m, at, part * 2, order[i]);
                mdrt_ym_write(m, at + 600, part * 2 + 1, ym[part * 256 + order[i]]);
                at += 1400;
            }
        }
        for (int r = 0; r < 8; r++) {                     /* the PSG: each register, then the latch */
            uint16_t v = (uint16_t)(psg[2 * r] | psg[2 * r + 1] << 8);
            mdrt_psg_port(m, at, (uint8_t)(0x80 | r << 4 | (v & 15)));
            if (!(r & 1) && r != 6) mdrt_psg_port(m, at + 100, (uint8_t)(v >> 4 & 0x3F));
            at += 200;
        }
        uint16_t lv = (uint16_t)(psg[2 * psg[16]] | psg[2 * psg[16] + 1] << 8);
        mdrt_psg_port(m, at, (uint8_t)(0x80 | psg[16] << 4 | (lv & 15)));
        if (mdrt_wait_vblank(m)) native_quit();           /* that frame: the snapshot's picture */
        mdrt_vblank_end(m);
        native_hal_start(m, mdrt_frame_mclks(m));
        z80.now = mdrt_frame_mclks(m);
        z80_relay_frame();
    }
    if (full && (zs[7] & 1)) {                            /* the Z80 where it was (in every snapshot) */
        z80_relay_seed((uint16_t)(zs[0] | zs[1] << 8), (uint16_t)(zs[2] | zs[3] << 8), zs[4], zs[5]);
        z80.bus_taken = (zs[7] & 2) ? 1 : 0;
    }
}

/* ---- the session ------------------------------------------------------------------------------ */

static uint8_t read68k(void *user, uint32_t addr) { (void)user; return *mem_ptr(addr); }

void native_quit(void) { longjmp(out, 1); }

/* the game loop, from the 68k at setvideo's wait at the end of a frame of DoGameFrame: the rest
 * of that frame, then the loop */
static void resume_game_loop(void)
{
    setvideo();
    demoread();
    if (g.sflags & HI(sfpz)) Pausemode();
    Gameloop();
}

/* the same from the moment of a vblank interrupt there (lockstep, after it takes the oracle's
 * state): the vblank first, whether or not setvideo would have waited for it */
void native_resume_game_loop(void)
{
    hal_wait_vblank();
    resume_game_loop();
}

/* where the snapshot's 68k was: the rest of what it was doing, before the game loop goes on */
static void resume(void)
{
    const uint8_t *p = snapshot + sizeof snapshot - 20;
    uint32_t pc = snapshot_full ? (uint32_t)(p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24) : 0;
    if (pc >= SYM_setvideo && pc < SYM_setvideo + 12) {   /* in setvideo's wait for dfok, at a game frame's end */
        native_set_sr(0x2300);                            /* the game plays unmasked, as setupice left it */
        run68k(resume_game_loop);
    } else if (pc) {
        fprintf(stderr, "%s: the 68k was at %06x, not a place the native game resumes from: it starts the game loop\n", snap, pc);
    }
}

static void trace_sink(void *user, const void *b, size_t n) { fwrite(b, 1, n, user); }

/* --record: start trace i (MDRT_TRACE_*) or, i < 0, end them all */
static void record(mdrt *m, int i)
{
    static const char *const names[3] = { "nhl94.vdptrace", "nhl94.padtrace", "nhl94.sndtrace" };
    if (!record_dir) return;
    if (i >= 0) {
        char path[1024];
        snprintf(path, sizeof path, "%s/%s", record_dir, names[i]);
        if ((traces[i] = fopen(path, "wb"))) mdrt_record(m, i, trace_sink, traces[i]);
        else fprintf(stderr, "cannot write %s\n", path);
        return;
    }
    for (i = 0; i < 3; i++)
        if (traces[i]) { mdrt_record(m, i, NULL, NULL); fclose(traces[i]); traces[i] = NULL; }
}

void native_game(mdrt *m, void *user)
{
    (void)user;
    native_m = m;
    if (setjmp(out)) { lockstep_report(); record(m, -1); return; }
    if (!load_state_path) record(m, MDRT_TRACE_SOUND);    /* (from the first frame: the chips' every write) */
    if (boot) { record(m, MDRT_TRACE_VDP); record(m, MDRT_TRACE_PADS); }
    if (states) native_states_init(m, rewind_states ? state_file : NULL, rewind_states);
    if (save_state_path) native_save_state_at(save_state_frame, save_state_path);
    if (load_state_path) {                                /* a native state: the game loop from its wait */
        native_hal_start(m, 0);
        if (native_load_state_file(load_state_path)) native_quit();
        for (int i = 0; i < 3; i++) record(m, i);         /* (from the state: the trace's start) */
        run68k(native_resume_game_loop);
    }
    if (boot) {                                           /* power on */
        native_hal_start(m, 0);
        run68k(Start);
    }
    load_snapshot(m);
    resume();
    run68k(Gameloop);
}

/* ---- golden runs and scripted input ---------------------------------------------------------- */

/* golden runs (--golden FILE): a line a frame with hashes of work RAM, the VDP state and Z80 RAM
 * at the game's wait, the picture and the frame's sound, so two builds can be compared bit for
 * bit (make golden, make golden-check). The HV counter comes from a stand-in fixed here (the
 * beam pulseman reported until 2026-10-16), so changes to the runtime's HV model do not show. */
static FILE *golden;
static uint64_t golden_ram, golden_vdp, golden_z80, golden_pic;
static uint32_t golden_beam;

static uint64_t fnv(uint64_t h, const void *p, size_t n)
{
    const uint8_t *b = p;
    for (size_t i = 0; i < n; i++) h = (h ^ b[i]) * 0x100000001B3ull;
    return h;
}
#define FNV0 0xCBF29CE484222325ull

static uint16_t golden_hv(void *user)
{
    mdrt *m = user;
    uint32_t width = mdrt_vdp_width(mdrt_get_vdp(m)) == 320 ? 420 : 342, lines = mdrt_frame_mclks(m) / 3420;
    golden_beam += 97;
    uint32_t line = (224 + golden_beam / width) % lines, h = golden_beam % width;
    return (uint16_t)((line & 0xFF) << 8 | (h >> 1));
}

static void golden_wait(void)
{
    mdrt_vdp_state s;
    mdrt_vdp_get_state(mdrt_get_vdp(native_m), &s);
    golden_ram = fnv(FNV0, ram_ptr(RAM_BASE), 0x10000);
    golden_vdp = fnv(fnv(fnv(fnv(FNV0, s.reg, 24), s.vram, 0x10000), s.cram, sizeof s.cram), s.vsram, sizeof s.vsram);
    golden_z80 = fnv(FNV0, z80_ram, 0x2000);
    golden_beam = 0;                                      /* (the beam starts again at the handshake) */
}

static void golden_present(void *user, const uint16_t *px, int w, int h, int pitch)
{
    (void)user;
    golden_pic = FNV0;
    for (int y = 0; y < h; y++) golden_pic = fnv(golden_pic, px + y * pitch, (size_t)w * 2);
}

/* --input FILE: pad 1's buttons from a script ("FRAME:BUTTON+BUTTON,..." as tools/oracle/run.py
 * takes, # lines are comments), held from that frame (the handshakes so far) on. SAVE, LOAD and
 * REWIND are the window's F5, F9 and Backspace, by the handshakes of this process (SAVE and LOAD
 * once, at the handshake they are given) */
#define REQ(r) ((uint32_t)(r) << 16)
static struct { long frame; uint32_t buttons; } *script;
static size_t script_len;
static int script_requests;             /* the script has SAVE, LOAD or REWIND */

static int read_script(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f) { fprintf(stderr, "cannot open %s\n", path); return 0; }
    static const struct { const char *name; uint32_t bit; } names[] = {
        { "UP", MDRT_PAD_UP }, { "DOWN", MDRT_PAD_DOWN }, { "LEFT", MDRT_PAD_LEFT }, { "RIGHT", MDRT_PAD_RIGHT },
        { "A", MDRT_PAD_A }, { "B", MDRT_PAD_B }, { "C", MDRT_PAD_C }, { "START", MDRT_PAD_START },
        { "X", MDRT_PAD_X }, { "Y", MDRT_PAD_Y }, { "Z", MDRT_PAD_Z }, { "MODE", MDRT_PAD_MODE },
        { "SAVE", REQ(MDRT_REQUEST_SAVE) }, { "LOAD", REQ(MDRT_REQUEST_LOAD) }, { "REWIND", REQ(MDRT_REQUEST_REWIND) },
    };
    char line[65536];
    size_t cap = 0;
    while (fgets(line, sizeof line, f)) {
        if (line[0] == '#') continue;
        for (char *item = strtok(line, ",\r\n"); item; item = strtok(NULL, ",\r\n")) {
            char *colon = strchr(item, ':');
            if (!colon) continue;
            if (script_len == cap) script = realloc(script, (cap = cap ? 2 * cap : 256) * sizeof *script);
            script[script_len].frame = atol(item);
            uint32_t b = 0;
            for (char *name = colon + 1; *name; ) {
                size_t n = strcspn(name, "+");
                size_t i = 0;
                while (i < sizeof names / sizeof *names && (strlen(names[i].name) != n || strncmp(names[i].name, name, n))) i++;
                if (i == sizeof names / sizeof *names) { fprintf(stderr, "%s: no button %.*s\n", path, (int)n, name); fclose(f); return 0; }
                b |= names[i].bit;
                name += n + (name[n] == '+');
            }
            script[script_len++].buttons = b;
            if (b >> 16) script_requests = 1;
        }
    }
    fclose(f);
    return 1;
}

/* the script's entry now (the handshakes so far: in a state) */
static size_t script_now(mdrt *m)
{
    long frame = (long)mdrt_frame_count(m);
    size_t now = script_len;
    for (size_t i = 0; i < script_len; i++)
        if (script[i].frame <= frame) now = i;
    return now;
}
static void script_source(void *user, uint16_t b[2])
{
    size_t i = script_now(user);
    b[0] = i < script_len ? (uint16_t)script[i].buttons : 0;
    b[1] = 0;
}
/* the requests by this process's handshakes, not the game's frames (a state loaded puts those
 * back) */
static unsigned script_request(void *user)
{
    (void)user;
    long h = native_handshakes();
    size_t now = script_len;
    for (size_t i = 0; i < script_len; i++)
        if (script[i].frame <= h) now = i;
    if (now == script_len) return 0;
    unsigned r = (unsigned)(script[now].buttons >> 16);
    if (script[now].frame != h) r &= MDRT_REQUEST_REWIND;   /* (SAVE and LOAD once) */
    return r;
}

/* pictures to files */
static void write_ppm(const char *path)
{
    static uint16_t px[320 * 240];
    mdrt_vdp *v = mdrt_get_vdp(native_m);
    int w = mdrt_vdp_width(v), h = mdrt_vdp_height(v);
    mdrt_vdp_render(v, px, 320);
    FILE *f = fopen(path, "wb");
    if (!f) return;
    fprintf(f, "P6\n%d %d\n255\n", w, h);
    for (int i = 0; i < h; i++)
        for (int x = 0; x < w; x++) {
            uint32_t c = mdrt_vdp_rgb(px[i * 320 + x], 1);
            uint8_t rgb[3] = { (uint8_t)(c >> 16), (uint8_t)(c >> 8), (uint8_t)c };
            fwrite(rgb, 1, 3, f);
        }
    fclose(f);
}

void native_frame_done(long frame, int wait)
{
    if (wait) lockstep_check(frame);
    if (golden) golden_wait();
    for (int i = 0; i < nwatch; i++) {
        uint16_t v = rd16(ram_ptr(watch[i]));
        if (v != watched[i] || frame == 1) printf("nhl94: frame %ld: $%06X = $%04X\n", frame, (unsigned)watch[i], v);
        watched[i] = v;
    }
    if (shots_dir && frame % shots_every == 0) {
        char path[512];
        snprintf(path, sizeof path, "%s/f%06ld.ppm", shots_dir, frame);
        write_ppm(path);
    }
    if (stop_after >= 0 && frame >= stop_after) {
        if (shot_path) write_ppm(shot_path);
        native_quit();
    }
}

/* ---- headless: no window, no pacing, the sound to a file -------------------------------------- */

static FILE *wav;
static uint32_t wav_frames;
static long golden_lines;
long native_golden_lines(void) { return golden_lines; }
static void wav_audio(void *user, const int16_t *s, int n)
{
    (void)user;
    if (golden) golden_lines++;
    if (golden) fprintf(golden, "%016llx %016llx %016llx %016llx %016llx %d\n", (unsigned long long)golden_ram,
                        (unsigned long long)golden_vdp, (unsigned long long)golden_z80, (unsigned long long)golden_pic,
                        (unsigned long long)fnv(FNV0, s, (size_t)n * 4), n);
    if (!wav) return;
    fwrite(s, 4, (size_t)n, wav);                         /* (a little-endian host) */
    wav_frames += (uint32_t)n;
}
static void wav_header(uint32_t rate)
{
    uint32_t v, data = wav_frames * 4;
    uint16_t h[2];
    fseek(wav, 0, SEEK_SET);
    fwrite("RIFF", 1, 4, wav); v = 36 + data; fwrite(&v, 4, 1, wav); fwrite("WAVEfmt ", 1, 8, wav);
    v = 16; fwrite(&v, 4, 1, wav); h[0] = 1; h[1] = 2; fwrite(h, 2, 2, wav);
    fwrite(&rate, 4, 1, wav); v = rate * 4; fwrite(&v, 4, 1, wav); h[0] = 4; h[1] = 16; fwrite(h, 2, 2, wav);
    fwrite("data", 1, 4, wav); fwrite(&data, 4, 1, wav);
}

/* the directory a save file goes in, made when it is missing (one level: out/) */
static void make_parent(const char *path)
{
    char d[1024];
    snprintf(d, sizeof d, "%s", path);
    char *slash = strrchr(d, '/');
    if (!slash || slash == d) return;
    *slash = 0;
    make_dir(d);                                          /* (fails harmlessly when it exists) */
}

int main(int argc, char **argv)
{
    const char *rom = "NHL 94 (UE) [!].gen", *sram = "out/nhl94.srm";
    int scale = 0, aspect = 0, headless = 0;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--rom") && i + 1 < argc) rom = argv[++i];
        else if (!strcmp(argv[i], "--snap") && i + 1 < argc) { snap = argv[++i]; boot = 0; }
        else if (!strcmp(argv[i], "--boot")) boot = 1;
        else if (!strcmp(argv[i], "--scale") && i + 1 < argc) scale = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--aspect")) aspect = 1;
        else if (!strcmp(argv[i], "--frames") && i + 1 < argc) stop_after = atol(argv[++i]);
        else if (!strcmp(argv[i], "--shot") && i + 1 < argc) shot_path = argv[++i];
        else if (!strcmp(argv[i], "--sram") && i + 1 < argc) sram = argv[++i];
        else if (!strcmp(argv[i], "--shots") && i + 1 < argc) shots_dir = argv[++i];
        else if (!strcmp(argv[i], "--every") && i + 1 < argc) shots_every = atol(argv[++i]);
        else if (!strcmp(argv[i], "--wav") && i + 1 < argc) wav_path = argv[++i];
        else if (!strcmp(argv[i], "--headless")) headless = 1;
        else if (!strcmp(argv[i], "--lockstep") && i + 1 < argc) { lockstep_path = argv[++i]; headless = 1; }
        else if (!strcmp(argv[i], "--golden") && i + 1 < argc) { golden_path = argv[++i]; headless = 1; }
        else if (!strcmp(argv[i], "--input") && i + 1 < argc) input_path = argv[++i];
        else if (!strcmp(argv[i], "--state") && i + 1 < argc) state_file = argv[++i];
        else if (!strcmp(argv[i], "--record") && i + 1 < argc) record_dir = argv[++i];
        else if (!strcmp(argv[i], "--watch") && i + 1 < argc) {
            for (char *w = strtok(argv[++i], ","); w && nwatch < 16; w = strtok(NULL, ","))
                watch[nwatch++] = (uint32_t)(strtoul(w, NULL, 16) | 0xFF0000) & ~1u;
        }
        else if (!strcmp(argv[i], "--fm-core") && i + 1 < argc) fm_core = argv[++i];
        else if (!strcmp(argv[i], "--fm-model") && i + 1 < argc) fm_model = !strcmp(argv[++i], "ym3438") ? MDRT_FM_YM3438 : MDRT_FM_YM2612;
        else if (!strcmp(argv[i], "--load-state") && i + 1 < argc) { load_state_path = argv[++i]; boot = 0; }
        else if (!strcmp(argv[i], "--save-state") && i + 1 < argc && strchr(argv[i + 1], ':')) {
            save_state_frame = atol(argv[++i]);
            save_state_path = strchr(argv[i], ':') + 1;
        }
        else {
            fprintf(stderr, "usage: %s [--rom FILE] [--snap FILE | --boot] [--scale N] [--aspect] [--sram FILE] [--headless]\n"
                            "       [--frames N] [--shot FILE.ppm] [--shots DIR --every N] [--wav FILE] [--input FILE]\n"
                            "       [--state FILE] [--load-state FILE] [--save-state FRAME:FILE] [--lockstep FILE | --golden FILE]\n"
                            "       [--record DIR] [--fm-core nuked|ymfm] [--fm-model ym2612|ym3438] [--watch ADDR,...]\n", argv[0]);
            return 2;
        }
    }
    if (!rom_load(rom)) { fprintf(stderr, "cannot load the ROM %s\n", rom); return 1; }
    if (sram) make_parent(sram);                          /* (a copy run outside the repository has no out/) */
    if (state_file) make_parent(state_file);
    if (input_path && (lockstep_path || !read_script(input_path))) { if (lockstep_path) fprintf(stderr, "--input: not with --lockstep\n"); return 1; }
    if (snap && load_state_path) { fprintf(stderr, "--snap or --load-state, not both\n"); return 2; }
    if (snap) read_snapshot();
    states = !lockstep_path && (!headless || load_state_path || save_state_path || script_requests);
    rewind_states = !headless || script_requests;
    mdrt_config c = { 0 };
    c.fm_core = fm_core;
    c.fm_model = fm_model;
    c.audio_rate = 48000;
    c.sram_size = 0x2000;
    c.read68k = read68k;
    if (!headless) {
        mdrt_sdl_options o = { .title = "NHL '94", .scale = scale, .aspect = aspect, .sram_path = sram, .app = "ninety-four" };
        return mdrt_sdl_run(&c, &o, native_game, NULL);
    }
    if (wav_path && !(wav = fopen(wav_path, "wb"))) { fprintf(stderr, "cannot write %s\n", wav_path); return 1; }
    if (wav) wav_header(c.audio_rate);
    if (golden_path && !(golden = fopen(golden_path, "w"))) { fprintf(stderr, "cannot write %s\n", golden_path); return 1; }
    c.platform = (mdrt_platform){ .present = golden ? golden_present : NULL, .audio = wav_audio };
    if (script_requests) c.platform.requests = script_request;
    mdrt *m = mdrt_new(&c);
    if (!m) { fprintf(stderr, "no FM core %s in this build of pulseman\n", fm_core ? fm_core : "(default)"); return 1; }
    if (lockstep_path && !lockstep_load(lockstep_path, m)) return 1;
    if (golden) mdrt_set_hv_source(m, golden_hv, m);
    if (input_path) mdrt_set_pad_source(m, script_source, m);
    if (lockstep_path || golden) sram = NULL;             /* lockstep, golden: save RAM blank, as the oracle's at power on */
    FILE *f = sram ? fopen(sram, "rb") : NULL;            /* save RAM: as the window's platform keeps it */
    if (f) { if (fread(mdrt_get_sram(m)->bytes, 1, 0x2000, f) != 0x2000) fprintf(stderr, "%s: short\n", sram); fclose(f); }
    native_game(m, NULL);
    if (sram && mdrt_get_sram(m)->dirty && (f = fopen(sram, "wb"))) { fwrite(mdrt_get_sram(m)->bytes, 1, 0x2000, f); fclose(f); }
    if (wav) { wav_header(c.audio_rate); fclose(wav); }
    if (golden) fclose(golden);
    mdrt_free(m);
    int ls = lockstep_result();                           /* (a run cut short by --frames is not a failure) */
    return ls == 1 || (ls == 2 && stop_after < 0) ? 1 : 0;
}
