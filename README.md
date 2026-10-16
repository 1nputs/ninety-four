# NHL '94 (Genesis) in C

A behaviour-verified C reimplementation of EA's NHL '94 for the Sega Genesis, built on the
community's byte-matching assembly rebuild. See `docs/PLAN.md` for the approach and
`docs/HANDOFF.md` for the working guide (setup, porting recipe, status, quirks, next steps).

Status: the whole game is ported to `src/`, from power on (the EA screen, the title, the setup
screen, the name entry, the playoffs, the scouting report) through the game, its menus and
screens, the intermissions and highlights to the game over, with the Z80's sound relay in C. It
passes 10065 recorded function-level tests and frame-by-frame lockstep runs against the oracle
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

```
./tools/fetch_refs.sh          # clones the asm rebuild and disassembly, builds the oracle emulator
cp /path/to/your/nhl94.gen .   # retail ROM, SHA-1 7b1489ab501258133bf03e5f0892b77d04312580
```

## Oracle

```
python3 tools/oracle/run.py "NHL 94 (UE) [!].gen" --frames 600 --shots out/shots
```

## Assemble the upstream source on Linux (byte-exact)

```
tools/asm/build_vasm.sh        # build/asm/nhl94.bin == the ROM; build/asm/symbols.json = every label
```

## Function-level tests

```
make oracle                    # build/nhl_oracle.so: the emulator with the capture hook
tools/hybrid/capture_all.sh    # records the sixteen standard sessions into out/captures/*.cap
python3 tools/hybrid/capture.py "NHL 94 (UE) [!].gen" --fn checkcoll,checkcx --out out/captures/x.cap --frames 600
make test                      # replays every capture in out/captures against the C port
```
