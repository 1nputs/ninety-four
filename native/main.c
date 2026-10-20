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
 *              [--fm-model ym2612|ym3438] [--watch ADDR,...] [--restart-at FRAME]
 *              [--four-way] [--input2 FILE] [--input3 FILE] [--input4 FILE]
 *              [--rule NAME=VALUE ...] [--league DIR] [--check-league DIR] [--export-league DIR]
 *              [--extract-rom HACK DIR] [--pack-league DIR FILE.zip] [--art-check | --art-map]
 *              [--hashes FILE] [--host [PORT] | --join ADDR[:PORT] | --peer ADDR[:PORT]] [--port PORT]
 *              [--delay N] [--name NAME] [--here N] [--players N] [--net-sim MS,MS,%,%]
 *
 * --art-check (headless) counts every byte the VDP takes by DMA from the game's data and those in
 * no piece of the art map (art.c: every piece of art the game reads by number, the cartridge's
 * and the league's own), and says so at the end (tools/golden.py makes every run check it);
 * --art-map prints the map too, a piece a line.
 * --headless runs without a window or pacing (as fast as it goes): with --frames it stops, --shot
 * writes the last picture, --wav the sound, --input plays pad 1 from a script (tools/oracle/scripts)
 * and --golden writes a line of hashes a frame (make golden). Keys: pulseman's
 * (include/mdrt/platform_sdl2.h, all of them changeable in its settings file): pad 1 arrows,
 * Z / X / C = A / B / C, Enter = Start; pad 2 I J K L, Q / W / E, Tab; two game controllers; F1
 * mute, F2 / F3 volume, F4 held steps back (state.c), F5 pause, F6 held fast-forward, F7 slow
 * motion, F8 a frame while paused, F10 saves the replay as a GIF, F11 records one, F12 a
 * screenshot, Alt+Enter full screen, Esc the menu (states saved and loaded in its slots, or with
 * none named, --state FILE: out/nhl94.state). The settings are settings.ini in
 * the working directory. --scale and --aspect set the window for this run (otherwise the
 * settings' size and aspect).
 * --load-state FILE starts from a native state; --save-state FRAME:FILE (headless) saves one at
 * the first game loop wait from that frame on. --record DIR has pulseman write the run's
 * .vdptrace, .padtrace and .sndtrace into DIR (make runtime-selfcheck replays them). --fm-core and
 * --fm-model choose pulseman's FM core (Nuked-OPN2 by default, or the settings' in the window)
 * and the chip (the YM2612 of the first consoles by default, or the later YM3438). In the window
 * the ROM is --rom's, else the one chosen last in pulseman's menu (Esc), else "NHL 94 (UE) [!].gen"
 * here; when none is the 1 MB US/Europe cartridge, the menu asks for one. --restart-at FRAME
 * (headless) starts the game again at that frame on a new console, as the menu's reset does
 * (tools/golden.py checks that it goes as a fresh run). --four-way puts EA's 4 Way Play in the
 * ports (the game finds it at power on: four players); --input2 to --input4 script pads 2-4. --watch prints work RAM words (hex addresses,
 * e.g. FFC472) at frame 1 and whenever they change. --rule and --league (headless; the window
 * takes them from its menu's Game rules and League pages) set the game's rules (rules.c) and its
 * league, a folder of teams (league.c: DIR, or a folder of leagues/), whose save RAM is its own
 * (out/nhl94-NAME.srm). --export-league DIR writes the cartridge's league into DIR, to change;
 * --extract-rom HACK DIR a hack's, one made on the community's 32-team template (read as data,
 * never run); --check-league DIR builds one and says what is wrong with it, or what it holds;
 * --pack-league DIR FILE.zip makes a folder a package (its files in a zip: to send, or to keep
 * in leagues/, which plays it as it is). --host, --join and --peer play online (net.c: two
 * players from power on, the host pad 1; in the window it connects first; headless, each plays
 * its pad's script and the run ends at --frames once both have every button), --delay the input
 * delay, --here N the players at this machine, --players N a host's players in all (up to four
 * machines; pads 3 or 4 played: 4 Way Play), --net-sim a simulated network for tests. --hashes FILE (headless) writes a line a frame
 * at its wait, the frame and hashes of work RAM, the VDP, Z80 RAM and save RAM, frames a rollback plays
 * again included (tools/netplay_check.py compares the last of each with a solo run's). */
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
#include "rules.h"
#include "view.h"
#include "mdrt/platform_sdl2.h"

static const char *snap, *shot_path, *shots_dir, *wav_path, *lockstep_path, *golden_path, *input_path[MDRT_PADS];
static int four_way;                    /* --four-way: EA's 4 Way Play in the ports */
static const char *state_file = "out/nhl94.state", *load_state_path, *save_state_path;
static const char *record_dir, *fm_core;
static uint32_t watch[16];              /* --watch: work RAM words printed when they change */
static uint16_t watched[16];
static int nwatch;
static int fm_model, fm_model_given;
static FILE *traces[3];                 /* --record: pulseman's .vdptrace, .padtrace, .sndtrace of the run */
static long save_state_frame = -1;
static int states, rewind_states;       /* states on (the window, or --save-state / --load-state); rewind (the window) */
static long stop_after = -1, shots_every = 600;
static int boot = 1;                    /* power on, unless --snap */
static long restart_frame = -1;         /* --restart-at */
static FILE *hashes;                    /* --hashes: a line a frame at its wait, frames played again included */
static int restarting;
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
    ram_unpack(p); p += 0x10000;
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

/* the VDP's DMA reads (--art-check: each byte from the game's data counted, and those in no piece
 * of the art map) */
static int art_check, art_ranges;
static unsigned long art_bytes, art_lost;
static uint32_t art_first, art_range[8][2];             /* (the first stretches in no piece) */
static uint8_t read68k(void *user, uint32_t addr)
{
    (void)user;
    if (art_check && addr < 0xE00000u) {
        art_bytes++;
        if (!art_piece_at(addr)) {
            if (!art_lost++) art_first = addr;
            int k = 0;
            while (k < art_ranges && !(addr + 1 >= art_range[k][0] && addr <= art_range[k][1])) k++;
            if (k < art_ranges) {
                if (addr < art_range[k][0]) art_range[k][0] = addr;
                if (addr + 1 > art_range[k][1]) art_range[k][1] = addr + 1;
            } else if (art_ranges < 8) {
                art_range[art_ranges][0] = addr;
                art_range[art_ranges++][1] = addr + 1;
            }
        }
    }
    return m8(addr);
}

/* the ROM in the window: pulseman's menu asks whether a file will do, then hands it over */

static uint32_t crc32_of(const uint8_t *b, size_t n)
{
    uint32_t c = 0xFFFFFFFFu;
    while (n--) {
        c ^= *b++;
        for (int k = 0; k < 8; k++) c = c & 1 ? 0xEDB88320u ^ (c >> 1) : c >> 1;
    }
    return ~c;
}

static const char *rom_check(const uint8_t *b, size_t n, void *user)
{
    static char why[64];
    (void)user;
    if (n != ROM_SIZE) { snprintf(why, sizeof why, "%u KB: NHL '94 is a 1 MB cartridge", (unsigned)(n >> 10)); return why; }
    return crc32_of(b, n) == NATIVE_ROM_CRC32 ? NULL : "not the 1 MB US/Europe NHL '94 cartridge";
}

static void rom_use(const uint8_t *b, size_t n, void *user) { (void)n; (void)user; rom_cart = b; rom_art = NULL; rom_limit = ROM_SIZE; }

/* the save RAM's file for the game about to start: the league's own (the window's sram_file) */
static const char *sram_base;
static const char *sram_for(void *user) { (void)user; return league_sram(sram_base); }
/* the window's Start on its Online page: the menu's rules and league (options_use has had them),
 * the league's save RAM */
static const void *window_settings(size_t *size, void *user) { return native_net_settings(sram_for(user), size); }

/* --extract-rom HACK DIR: a hack's league (one made on the 32-team template) written as a folder,
 * read from it as data (romsrc.c): 0, -1 */
static int extract_league(const char *path, const char *dir)
{
    FILE *f = fopen(path, "rb");
    static uint8_t buf[0x400001];
    char why[300], name[64];
    if (!f) { fprintf(stderr, "--extract-rom: cannot read %s\n", path); return -1; }
    size_t n = fread(buf, 1, sizeof buf, f);
    fclose(f);
    struct romsrc s;
    if (romsrc_hack(&s, buf, (uint32_t)n, rom_cart, why, sizeof why) < 0) { fprintf(stderr, "--extract-rom %s: %s\n", path, why); return -1; }
    const char *base = strrchr(path, '/'), *dot;
    base = base ? base + 1 : path;
    dot = strrchr(base, '.');
    snprintf(name, sizeof name, "%.*s", dot && dot > base ? (int)(dot - base) : (int)strlen(base), base);
    printf("%s: %d teams, %d for the cup, %d playoff draws\n", name, s.teams, s.playoff_teams, s.rows);
    if (league_extract(&s, dir, name) < 0) { fprintf(stderr, "--extract-rom %s: %s\n", path, league_error()); return -1; }
    printf("its league is in %s: its files are yours to change (README.md, \"Leagues\")\n", dir);
    return 0;
}

/* --league DIR: the folder (or package, NAME.zip), or one of leagues/ */
static const char *league_folder(const char *arg)
{
    static char dir[600];
    char probe[700];
    FILE *f;
    size_t n = strlen(arg);
    if (n > 4 && !strcmp(arg + n - 4, ".zip")) {          /* (a package: the file, or one of leagues/) */
        if ((f = fopen(arg, "rb"))) { fclose(f); return arg; }
        snprintf(dir, sizeof dir, "leagues/%s", arg);
        if ((f = fopen(dir, "rb"))) { fclose(f); return dir; }
        return arg;
    }
    snprintf(probe, sizeof probe, "%s/teams.csv", arg);
    if ((f = fopen(probe, "rb"))) { fclose(f); return arg; }
    snprintf(dir, sizeof dir, "leagues/%s", arg);
    snprintf(probe, sizeof probe, "%s/teams.csv", dir);
    if ((f = fopen(probe, "rb"))) { fclose(f); return dir; }
    return arg;
}

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
        view_rink_resume();                               /* (the wide view: the snapshot's rink is 32 columns) */
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
    native_tracking_reset();                              /* (the game may have run before: the window's reset) */
    native_net_start(m);                                  /* (netplay: from power on, by the host's settings) */
    native_net_power_on(m);
    rules_power_on();                                     /* (this game's rules and league; a state loaded brings its own) */
    league_power_on();
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
        native_power_on();
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
    static uint8_t ram[RAM_SIZE];
    ram_pack(ram);                                        /* (RAM as the 68000 has it) */
    golden_ram = fnv(FNV0, ram, 0x10000);
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

/* --input FILE (--input2 to --input4 for pads 2-4): a pad's buttons from a script
 * ("FRAME:BUTTON+BUTTON,..." as tools/oracle/run.py takes, # lines are comments), held from that
 * frame (the handshakes so far) on. In pad 1's, SAVE, LOAD and REWIND are the window's save and load
 * (its menu) and F4, by the handshakes of this process (SAVE and LOAD once, at the handshake they are given) */
#define REQ(r) ((uint32_t)(r) << 16)
typedef struct { long frame; uint32_t buttons; } script_entry;
static struct { script_entry *e; size_t len; } scripts[MDRT_PADS];
static int script_requests;             /* pad 1's script has SAVE, LOAD or REWIND */

static int read_script(const char *path, int pad)
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
    script_entry *script = NULL;
    size_t script_len = 0;
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
            if (b >> 16) {
                if (pad) { fprintf(stderr, "%s: SAVE, LOAD and REWIND are pad 1's\n", path); fclose(f); return 0; }
                script_requests = 1;
            }
        }
    }
    fclose(f);
    scripts[pad].e = script;
    scripts[pad].len = script_len;
    return 1;
}

/* a script's entry at a frame (the handshakes so far: in a state), its length when none */
static size_t script_at(int pad, long frame)
{
    size_t now = scripts[pad].len;
    for (size_t i = 0; i < scripts[pad].len; i++)
        if (scripts[pad].e[i].frame <= frame) now = i;
    return now;
}
static void script_source(void *user, uint16_t b[MDRT_PADS])
{
    long frame = (long)mdrt_frame_count(user);
    for (int pad = 0; pad < MDRT_PADS; pad++) {
        size_t i = script_at(pad, frame);
        b[pad] = i < scripts[pad].len ? (uint16_t)scripts[pad].e[i].buttons : 0;
    }
}
/* the requests by this process's handshakes, not the game's frames (a state loaded puts those
 * back) */
static unsigned script_request(void *user)
{
    (void)user;
    long h = native_handshakes();
    size_t now = script_at(0, h);
    if (now == scripts[0].len) return 0;
    unsigned r = (unsigned)(scripts[0].e[now].buttons >> 16);
    if (scripts[0].e[now].frame != h) r &= MDRT_REQUEST_REWIND;   /* (SAVE and LOAD once) */
    return r;
}

/* netplay's local player, headless: this pad's script, at the frame the session asks in */
static uint16_t script_pad(int pad, int k)
{
    (void)k;
    size_t i = script_at(pad, native_frames());
    return i < scripts[pad].len ? (uint16_t)scripts[pad].e[i].buttons : 0;
}

/* netplay's local player in the window: the window's player k's controls (pad 1's for the first pad
 * this peer plays) */
static uint16_t window_pad(int pad, int k)
{
    (void)pad;
    uint16_t b[MDRT_PADS];
    mdrt_platform_input(native_m, b);
    return b[k];
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
    if (hashes) {
        static uint8_t ram[RAM_SIZE];
        mdrt_vdp_state v;
        mdrt_vdp_get_state(mdrt_get_vdp(native_m), &v);
        ram_pack(ram);
        mdrt_sram *s = mdrt_get_sram(native_m);
        fprintf(hashes, "%ld %016llx %016llx %016llx %016llx\n", frame, (unsigned long long)fnv(FNV0, ram, 0x10000),
                (unsigned long long)fnv(fnv(fnv(fnv(FNV0, v.reg, 24), v.vram, 0x10000), v.cram, sizeof v.cram), v.vsram, sizeof v.vsram),
                (unsigned long long)fnv(FNV0, z80_ram, 0x2000), (unsigned long long)fnv(FNV0, s->bytes, s->size));
    }
    for (int i = 0; i < nwatch; i++) {
        uint16_t v = r16(ram_at(watch[i]), 0);
        if (v != watched[i] || frame == 1) printf("nhl94: frame %ld: $%06X = $%04X\n", frame, (unsigned)watch[i], v);
        watched[i] = v;
    }
    if (shots_dir && frame % shots_every == 0) {
        char path[512];
        snprintf(path, sizeof path, "%s/f%06ld.ppm", shots_dir, frame);
        write_ppm(path);
    }
    if (restart_frame >= 0 && frame >= restart_frame) {   /* --restart-at: back to main, which starts it again */
        restart_frame = -1;
        restarting = 1;
        native_quit();
    }
    if (stop_after >= 0 && frame >= stop_after && !native_net_on()) {   /* (netplay: net.c ends it) */
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
    const char *rom = "NHL 94 (UE) [!].gen", *rom_given = NULL, *sram = "out/nhl94.srm", *league_arg = NULL;
    const char *export_dir = NULL, *check_dir = NULL, *extract_rom = NULL, *extract_dir = NULL, *pack_dir = NULL, *pack_zip = NULL;
    int sram_given = 0;
    int scale = 0, aspect = 0, headless = 0;
    const char *hashes_path = NULL;
    mdrt_net_options netopt;
    char err[256];
    mdrt_net_options_init(&netopt);
    for (int i = 1; i < argc; i++) {
        int net = mdrt_net_parse(&netopt, argc, argv, &i, err, sizeof err);
        if (net < 0) { fprintf(stderr, "%s\n", err); return 2; }
        if (net) continue;
        if (!strcmp(argv[i], "--rom") && i + 1 < argc) rom = rom_given = argv[++i];
        else if (!strcmp(argv[i], "--snap") && i + 1 < argc) { snap = argv[++i]; boot = 0; }
        else if (!strcmp(argv[i], "--boot")) boot = 1;
        else if (!strcmp(argv[i], "--scale") && i + 1 < argc) scale = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--aspect")) aspect = 1;
        else if (!strcmp(argv[i], "--frames") && i + 1 < argc) stop_after = atol(argv[++i]);
        else if (!strcmp(argv[i], "--shot") && i + 1 < argc) shot_path = argv[++i];
        else if (!strcmp(argv[i], "--sram") && i + 1 < argc) { sram = argv[++i]; sram_given = 1; }
        else if (!strcmp(argv[i], "--league") && i + 1 < argc) league_arg = league_folder(argv[++i]);
        else if (!strcmp(argv[i], "--export-league") && i + 1 < argc) export_dir = argv[++i];
        else if (!strcmp(argv[i], "--check-league") && i + 1 < argc) check_dir = league_folder(argv[++i]);
        else if (!strcmp(argv[i], "--extract-rom") && i + 2 < argc) { extract_rom = argv[++i]; extract_dir = argv[++i]; }
        else if (!strcmp(argv[i], "--pack-league") && i + 2 < argc) { pack_dir = league_folder(argv[++i]); pack_zip = argv[++i]; }
        else if (!strcmp(argv[i], "--shots") && i + 1 < argc) shots_dir = argv[++i];
        else if (!strcmp(argv[i], "--every") && i + 1 < argc) shots_every = atol(argv[++i]);
        else if (!strcmp(argv[i], "--wav") && i + 1 < argc) wav_path = argv[++i];
        else if (!strcmp(argv[i], "--headless")) headless = 1;
        else if (!strcmp(argv[i], "--lockstep") && i + 1 < argc) { lockstep_path = argv[++i]; headless = 1; }
        else if (!strcmp(argv[i], "--golden") && i + 1 < argc) { golden_path = argv[++i]; headless = 1; }
        else if (!strcmp(argv[i], "--art-check")) art_check = 1;
        else if (!strcmp(argv[i], "--art-map")) art_check = 2;
        else if (!strcmp(argv[i], "--input") && i + 1 < argc) input_path[0] = argv[++i];
        else if (!strncmp(argv[i], "--input", 7) && argv[i][7] >= '2' && argv[i][7] <= '4' && !argv[i][8] && i + 1 < argc) { int pad = argv[i][7] - '1'; input_path[pad] = argv[++i]; }
        else if (!strcmp(argv[i], "--four-way")) four_way = 1;
        else if (!strcmp(argv[i], "--rule") && i + 1 < argc) {
            const char *why = rules_arg(argv[++i]);
            if (why) { fprintf(stderr, "--rule %s: %s\n", argv[i], why); return 2; }
        }
        else if (!strcmp(argv[i], "--hashes") && i + 1 < argc) hashes_path = argv[++i];
        else if (!strcmp(argv[i], "--state") && i + 1 < argc) state_file = argv[++i];
        else if (!strcmp(argv[i], "--record") && i + 1 < argc) record_dir = argv[++i];
        else if (!strcmp(argv[i], "--watch") && i + 1 < argc) {
            for (char *w = strtok(argv[++i], ","); w && nwatch < 16; w = strtok(NULL, ","))
                watch[nwatch++] = (uint32_t)(strtoul(w, NULL, 16) | 0xFF0000) & ~1u;
        }
        else if (!strcmp(argv[i], "--fm-core") && i + 1 < argc) fm_core = argv[++i];
        else if (!strcmp(argv[i], "--restart-at") && i + 1 < argc) restart_frame = atol(argv[++i]);
        else if (!strcmp(argv[i], "--fm-model") && i + 1 < argc) { fm_model = !strcmp(argv[++i], "ym3438") ? MDRT_FM_YM3438 : MDRT_FM_YM2612; fm_model_given = 1; }
        else if (!strcmp(argv[i], "--load-state") && i + 1 < argc) { load_state_path = argv[++i]; boot = 0; }
        else if (!strcmp(argv[i], "--save-state") && i + 1 < argc && strchr(argv[i + 1], ':')) {
            save_state_frame = atol(argv[++i]);
            save_state_path = strchr(argv[i], ':') + 1;
        }
        else {
            fprintf(stderr, "usage: %s [--rom FILE] [--snap FILE | --boot] [--scale N] [--aspect] [--sram FILE] [--headless]\n"
                            "       [--frames N] [--shot FILE.ppm] [--shots DIR --every N] [--wav FILE] [--input FILE]\n"
                            "       [--state FILE] [--load-state FILE] [--save-state FRAME:FILE] [--lockstep FILE | --golden FILE]\n"
                            "       [--record DIR] [--fm-core nuked|ymfm] [--fm-model ym2612|ym3438] [--watch ADDR,...]\n"
                            "       [--restart-at FRAME] [--four-way] [--input2 FILE] [--input3 FILE] [--input4 FILE]\n"
                            "       [--rule NAME=VALUE ...] (the game rules: native/rules.c) [--league DIR] (league.c)\n"
                            "       [--check-league DIR] [--export-league DIR] [--extract-rom HACK DIR] [--pack-league DIR FILE.zip]\n"
                            "       [--art-check | --art-map] (the art map: art.c) [--hashes FILE]\n"
                            "       %s (netplay: net.c)\n", argv[0], MDRT_NET_USAGE);
            return 2;
        }
    }
    if ((headless || export_dir || check_dir || extract_rom || pack_dir) && !rom_load(rom)) { fprintf(stderr, "cannot load the ROM %s\n", rom); return 1; }   /* (the window: pulseman's menu) */
    if (export_dir || check_dir || extract_rom || pack_dir) {   /* (and nothing else) */
        if (pack_dir && league_pack(pack_dir, pack_zip) < 0) { fprintf(stderr, "--pack-league %s: %s\n", pack_dir, league_error()); return 1; }
        if (extract_rom && extract_league(extract_rom, extract_dir) < 0) return 1;
        if (export_dir && league_export(export_dir) < 0) { fprintf(stderr, "--export-league %s: %s\n", export_dir, league_error()); return 1; }
        if (export_dir) printf("the cartridge's league is in %s: its files are yours to change (README.md, \"Leagues\")\n", export_dir);
        if (check_dir && league_check(check_dir, 1) < 0) { fprintf(stderr, "%s: %s\n", check_dir, league_error()); return 1; }
        return 0;
    }
    if (league_arg) {
        if (!headless && !netopt.on) fprintf(stderr, "--league: the window plays the league its menu's League page names\n");
        else if (league_check(league_arg, 0) < 0) {
            fprintf(stderr, "--league %s: %s\n", league_arg, league_error());
            return 1;
        }
        league_next(league_arg);
        if (!sram_given) sram = league_sram(sram);
    }
    if (sram) make_parent(sram);                          /* (a copy run outside the repository has no out/) */
    if (state_file) make_parent(state_file);
    int inputs = 0;
    for (int pad = 0; pad < MDRT_PADS; pad++) {
        if (!input_path[pad]) continue;
        if (lockstep_path) { fprintf(stderr, "--input: not with --lockstep\n"); return 1; }
        if (!read_script(input_path[pad], pad)) return 1;
        inputs = 1;
    }
    if (snap && load_state_path) { fprintf(stderr, "--snap or --load-state, not both\n"); return 2; }
    if (lockstep_path && !rules_classic()) { fprintf(stderr, "--lockstep: the original is played by the Classic rules only\n"); return 2; }
    if (lockstep_path && !league_next_is_cartridge()) { fprintf(stderr, "--lockstep: the original is played by the cartridge's teams only\n"); return 2; }
    if (netopt.on && (snap || load_state_path || lockstep_path || restart_frame >= 0 || script_requests)) {
        fprintf(stderr, "netplay: from power on, without --snap, --load-state, --lockstep, --restart-at or a script's requests\n");
        return 2;
    }
    if (netopt.on && headless && crc32_of(rom_cart, ROM_SIZE) != NATIVE_ROM_CRC32) { fprintf(stderr, "netplay: %s is not the 1 MB US/Europe cartridge\n", rom); return 1; }
    if (snap) read_snapshot();
    states = !lockstep_path && (!headless || load_state_path || save_state_path || script_requests || netopt.on);   /* (netplay: rollbacks load them) */
    rewind_states = !headless || script_requests;
    mdrt_config c = { 0 };
    c.fm_core = fm_core ? fm_core : fm_model_given ? mdrt_fm_core() : NULL;   /* (NULL: the window's settings) */
    c.fm_model = fm_model;
    c.audio_rate = 48000;
    c.sram_size = 0x2000;
    c.read68k = read68k;
    if (!headless) {
        mdrt_sdl_options o = { .title = "NHL '94", .scale = scale, .aspect = aspect, .sram_path = sram, .app = "ninety-four",
                               .rom_path = rom_given, .rom_default = rom, .rom_hint = "NHL '94: the 1 MB cartridge (US/Europe)",
                               .rom_check = rom_check, .rom_use = rom_use,
                               .options_use = rules_page_use, .options_changed = rules_page_changed, .options_text = rules_page_text,
                               .options_pick = rules_page_pick, .options_detail = rules_page_detail, .sram_file = sram_for };
        sram_base = sram;
        o.options = rules_page(&o.noptions);
        o.pages = rules_pages(&o.npages);
        o.net_identity = native_net_identity("");        /* (the menu's Online page) */
        o.net_settings = window_settings;
        o.options_session = rules_page_session;            /* (the pages in a session: the host's) */
        o.net_pads = 4;                                   /* (up to four players online: 4 Way Play) */
        if (netopt.on) {                                  /* (the window connects, then powers on: by the command line's rules and league) */
            size_t n;
            const void *set = native_net_settings(sram_for(NULL), &n);
            if (!native_net_open(&netopt, "", set, n, window_pad, err, sizeof err)) { fprintf(stderr, "netplay: %s\n", err); return 1; }
            o.net = native_net();
            o.net_udp = native_net_udp();
        }
        int r = mdrt_sdl_run(&c, &o, native_game, NULL);
        native_net_close();
        return r;
    }
    if (wav_path && !(wav = fopen(wav_path, "wb"))) { fprintf(stderr, "cannot write %s\n", wav_path); return 1; }
    if (wav) wav_header(c.audio_rate);
    if (golden_path && !(golden = fopen(golden_path, "w"))) { fprintf(stderr, "cannot write %s\n", golden_path); return 1; }
    if (hashes_path && !(hashes = fopen(hashes_path, "w"))) { fprintf(stderr, "cannot write %s\n", hashes_path); return 1; }
    c.platform = (mdrt_platform){ .present = golden ? golden_present : NULL, .audio = wav_audio };
    if (script_requests) c.platform.requests = script_request;
    mdrt *m = mdrt_new(&c);
    if (!m) { fprintf(stderr, "no FM core %s in this build of pulseman\n", fm_core ? fm_core : "(default)"); return 1; }
    if (lockstep_path && !lockstep_load(lockstep_path, m)) return 1;
    if (golden) mdrt_set_hv_source(m, golden_hv, m);
    if (inputs) mdrt_set_pad_source(m, script_source, m);
    if (four_way) mdrt_pads_set_adaptor(mdrt_get_pads(m), MDRT_ADAPTOR_EA4WAY);
    if (netopt.on) {                                      /* (from power on: the session's pads replace the scripts'; its save RAM the host's) */
        size_t n;
        const void *set = native_net_settings(sram, &n);
        if (!native_net_open(&netopt, "", set, n, script_pad, err, sizeof err)) { fprintf(stderr, "netplay: %s\n", err); return 1; }
        if (!native_net_connect()) return 1;
        mdrt_pads_set_adaptor(mdrt_get_pads(m), native_net_four_way() ? MDRT_ADAPTOR_EA4WAY : MDRT_ADAPTOR_NONE);   /* (the session's pads say: pads 3 or 4, 4 Way Play) */
        native_net_stop_at(stop_after);
    }
    if (lockstep_path || golden || netopt.on) sram = NULL;   /* lockstep, golden: save RAM blank, as the oracle's at power on (netplay: the host's, not written) */
    FILE *f = sram ? fopen(sram, "rb") : NULL;            /* save RAM: as the window's platform keeps it */
    if (f) { if (fread(mdrt_get_sram(m)->bytes, 1, 0x2000, f) != 0x2000) fprintf(stderr, "%s: short\n", sram); fclose(f); }
    native_game(m, NULL);
    while (restarting) {                                  /* --restart-at: the game again on a new console */
        restarting = 0;
        mdrt_free(m);
        if (!(m = mdrt_new(&c))) return 1;
        if (golden) mdrt_set_hv_source(m, golden_hv, m);
        if (inputs) mdrt_set_pad_source(m, script_source, m);
        if (four_way) mdrt_pads_set_adaptor(mdrt_get_pads(m), MDRT_ADAPTOR_EA4WAY);
        native_game(m, NULL);
    }
    native_net_close();
    if (sram && mdrt_get_sram(m)->dirty && (f = fopen(sram, "wb"))) { fwrite(mdrt_get_sram(m)->bytes, 1, 0x2000, f); fclose(f); }
    if (wav) { wav_header(c.audio_rate); fclose(wav); }
    if (golden) fclose(golden);
    if (hashes) fclose(hashes);
    mdrt_free(m);
    if (art_check) {
        printf("art check: %lu bytes taken by DMA from the game's data, %lu in no piece of art (the first at $%06X), %d pieces overlapping\n",
               art_bytes, art_lost, art_first, art_map_overlaps());
        for (int k = 0; k < art_ranges; k++) printf("  in no piece: $%06X-$%06X\n", art_range[k][0], art_range[k][1]);
        art_map_report();
        if (art_check == 2) {                             /* (--art-map: the map, a piece a line) */
            int n;
            const struct art_piece *p = art_map(&n);
            char name[80];
            for (int k = 0; k < n; k++) printf("  $%06X %6u %s\n", p[k].at, p[k].size, art_piece_name(&p[k], name, sizeof name));
        }
    }
    int ls = lockstep_result();                           /* (a run cut short by --frames is not a failure) */
    return ls == 1 || (ls == 2 && stop_after < 0) ? 1 : 0;
}
