/* NHL '94 on Linux: the C game on md-runtime (../md-runtime) in an SDL2 window, with sound. Until
 * the front end is ported it starts from a snapshot of the oracle taken at a faceoff
 * (tools/oracle/snapshot.py: RAM, VDP, Z80 RAM, the sound chips' registers, the Z80's place) and
 * enters the game loop.
 *
 * usage: nhl94 [--rom FILE] [--snap FILE] [--scale N] [--aspect] [--sram FILE]
 *              [--headless] [--frames N] [--shot FILE.ppm] [--shots DIR --every N] [--wav FILE]
 *
 * --headless runs without a window or pacing (as fast as it goes): with --frames it stops, --shot
 * writes the last picture, --wav the sound. Keys: md-runtime's (include/mdrt/platform_sdl2.h):
 * pad 1 arrows, Z / X / C = A / B / C, Enter = Start; pad 2 I J K L, Q / W / E, Tab; two game
 * controllers; Esc quits, P pauses, F11 full screen. */
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "game.h"
#include "rom.h"
#include "z80.h"
#include "native.h"
#include "mdrt/platform_sdl2.h"

static const char *snap = "out/states/faceoff.snap", *shot_path, *shots_dir, *wav_path;
static long stop_after = -1, shots_every = 600;
static jmp_buf out;

/* ---- the snapshot --------------------------------------------------------------------------- */

#define SNAP_BASE (8 + 0x10000 + 0x10000 + 128 + 80 + 32 + 0x2000)
static uint8_t snapshot[SNAP_BASE + 4 + 512 + 17 + 8];
static size_t snapshot_len;

static void read_snapshot(void)
{
    FILE *f = fopen(snap, "rb");
    if (!f) { fprintf(stderr, "cannot open %s (make one with tools/oracle/snapshot.py)\n", snap); exit(1); }
    snapshot_len = fread(snapshot, 1, sizeof snapshot, f);
    fclose(f);
    if (snapshot_len < SNAP_BASE || memcmp(snapshot, "NHLS", 4)) { fprintf(stderr, "%s: not a snapshot\n", snap); exit(1); }
}

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
    native_hal_start(m, 0);
    int chips = snapshot_len >= sizeof snapshot && (p[0] & 1);
    if (!chips) fprintf(stderr, "%s: no sound chip registers (a snapshot from a saved state): the FM sounds start blank\n", snap);
    if (chips) {
        const uint8_t *ym = p + 4, *psg = p + 4 + 512, *zs = p + 4 + 512 + 17;
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
        if (zs[7] & 1) {
            z80_relay_seed((uint16_t)(zs[0] | zs[1] << 8), (uint16_t)(zs[2] | zs[3] << 8), zs[4], zs[5]);
            z80.bus_taken = (zs[7] & 2) ? 1 : 0;
        }
    }
}

/* ---- the session ------------------------------------------------------------------------------ */

static uint8_t read68k(void *user, uint32_t addr) { (void)user; return *mem_ptr(addr); }

void native_quit(void) { longjmp(out, 1); }

void native_game(mdrt *m, void *user)
{
    (void)user;
    native_m = m;
    if (setjmp(out)) return;
    load_snapshot(m);
    run68k(Gameloop);
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

void native_frame_done(long frame)
{
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
static void wav_audio(void *user, const int16_t *s, int n)
{
    (void)user;
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

int main(int argc, char **argv)
{
    const char *rom = "NHL 94 (UE) [!].gen", *sram = "out/nhl94.srm";
    int scale = 3, aspect = 0, headless = 0;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--rom") && i + 1 < argc) rom = argv[++i];
        else if (!strcmp(argv[i], "--snap") && i + 1 < argc) snap = argv[++i];
        else if (!strcmp(argv[i], "--scale") && i + 1 < argc) scale = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--aspect")) aspect = 1;
        else if (!strcmp(argv[i], "--frames") && i + 1 < argc) stop_after = atol(argv[++i]);
        else if (!strcmp(argv[i], "--shot") && i + 1 < argc) shot_path = argv[++i];
        else if (!strcmp(argv[i], "--sram") && i + 1 < argc) sram = argv[++i];
        else if (!strcmp(argv[i], "--shots") && i + 1 < argc) shots_dir = argv[++i];
        else if (!strcmp(argv[i], "--every") && i + 1 < argc) shots_every = atol(argv[++i]);
        else if (!strcmp(argv[i], "--wav") && i + 1 < argc) wav_path = argv[++i];
        else if (!strcmp(argv[i], "--headless")) headless = 1;
        else {
            fprintf(stderr, "usage: %s [--rom FILE] [--snap FILE] [--scale N] [--aspect] [--sram FILE] [--headless]\n"
                            "       [--frames N] [--shot FILE.ppm] [--shots DIR --every N] [--wav FILE]\n", argv[0]);
            return 2;
        }
    }
    if (!rom_load(rom)) { fprintf(stderr, "cannot load the ROM %s\n", rom); return 1; }
    read_snapshot();
    mdrt_config c = { 0 };
    c.audio_rate = 48000;
    c.sram_size = 0x2000;
    c.read68k = read68k;
    if (!headless) {
        mdrt_sdl_options o = { "NHL '94", scale, aspect, sram };
        return mdrt_sdl_run(&c, &o, native_game, NULL);
    }
    if (wav_path && !(wav = fopen(wav_path, "wb"))) { fprintf(stderr, "cannot write %s\n", wav_path); return 1; }
    if (wav) wav_header(c.audio_rate);
    c.platform = (mdrt_platform){ NULL, NULL, wav_audio, NULL, NULL };
    mdrt *m = mdrt_new(&c);
    FILE *f = fopen(sram, "rb");                          /* save RAM: as the window's platform keeps it */
    if (f) { if (fread(mdrt_get_sram(m)->bytes, 1, 0x2000, f) != 0x2000) fprintf(stderr, "%s: short\n", sram); fclose(f); }
    native_game(m, NULL);
    if (mdrt_get_sram(m)->dirty && (f = fopen(sram, "wb"))) { fwrite(mdrt_get_sram(m)->bytes, 1, 0x2000, f); fclose(f); }
    if (wav) { wav_header(c.audio_rate); fclose(wav); }
    mdrt_free(m);
    return 0;
}
