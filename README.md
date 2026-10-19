# ninety-four 🏒

*NHL '94, rebuilt in C.*

**The greatest hockey game ever made, running natively on your machine, with no emulator in
sight.** All of the roughly 1,500 routines of EA's 1993 Sega Genesis classic have been rewritten
by hand in C and checked against the original, frame by frame, until the two can't be told
apart. The same one-timers, the same diving saves, the same organ, the same Gretzky. Smooth 60
frames a second, without the cartridge's slowdown.

And things the cartridge never had: **save in the middle of a game and load it back (eight slots
in the menu), and hold F4 to rewind** the goal you just gave up. Hold F6 to fast-forward, slow the
replay of that one-timer down, step through it a frame at a time, and press F12 to keep it.
Scored a beauty? **F10 saves the last 20 seconds** as a GIF to share.

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
| F10 | save the replay: the last 20 seconds, as a GIF (in `videos/`) |
| F11 | start / stop recording a GIF |
| F12 | screenshot (a PNG in `screenshots/`) |
| Esc | the menu: save and load states there |
| Alt+Enter | full screen |

## Playing

### The menu

Esc (or a controller's Back button) opens it; arrows and Enter, or the d-pad, A and B, work it.

- **Game ROM**: your cartridge dump, in a file browser.
- **Reset**, and **Save a state** / **Load a state**: eight slots, each with a picture of the
  moment, in `states/` beside the game. States are taken during play (not in the menus or a
  pause). Quit in the middle of a game and the next start puts you back where you were (quick
  resume, an option).
- **Video**: a pixel-art upscaler (Scale2x, Scale3x or MMPX, which smooth the staircases without
  blurring or adding colours, or xBR, which blends its edges smooth); sharp, integer or fit
  scaling; square pixels or 4:3; the colours' levels (standard, full range or the console's own);
  dithering blended as a television blended it; a picture tube's scanlines and its aperture
  grille, slot mask or shadow mask; full screen; and **Vsync**, so nothing tears (on unless you
  turn it off).
- **Capture**: the last 20 seconds are always kept; F10 (or this page) saves them, and F11
  records until you press it again. They go to `videos/` as GIFs (exact colours, 30 frames a
  second, no sound). The page sets how long the replay is and a GIF's size.
- **Sound**: volume, and the FM sound core and chip (hear the difference as you play).
- **Players**: which controller each player has, a controller's buttons (press the new one),
  turbo A / B / C, the stick's dead zone, and EA's **4 Way Play** for four players (as on the
  cartridge: set it, Reset, then choose Players: Three or Four in the game's setup screen).
- **League**: who plays (below).
- **Game rules**: how the game plays (below).
- **Options** and **Keys**: pausing when the window loses focus, fast-forward's speed, and every
  key of the pads and the controls (pick one and press the new key, with Shift, Ctrl or Alt if you
  like).

All of it is kept in `settings.ini`, in the folder you run the game from, beside its saves; you
can edit it while the game is closed. On a Mac, hold Fn for the function keys.

### Game rules

Things the cartridge never offered, for every player and the whole game. They are set before a
game starts (a change made during one shows from the next Reset), and a saved state keeps its
game's rules. **Classic**, the default, is the original exactly.

- **Fixes and stats**: the weight bug fixed (the original sums a check's weights in a byte that
  wraps, so a heavy skater checked by a lighter player falls easily), the second assist kept on a
  give-and-go, and plus/minus with body checks in Player Stats (the sort scrolls on to two more
  columns; power play goals count for no one's +/-).
- **Time**: more period lengths in the setup (2, 3, 7 and 15 minutes besides 5, 10 and 20), and
  the regular season's overtime length.
- **How it plays**: skating speed, the speed burst, shot power, how far the puck slides, tiring,
  bench rest, rest in the penalty box, how many penalties are called, and hot and cold streaks.
- **Presets** set them all: Classic, Simulation (the fixes and stats, a weaker burst, fewer
  penalties, milder streaks) and Arcade (faster, harder, livelier).
- **The rink view**: 320 wide instead of the cartridge's 256, showing the whole width of the ice
  with the camera moving less, the scoreboard and overlays in the middle (the pause screen stays
  256 wide, as on the cartridge).

The rules that are amounts are numbers underneath. The page steps through named choices, each
showing its number (Skating speed: classic, faster 110%, fastest 125%, slower 90%), and
`settings.ini` takes a name or any number in the rule's range (`rules.skating = 115`), which the
page then shows as "custom (115%)"; the preset line follows what the file holds. `--rule` takes the
same (`--rule skating=115`; a wrong one says what the rule takes).

| Rule | Its number | Named | Range |
|---|---|---|---|
| `overtime` | minutes of regular season overtime | classic 10, 5min, 3min | 1 to 20 |
| `skating` | % of acceleration and top speed | classic 100, faster 110, fastest 125, slower 90 | 50 to 150 |
| `burst` | % of the speed burst's push | classic 100, weaker 50, weakest 25, stronger 150 | 0 to 200 |
| `shot_power` | % of a shot's speed | classic 100, harder 115, softer 85 | 50 to 150 |
| `puck_slide` | % of the puck's slowing on the ice (less slides farther) | classic 100, farther 67, shorter 150 | 25 to 200 |
| `tiring` | % of the energy skating and bursts use | classic 100, less 50, more 150 | 0 to 200 |
| `bench_rest` | % of the energy the bench gives back | classic 100, faster 150, slower 67 | 0 to 200 |
| `penalties` | % of penalties called (icing and offside always) | classic 100, fewer 50, rare 25 | 0 to 100 |
| `streaks` | % of hot and cold streaks' effect on ratings | classic 100, milder 50, off 0 | 0 to 200 |

The ranges are what the game's motion takes: headless games at both ends of every one, all at
once, play on with every speed well inside what the game keeps. The on / off rules and period
lengths are `on` / `off` and `classic` / `more`; the rink view is `classic` / `wide`.

## Leagues

Play the cartridge's 26 teams (and its two All-Star teams), or a league of your own of up to 32:
teams, players, ratings, lines, colours, logos, banners, centre ice, player photos, and even the
title screens, rinks, sprites and fonts every team shares. A league is a folder of text files a
spreadsheet opens and PNGs any paint program edits, in `leagues/` beside the game, or the same
packed in one zip.

```
./build/nhl94 --export-league leagues/my-league          # the cartridge's league, to change as you like
./build/nhl94 --extract-rom "HACK.bin" leagues/2026      # a hack's league (read as data, never run)
./build/nhl94 --check-league leagues/my-league           # what holds, or what is wrong and on which line
./build/nhl94 --pack-league leagues/my-league my.zip     # one file to keep or send, played as it is
```

(Each needs the cartridge too: `--rom FILE`, or your dump in the folder as `NHL 94 (UE) [!].gen`.)

Choose one on the menu's **League** page and Reset: those in `leagues/` are listed, and its
**Files** line finds one anywhere else (a folder, or a package). The page says what the chosen
league holds: where its files are, how many teams it has (and how many play for the cup), and its
best five skaters, five goalies and eleven teams, by the overall and team ratings the game's cards
show. Each league keeps its own records, and a saved state plays on with the league it was saved
in. The cartridge's own league, exported and built again, plays the original frame for frame.

Community hacks made on the 32-team template (NHL 94 2025, NHL 2026 94 Edition, the 30th
Anniversary Championship Teams and the like) extract whole: teams, players, ratings, lines,
playoff draws, colours and jerseys, and all their art. Their code is not taken, so what a hack
changed there (its own gameplay, extra menu lines, its texts) is not either.

**Everything about leagues, file by file and column by column, is in
[docs/LEAGUES.md](docs/LEAGUES.md):** the folder's layout, every CSV column and its range, the
art's sizes and colours, what is and is not taken from a hack, packages, records and states.

## Build

The hardware side is [pulseman](https://github.com/1nputs/pulseman), our Mega Drive runtime,
included as a git submodule in `ext/pulseman`: clone with `--recursive`, or after a plain clone
run `git submodule update --init`. `make nhl94` builds it too. Besides that you need GCC (C and
C++), `make`, `pkg-config` and SDL2. The game's own code builds with Clang as well (its capture
test passes built either way), but the builds made and tested here use GCC. Your saves and
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

## For agents: where this stands (18 October 2026)

- **What it is.** A hand-written C reimplementation of NHL '94 (Genesis, US/Europe retail, SHA-1
  `7b1489ab501258133bf03e5f0892b77d04312580`). `src/` has one C file per asm module of the
  community's byte-exact rebuild (34 modules, about 1,500 routines, the same names). The Z80's
  661-byte sound relay is ported too (`src/z80.c`). It runs on **pulseman**
  ([1nputs/pulseman](https://github.com/1nputs/pulseman), MIT, the git submodule `ext/pulseman`;
  its C API keeps the `mdrt_` prefix): the VDP, PSG, YM2612, pads and save RAM as C modules,
  with an SDL2 platform. There is no 68k or Z80 emulation. A change to pulseman is committed and
  pushed there, then the submodule's new commit is committed here.
- **How it is verified.** Genesis Plus GX with a hook (the oracle, a development tool only)
  records real play. `make test` replays 10,276 recorded routine calls. `make lockstep` runs 18
  whole sessions against the oracle, checking work RAM and VDP at every vblank. `make
  golden-check` compares the native build's own output with a recorded build, bit for bit (20
  runs, about 227,000 frames, the rules' and leagues' own among them), plus save-state round
  trips. `make rulestest` checks the game rules on the game's own routines (and the League page's
  ratings against the cards' for every player). `make leaguetest` checks the leagues: the
  cartridge's exported and built again, byte for byte; a 32-team one; 22 broken ones refused; and
  with hack ROMs in `../roms` (or `HACKS=`), each extracted, built to its teams, colours and art
  as the hack has them, played and packed. pulseman is checked on the oracle's vectors (`make
  runtime-check`) and on this game's own recordings (`make runtime-selfcheck`).
- **Done.** The harness, the game-play core, video and sound, the frame drivers, native running,
  lockstep, the whole front end (it boots from power on), a cleanup, golden runs, native save
  states and rewind. Then the game rules (`src/rules.h`: numbers, Classic the original's, kept in
  states) and leagues (`src/league.h`'s tables; `native/league.c` reads and builds them,
  `native/romsrc.c` reads hacks, `native/art.c` the pictures, `docs/LEAGUES.md` the format), the
  League and Game rules pages. In pulseman: the console's state as a value, a client-owned clock
  with line-by-line drawing and an H-interrupt hook, a bus map, FM cores and models chosen at run
  time, a trace recorder, PNG and zip, the client's option pages, and vsync.
- **Native state** (the owner's direction): the game's RAM is a native struct (the 68000's offsets,
  the host's byte order, any compiler); the console's big-endian view of it is made on demand
  (`src/mem.h` reads and writes by 68000 address through `src/ram.c`'s generated map,
  `ram_pack`/`ram_unpack` for the checks and states). RAM's regions become typed native structs
  one by one: the team structs are (`struct Team`); then the rest, then leagues as native tables.
- **Deferred by the owner.** WebAssembly (the game's code compiles for it now; a browser platform
  for pulseman is the job) and the cartridge's slowdown (it needs a 68k cost model).
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
- [x] Save states (the menu's slots) and rewind (F4)
- [x] FM core and chip model choice (`--fm-core nuked|ymfm`, `--fm-model ym2612|ym3438`)
- [x] Headless runs: screenshots, WAV, scripted input, RAM watch (`--watch`), trace recording (`--record`)
- [x] Mute, volume, audio device and latency options
- [x] A settings file (window, scaling, volume, keys), every key rebindable
- [x] A menu: the ROM in a file browser, reset, video, sound and the FM chip live, key rebinding
- [x] Integer and sharp-bilinear scaling, hi-DPI
- [x] Pixel-art upscalers: Scale2x, Scale3x, MMPX 2x and 4x, xBR 2x, 3x and 4x
- [x] Screenshot key (PNG)
- [x] Fast-forward, slow motion, frame step
- [x] Pause when the window loses focus
- [x] Controller remapping, player slots, turbo, dead zones
- [x] Capture: the replay (the last 20 seconds) and recordings, as GIFs
- [x] Save slots with thumbnails; quick resume on quit and start (states during play)
- [x] Game rules, the host's for every player (Classic: the original): period lengths, overtime, the weight bug fix, the second assist fix, plus/minus and checks, skating, burst, shots, puck, energy, penalties, streaks, presets, the rink view
- [x] The rules as numbers: any value in a rule's range in `settings.ini` or `--rule`, named choices on the page
- [x] Leagues of your own, up to 32 teams: teams, players, lines, colours and pictures in files, name banners drawn for new teams
- [x] Leagues extracted from hacks on the 32-team template (teams, players, ratings, lines, draws, colours and all their art)
- [x] League packages: a league in one zip, to keep or send, played as it is
- [x] League art as PNGs to draw over: logos, banners, centre ice, photos, and the art every team shares (title screens, rinks, sprites, fonts ...)
- [x] A League page: choose the league, and see its files, teams, best players and teams
- [x] Vsync: no tearing
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
- [x] A wide rink view (320 wide, a game rule; true 16:9 widescreen still to come)
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
  JCGT 2021), **xBR** by Hyllian, and **Scale2x / Scale3x** by Andrea Mazzoleni after Eric
  Johnston's EPX: the upscalers in the Video menu.
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
and xBR upscalers follow their authors' MIT-licensed code.
