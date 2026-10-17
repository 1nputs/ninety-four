# ninety-four 🏒

*NHL '94, rebuilt in C.*

**The greatest hockey game ever made, running natively on your machine, with no emulator in
sight.** All of the roughly 1,500 routines of EA's 1993 Sega Genesis classic have been rewritten
by hand in C and checked against the original, frame by frame, until the two can't be told
apart. The same one-timers, the same diving saves, the same organ, the same Gretzky. Smooth 60
frames a second, without the cartridge's slowdown.

And things the cartridge never had: **save in the middle of a game (F9), load it back (F10), and
hold F4 to rewind** the goal you just gave up. Hold F6 to fast-forward, slow the replay of that
one-timer down, step through it a frame at a time, and press F12 to keep it. Scored a beauty?
**Shift+F12 saves the last 20 seconds** as a GIF or an MP4 to share.

Bring your own cartridge dump and you're on the ice in a minute (what to install first is
under [Build](#build)):

```
git clone --recursive https://github.com/1nputs/ninety-four.git && cd ninety-four
make nhl94
./build/nhl94          # the first time, it asks for your ROM: the 1 MB retail cartridge (US/Europe)
```

The first time, pick your ROM in the game's file browser (or give it with `--rom FILE`); it is
remembered after that.

| | Player 1 | Player 2 |
|---|---|---|
| D-pad | arrows | I J K L |
| A / B / C | Z / X / C | Q / W / E |
| Start | Enter | Tab |

Game controllers work too (the first two are players 1 and 2).

| Key | |
|---|---|
| F1 | mute |
| F2 / F3 | volume down / up |
| F4 (held) | rewind |
| F5 | pause |
| F6 (held) | fast-forward |
| F7 | slow motion on / off |
| F8 | a frame at a time, while paused |
| F9 / F10 | save / load a state (the slot chosen last) |
| F11 or Alt+Enter | full screen |
| F12 | screenshot (a PNG in `screenshots/`) |
| Shift+F12 | save the replay: the last 20 seconds (in `videos/`) |
| Ctrl+F12 | start / stop recording a video |
| Esc | the menu |

**The menu** (Esc, or a controller's Back button; arrows and Enter, or the d-pad, A and B) has the
ROM, a reset, states, the video (a pixel-art upscaler: Scale2x, Scale3x or MMPX, which smooth the
staircases without blurring or adding colours; sharp, integer or fit scaling; square pixels or
4:3; the colours' levels, standard, full range or the console's own; dithering blended as a
television blended it; a picture tube's scanlines and its aperture grille, slot mask or shadow
mask; full screen), captures (below), the sound (volume, and the FM sound core and chip: hear the
difference as you play), players (see below), options, and every key of the pads and the
controls: pick one and press the new key (with Shift, Ctrl or Alt if you like).

**Captures:** the last 20 seconds are always kept; Shift+F12 (or the menu's Capture page) saves
them, and Ctrl+F12 records until you press it again. They go to `videos/` as a GIF (exact
colours, 30 frames a second, no sound) or an MP4 with the sound, made by
[ffmpeg](https://ffmpeg.org) if it is installed (otherwise a GIF). The Capture page picks the
format, how long the replay is, and a GIF's size.

**Save slots and quick resume:** the menu's Save and Load pages have eight slots, each with a
picture of the moment, in `states/` beside the game. States are taken during play (not in the menus
or a pause). Quit in the middle of a game and the next start puts you back where you were (an
option in the menu).

**Four players** (EA's 4 Way Play, as on the cartridge): in the menu, Players, set Controllers to
EA 4 Way Play and Reset; then choose Players: Three or Four in the game's setup screen. The
Players page also gives each player a controller, moves a controller's buttons (press the new
one), adds turbo A / B / C, and sets the stick's dead zone.

All of it is kept in `settings.ini`, in the folder you run the game from, beside its saves (you
can also edit it while the game is closed). The game pauses when its window loses focus (an
option in the menu). On a Mac, hold Fn for the function keys.

## Build

The hardware side is [pulseman](https://github.com/1nputs/pulseman), our Mega Drive runtime,
included as a git submodule in `ext/pulseman`: clone with `--recursive`, or after a plain clone
run `git submodule update --init`. `make nhl94` builds it too. Besides that you need GCC (C and
C++), `make`, `pkg-config` and SDL2. The game itself must be built with GCC for now: its RAM
layout uses a GCC-only attribute until the planned WebAssembly work replaces it. Your saves and
states go to `out/` in this repository.

**Linux** (built and tested here, Debian 13, GCC 14):

```
sudo apt install build-essential pkg-config libsdl2-dev          # Debian, Ubuntu
sudo dnf install gcc gcc-c++ make pkgconf-pkg-config SDL2-devel  # Fedora
sudo pacman -S --needed base-devel sdl2                          # Arch
make nhl94
./build/nhl94 --rom "/path/to/NHL 94.gen"
```

**macOS** (not tried yet). Apple's `gcc` is really Clang, so install GCC from
[Homebrew](https://brew.sh) and name it:

```
brew install gcc sdl2 pkg-config
make CC=gcc-15 CXX=g++-15 nhl94          # the version brew installed: ls $(brew --prefix)/bin/gcc-*
./build/nhl94 --rom "/path/to/NHL 94.gen"
```

**Windows** (cross-built here with MinGW-w64 GCC 14 without a warning, not yet run on Windows),
with [MSYS2](https://www.msys2.org), in its *UCRT64* shell:

```
pacman -S --needed git make mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-pkgconf mingw-w64-ucrt-x86_64-SDL2
make CC=gcc CXX=g++ nhl94
./build/nhl94.exe --rom "C:/path/to/NHL 94.gen"
```

The C++ runtime is linked in, so outside the MSYS2 shell `build/nhl94.exe` needs only
`SDL2.dll` beside it (`cp /ucrt64/bin/SDL2.dll build/`).

If a compiler newer than these warns about something, `make WERROR= nhl94` builds anyway; please
tell us what it said.

## For agents: where this stands (16 October 2026)

- **What it is.** A hand-written C reimplementation of NHL '94 (Genesis, US/Europe retail, SHA-1
  `7b1489ab501258133bf03e5f0892b77d04312580`). `src/` has one C file per asm module of the
  community's byte-exact rebuild (34 modules, about 1,500 routines, the same names). The Z80's
  661-byte sound relay is ported too (`src/z80.c`). It runs on **pulseman**
  ([1nputs/pulseman](https://github.com/1nputs/pulseman), MIT, the git submodule `ext/pulseman`;
  its C API keeps the `mdrt_` prefix): the VDP, PSG, YM2612, pads and save RAM as C modules,
  with an SDL2 platform. There is no 68k or Z80 emulation. A change to pulseman is committed and
  pushed there, then the submodule's new commit is committed here.
- **How it is verified.** Genesis Plus GX with a hook (the oracle, a development tool only)
  records real play. `make test` replays 10,197 recorded routine calls. `make lockstep` runs 17
  whole sessions against the oracle, checking work RAM and VDP at every vblank. `make
  golden-check` compares the native build's own output with a recorded build, bit for bit
  (88,000 frames), plus save-state round trips. pulseman is checked on the oracle's vectors
  (`make runtime-check`) and on this game's own recordings (`make runtime-selfcheck`).
- **Done.** The harness, the game-play core, video and sound, the frame drivers, native running,
  lockstep, the whole front end (it boots from power on), a cleanup, golden runs, native save
  states and rewind. In pulseman: the console's state as a value, a client-owned clock with
  line-by-line drawing and an H-interrupt hook, a bus map, FM cores and models chosen at run
  time, and a trace recorder.
- **Deferred by the owner.** WebAssembly (it first needs `src/ram.h`'s GCC-only big-endian
  struct replaced: the choice is part of that job) and the cartridge's slowdown (it needs a 68k
  cost model).
- **Read first.** `ext/pulseman/README.md` and `docs/API.md` there; the comments at the top
  of `tools/oracle/vectors.py`, `tools/golden.py` and `tools/hybrid/oracle_shim.c`. The working
  notes (the handoff guide, the plan, the playbook for another game) are kept outside the
  repository.
- **Rules.** The ROM and everything made from it (states, traces, captures, screenshots, GIFs
  such as `tools/goal_gif.py` makes) stay out of git. Commit after each finished job.
  Before and after a change: `make test`, `make lockstep`, `make golden-check`; for a runtime
  change, also `make runtime-check` and `make runtime-selfcheck`.

```
./tools/fetch_refs.sh && tools/asm/build_vasm.sh     # references, the oracle core, the byte-exact rebuild
cp /path/to/rom "NHL 94 (UE) [!].gen"                # your ROM at the repo root
make oracle && make vectors                          # the oracle; every state and vector (tools/oracle/vectors.py)
tools/hybrid/capture_all.sh                          # the 29 capture sessions (~15 min, 3.6 GB)
make test && make lockstep && make golden-check      # the checks
```

## Features

From the least ambitious to the most.

- [x] The whole game from power on: exhibition, playoffs, shootouts, user records, every menu and screen
- [x] FM and PSG music and effects, the Z80 sound relay ported to C
- [x] Keyboard and two game controllers
- [x] Window scaling, full screen, 4:3 aspect
- [x] Battery save (save RAM) kept in a file
- [x] Smooth 60 frames a second (no cartridge slowdown)
- [x] Save states (F9 / F10) and rewind (F4)
- [x] FM core and chip model choice (`--fm-core nuked|ymfm`, `--fm-model ym2612|ym3438`)
- [x] Headless runs: screenshots, WAV, scripted input, RAM watch (`--watch`), trace recording (`--record`)
- [x] Mute, volume, audio device and latency options
- [x] A settings file (window, scaling, volume, keys), every key rebindable
- [x] A menu: the ROM in a file browser, reset, video, sound and the FM chip live, key rebinding
- [x] Integer and sharp-bilinear scaling, hi-DPI
- [x] Pixel-art upscalers: Scale2x, Scale3x, MMPX 2x and 4x
- [x] Screenshot key (PNG)
- [x] Fast-forward, slow motion, frame step
- [x] Pause when the window loses focus
- [x] Controller remapping, player slots, turbo, dead zones
- [x] Video and audio capture: the replay (the last 20 seconds) and recordings, GIF or MP4
- [x] Save slots with thumbnails; quick resume on quit and start (states during play)
- [ ] Skip the EA screen, boot straight into a game
- [x] Colour options; scanlines, CRT mask, composite dither blending
- [ ] IPS and BPS patches, soft-patching, the ROM check as a toggle for data hacks
- [ ] Game Genie and Action Replay codes
- [ ] Input recording and replay files
- [ ] Save states anywhere, menus and pauses included
- [ ] Debug overlay: VRAM, palettes, sprites, a RAM watch and cheat search
- [ ] Run-ahead for lower input latency
- [ ] The cartridge's slowdown as an option, with an "overclock" slider
- [x] Four players with EA's 4 Way Play adaptor
- [ ] Play in the browser (WebAssembly)
- [ ] A per-game manifest: named RAM, roster and ratings tables to CSV and back, a hack diff tool
- [ ] HD sprite, tile and sound packs
- [ ] Widescreen
- [ ] Lua scripting: overlays, stats, bots
- [ ] Python bindings: headless training environments, automated tests
- [ ] Replays and spectating
- [ ] Rollback netplay online
- [ ] The next game on the same method and runtime

## Greets

Big, big greets to everyone whose work this stands on:

- **EA Sports and Park Place Productions**, who made NHL '94 in 1993. Still the best.
- **[abdulahmad/NHL94Genesis](https://github.com/abdulahmad/NHL94Genesis)**: the bitwise rebuild
  of the ROM in assembly, with every label and the module split, built in the style of its
  sibling [NHLPA93Genesis](https://github.com/abdulahmad/NHLPA93Genesis). Every C file here maps
  to one of its files, and every routine has its name.
- **[Chaos81/nhl94-disassembly](https://github.com/Chaos81/nhl94-disassembly)**: the IDA
  database and listing with the names and comments that made the code readable, and
  **[Mhopkinsinc/NHLHockey](https://github.com/Mhopkinsinc/NHLHockey)**, the NHL Hockey source
  code it started from.
- **The NHL '94 community** and its [Discord](https://discord.gg/KXJeQ6pyUc), keeping the game
  alive for over thirty years.
- **[Genesis Plus GX](https://github.com/ekeeke/Genesis-Plus-GX)** by Eke-Eke (after Charles
  MacDonald's Genesis Plus): the oracle. Every routine and every frame here was checked against
  it. A development tool only, never shipped.
- **[Nuked-OPN2](https://github.com/nukeykt/Nuked-OPN2)** by nukeykt and
  **[ymfm](https://github.com/aaronsgiles/ymfm)** by Aaron Giles: the YM2612 cores that make the
  music.
- **[vasm](http://sun.hasenbraten.de/vasm/)** by Volker Barthelmann and Frank Wille: the assembler
  that rebuilds the ROM byte for byte on Linux.
- **[SDL](https://www.libsdl.org)**: the window, the sound and the controllers.
- **MMPX** by Morgan McGuire and Mara Gagiu ("MMPX Style-Preserving Pixel Art Magnification",
  JCGT 2021) and **Scale2x / Scale3x** by Andrea Mazzoleni after Eric Johnston's EPX: the
  upscalers in the Video menu.
- **[libretro](https://www.libretro.com)**: the API the oracle is hosted through.
- The documenters of the hardware: Charles MacDonald (`genvdp.txt`),
  [Plutiedev](https://plutiedev.com), [Sega Retro](https://segaretro.org),
  [SMS Power](https://www.smspower.org) (the SN76489) and the
  [SpritesMind](https://gendev.spritesmind.net) forum (Nemesis's YM2612 and VDP research).
- **[Zig](https://ziglang.org)**, whose bundled Clang checks pulseman against a second
  compiler.

## License

MIT (`LICENSE`), for the code written for this project. It grants nothing over NHL '94 itself,
its name or its data, which belong to Electronic Arts: bring your own copy of the game.
pulseman is MIT too; its FM chip cores are Nuked-OPN2 (LGPL-2.1) and ymfm (BSD-3), and its MMPX
upscaler follows the authors' MIT-licensed code.
