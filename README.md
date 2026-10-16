# NHL '94 (Genesis) in C

A behaviour-verified C reimplementation of EA's NHL '94 for the Sega Genesis, built on the
community's byte-matching assembly rebuild. See `docs/PLAN.md` for the approach and
`docs/HANDOFF.md` for the working guide (setup, porting recipe, status, quirks, next steps).

Status: the whole game is ported to `src/`, from power on (the EA screen, the title, the setup
screen, the name entry, the playoffs, the scouting report) through the game, its menus and
screens, the intermissions and highlights to the game over, with the Z80's sound relay in C. It
passes 10197 recorded function-level tests and frame-by-frame lockstep runs against the oracle
emulator (work RAM and VDP at every vblank wait) with 0 mismatches. It runs natively on
md-runtime (`../md-runtime`) with sound.

## Play

```
make nhl94
./build/nhl94
```

Arrows, Z / X / C, Enter (or a game controller); pad 2 is I J K L, Q / W / E, Tab. It starts at
power on; `--snap FILE` starts from an oracle snapshot instead. See `docs/HANDOFF.md` section 9.

## Setup

The ROM is your own copy; nothing derived from it is committed.

```
./tools/fetch_refs.sh                  # the asm rebuild, the disassembly, the oracle emulator, vasm
cp /path/to/your/rom "NHL 94 (UE) [!].gen"   # retail ROM, SHA-1 7b1489ab501258133bf03e5f0892b77d04312580
tools/asm/build_vasm.sh                # the byte-exact rebuild: build/asm/symbols.json (every label)
```

## Verify

```
make oracle                            # build/nhl_oracle.so: the emulator with the capture hook
make vectors                           # saved states, snapshots, lockstep and hardware vectors (out/)
tools/hybrid/capture_all.sh            # the 29 capture sessions (out/captures/*.cap)
make test                              # the function-level tests: every capture against the C port
make lockstep                          # the native game against the oracle at every vblank
make runtime-check                     # md-runtime's tests on this game's vectors
make golden && make golden-check       # this build's own output, frame by frame, before and after a change
```

`docs/HANDOFF.md` section 2 has the details; `tools/oracle/vectors.py` holds every recipe.
