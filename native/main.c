/* NHL '94 on Linux: the C game in an SDL2 window. Until the front end is ported it starts from a
 * snapshot of the oracle (RAM, VRAM, CRAM, VSRAM, VDP registers, Z80 RAM; tools/oracle/snapshot.py)
 * taken during play, and enters the game loop.
 *
 * usage: nhl94 [--rom FILE] [--snap FILE] [--scale N] [--frames N --shot FILE.ppm] [--fast]
 *              [--headless] [--shots DIR --every N] [--sram FILE]
 *
 * Pad 1: arrows, Z / X / C = A / B / C, Enter = Start. Pad 2: I J K L, Q / W / E, Tab.
 * Game controllers: the first two are pads 1 and 2 (X / A / B = A / B / C). Esc quits. */
#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "game.h"
#include "rom.h"
#include "vdp.h"
#include "native.h"

static SDL_Window *win;
static SDL_Renderer *ren;
static SDL_Texture *tex;
static SDL_GameController *pads[2];
static uint32_t pixels[320 * 224];
static long frame, stop_after = -1;
static const char *shot_path, *shots_dir, *sram_path = "out/nhl94.srm";
static long shots_every = 600;
static int fast, headless;
static uint64_t next_tick;

static uint16_t bus_read16(uint32_t a)
{
    const uint8_t *p = mem_ptr(a & 0xFFFFFE);
    return (uint16_t)(p[0] << 8 | p[1]);
}

static void load_snapshot(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "cannot open %s (make one with tools/oracle/snapshot.py)\n", path); exit(1); }
    char magic[8];
    uint8_t cram[128], vsram[80];
    int ok = fread(magic, 1, 8, f) == 8 && memcmp(magic, "NHLS", 4) == 0 &&
             fread(ram_ptr(RAM_BASE), 1, 0x10000, f) == 0x10000 && fread(vdp.vram, 1, 0x10000, f) == 0x10000 &&
             fread(cram, 1, 128, f) == 128 && fread(vsram, 1, 80, f) == 80 &&
             fread(vdp.reg, 1, 32, f) == 32 && fread(zram, 1, 0x2000, f) == 0x2000;
    fclose(f);
    if (!ok) { fprintf(stderr, "%s: not a snapshot\n", path); exit(1); }
    for (int i = 0; i < 64; i++) vdp.cram[i] = (uint16_t)(cram[2 * i] << 8 | cram[2 * i + 1]);
    for (int i = 0; i < 40; i++) vdp.vsram[i] = (uint16_t)(vsram[2 * i] << 8 | vsram[2 * i + 1]);
}

static void write_ppm(const char *path, int width)
{
    FILE *f = fopen(path, "wb");
    if (!f) return;
    fprintf(f, "P6\n%d 224\n255\n", width);
    for (int y = 0; y < 224; y++)
        for (int x = 0; x < width; x++) {
            uint32_t c = pixels[y * 320 + x];
            uint8_t rgb[3] = { (uint8_t)(c >> 16), (uint8_t)(c >> 8), (uint8_t)c };
            fwrite(rgb, 1, 3, f);
        }
    fclose(f);
}

static void quit(int code)
{
    native_sram_save(sram_path);
    if (!headless) SDL_Quit();
    exit(code);
}

/* the pads from the keyboard and the game controllers */
static void read_pads(void)
{
    const uint8_t *k = SDL_GetKeyboardState(NULL);
    static const SDL_Scancode keys[2][8] = {
        { SDL_SCANCODE_UP, SDL_SCANCODE_DOWN, SDL_SCANCODE_LEFT, SDL_SCANCODE_RIGHT, SDL_SCANCODE_Z, SDL_SCANCODE_X, SDL_SCANCODE_C, SDL_SCANCODE_RETURN },
        { SDL_SCANCODE_I, SDL_SCANCODE_K, SDL_SCANCODE_J, SDL_SCANCODE_L, SDL_SCANCODE_Q, SDL_SCANCODE_W, SDL_SCANCODE_E, SDL_SCANCODE_TAB },
    };
    static const SDL_GameControllerButton buttons[8] = {
        SDL_CONTROLLER_BUTTON_DPAD_UP, SDL_CONTROLLER_BUTTON_DPAD_DOWN, SDL_CONTROLLER_BUTTON_DPAD_LEFT, SDL_CONTROLLER_BUTTON_DPAD_RIGHT,
        SDL_CONTROLLER_BUTTON_X, SDL_CONTROLLER_BUTTON_A, SDL_CONTROLLER_BUTTON_B, SDL_CONTROLLER_BUTTON_START,
    };
    for (int p = 0; p < 2; p++) {
        uint16_t b = 0;
        for (int i = 0; i < 8; i++) {
            if (k[keys[p][i]]) b |= (uint16_t)(1 << i);
            if (pads[p] && SDL_GameControllerGetButton(pads[p], buttons[i])) b |= (uint16_t)(1 << i);
        }
        if (pads[p]) {
            int x = SDL_GameControllerGetAxis(pads[p], SDL_CONTROLLER_AXIS_LEFTX), y = SDL_GameControllerGetAxis(pads[p], SDL_CONTROLLER_AXIS_LEFTY);
            if (y < -16000) b |= NATIVE_UP;
            if (y > 16000) b |= NATIVE_DOWN;
            if (x < -16000) b |= NATIVE_LEFT;
            if (x > 16000) b |= NATIVE_RIGHT;
        }
        native_pads[p] = b;
    }
}

void native_frame(void)
{
    int width = vdp_render(pixels);
    frame++;
    if (!headless) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT || (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE)) quit(0);
            if (e.type == SDL_CONTROLLERDEVICEADDED) {
                for (int p = 0; p < 2; p++) if (!pads[p]) { pads[p] = SDL_GameControllerOpen(e.cdevice.which); break; }
            }
        }
        read_pads();
        SDL_Rect src = { 0, 0, width, 224 };
        SDL_UpdateTexture(tex, NULL, pixels, 320 * 4);
        SDL_RenderClear(ren);
        SDL_RenderCopy(ren, tex, &src, NULL);
        SDL_RenderPresent(ren);
        if (!fast) {
            uint64_t freq = SDL_GetPerformanceFrequency(), now = SDL_GetPerformanceCounter();
            uint64_t period = freq * 1001 / 60000;        /* 59.94 frames a second */
            if (!next_tick || now > next_tick + period * 4) next_tick = now;
            next_tick += period;
            while ((now = SDL_GetPerformanceCounter()) < next_tick) {
                uint64_t ms = (next_tick - now) * 1000 / freq;
                if (ms > 1) SDL_Delay((uint32_t)(ms - 1));
            }
        }
    }
    if (frame % 600 == 0) native_sram_save(sram_path);
    if (shots_dir && frame % shots_every == 0) {
        char path[512];
        snprintf(path, sizeof path, "%s/f%06ld.ppm", shots_dir, frame);
        write_ppm(path, width);
    }
    if (stop_after >= 0 && frame >= stop_after) {
        if (shot_path) write_ppm(shot_path, width);
        quit(0);
    }
    if (vdp.reg[1] & 0x20) call68k(g.vbint);              /* the vblank interrupt */
}

int main(int argc, char **argv)
{
    const char *rom = "NHL 94 (UE) [!].gen", *snap = "out/states/faceoff.snap";
    int scale = 3;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--rom") && i + 1 < argc) rom = argv[++i];
        else if (!strcmp(argv[i], "--snap") && i + 1 < argc) snap = argv[++i];
        else if (!strcmp(argv[i], "--scale") && i + 1 < argc) scale = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--frames") && i + 1 < argc) stop_after = atol(argv[++i]);
        else if (!strcmp(argv[i], "--shot") && i + 1 < argc) shot_path = argv[++i];
        else if (!strcmp(argv[i], "--sram") && i + 1 < argc) sram_path = argv[++i];
        else if (!strcmp(argv[i], "--shots") && i + 1 < argc) shots_dir = argv[++i];
        else if (!strcmp(argv[i], "--every") && i + 1 < argc) shots_every = atol(argv[++i]);
        else if (!strcmp(argv[i], "--fast")) fast = 1;
        else if (!strcmp(argv[i], "--headless")) headless = 1;
        else { fprintf(stderr, "usage: %s [--rom FILE] [--snap FILE] [--scale N] [--frames N --shot FILE.ppm] [--fast] [--headless]\n", argv[0]); return 2; }
    }
    if (!rom_load(rom)) { fprintf(stderr, "cannot load the ROM %s\n", rom); return 1; }
    vdp_reset();
    vdp_bus_read16 = bus_read16;
    load_snapshot(snap);
    native_sram_load(sram_path);
    if (!headless) {
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) != 0) { fprintf(stderr, "SDL: %s\n", SDL_GetError()); return 1; }
        win = SDL_CreateWindow("NHL '94", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 320 * scale, 224 * scale, SDL_WINDOW_RESIZABLE);
        ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
        if (!ren) ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE);   /* no GPU: draw in software */
        if (!win || !ren) { fprintf(stderr, "SDL: %s\n", SDL_GetError()); return 1; }
        SDL_RenderSetLogicalSize(ren, 320 * scale, 224 * scale);
        tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, 320, 224);
        for (int i = 0, p = 0; i < SDL_NumJoysticks() && p < 2; i++)
            if (SDL_IsGameController(i)) pads[p++] = SDL_GameControllerOpen(i);
    }
    run68k(Gameloop);
    return 0;
}
