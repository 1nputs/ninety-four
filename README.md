# NHL '94, rebuilt in C 🏒

![Blake scores, assisted by Gretzky: NHL '94 running natively](docs/media/nhl94-scoring.gif)

**The greatest hockey game ever made, running natively on your machine, with no emulator in
sight.** All of the roughly 1,500 routines of EA's 1993 Sega Genesis classic have been rewritten
by hand in C and checked against the original, frame by frame, until the two can't be told
apart. The same one-timers, the same diving saves, the same organ, the same Gretzky. Smooth 60
frames a second, without the cartridge's slowdown.

And things the cartridge never had: **save in the middle of a game (F5), load it back (F9), and
hold Backspace to rewind** the goal you just gave up.

Bring your own cartridge dump and you're on the ice in a minute:

```
make nhl94                                  # needs gcc/g++, make, SDL2 and ../md-runtime beside this repo
./build/nhl94 --rom "/path/to/NHL 94.gen"   # the 1 MB retail ROM (US/Europe)
```

| | Player 1 | Player 2 |
|---|---|---|
| D-pad | arrows | I J K L |
| A / B / C | Z / X / C | Q / W / E |
| Start | Enter | Tab |

Game controllers work too (the first two are players 1 and 2). F5 saves, F9 loads, Backspace
rewinds, P pauses, F11 is full screen, Esc quits.

## For agents: where this stands (16 October 2026)

- **What it is.** A hand-written C reimplementation of NHL '94 (Genesis, US/Europe retail, SHA-1
  `7b1489ab501258133bf03e5f0892b77d04312580`). `src/` has one C file per asm module of the
  community's byte-exact rebuild (34 modules, about 1,500 routines, the same names). The Z80's
  661-byte sound relay is ported too (`src/z80.c`). It runs on **md-runtime** (`../md-runtime`,
  MIT): the VDP, PSG, YM2612, pads and save RAM as C modules, with an SDL2 platform. There is no
  68k or Z80 emulation.
- **How it is verified.** Genesis Plus GX with a hook (the oracle, a development tool only)
  records real play. `make test` replays 10,197 recorded routine calls. `make lockstep` runs 17
  whole sessions against the oracle, checking work RAM and VDP at every vblank. `make
  golden-check` compares the native build's own output with a recorded build, bit for bit
  (88,000 frames), plus save-state round trips. md-runtime is checked on the oracle's vectors
  (`make runtime-check`) and on this game's own recordings (`make runtime-selfcheck`).
- **Done.** The harness, the game-play core, video and sound, the frame drivers, native running,
  lockstep, the whole front end (it boots from power on), a cleanup, golden runs, native save
  states and rewind. In md-runtime: the console's state as a value, a client-owned clock with
  line-by-line drawing and an H-interrupt hook, a bus map, FM cores and models chosen at run
  time, and a trace recorder.
- **Deferred by the owner.** WebAssembly (it first needs `src/ram.h`'s GCC-only big-endian
  struct replaced: the choice is part of that job) and the cartridge's slowdown (it needs a 68k
  cost model).
- **Read first.** `docs/HANDOFF.md` (the living guide: setup, recipe, status, quirks, next
  steps), `docs/PLAN.md`, `../md-runtime/README.md`, and `../md-recomp-playbook/` (the method
  for another game; the runtime's directions; netplay, mods and scripting; quality of life).
- **Rules.** The ROM and everything made from it (states, traces, captures, screenshots, this
  README's GIF: `tools/readme_gif.py`) stay out of git. Commit after each finished job.
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
- [x] Save states (F5 / F9) and rewind (Backspace)
- [x] FM core and chip model choice (`--fm-core nuked|ymfm`, `--fm-model ym2612|ym3438`)
- [x] Headless runs: screenshots, WAV, scripted input, RAM watch (`--watch`), trace recording (`--record`)
- [ ] Mute, volume, audio device and latency options
- [ ] A settings file (window, scaling, volume, keys)
- [ ] Integer and sharp-bilinear scaling, hi-DPI
- [ ] Controller remapping: profiles (B/C swap, pads labelled A B C), player slots, turbo, dead zones
- [ ] Screenshot key; video and audio capture
- [ ] Fast-forward, slow motion, frame step
- [ ] Save slots with thumbnails; quick resume on quit and start
- [ ] Skip the EA screen, boot straight into a game
- [ ] Colour options; scanlines, CRT mask, composite dither blending
- [ ] IPS and BPS patches, soft-patching, the ROM check as a toggle for data hacks
- [ ] Game Genie and Action Replay codes
- [ ] Input recording and replay files
- [ ] Save states anywhere, menus and pauses included
- [ ] Debug overlay: VRAM, palettes, sprites, a RAM watch and cheat search
- [ ] Run-ahead for lower input latency
- [ ] The cartridge's slowdown as an option, with an "overclock" slider
- [ ] Four players with EA's 4 Way Play adaptor
- [ ] Play in the browser (WebAssembly)
- [ ] A per-game manifest: named RAM, roster and ratings tables to CSV and back, a hack diff tool
- [ ] HD sprite, tile and sound packs
- [ ] Widescreen
- [ ] Lua scripting: overlays, stats, bots
- [ ] Python bindings: headless training environments, automated tests
- [ ] Replays and spectating
- [ ] Rollback netplay online
- [ ] The next game on the same method and runtime
