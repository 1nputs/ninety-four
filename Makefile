# NHL '94 C port.
#   make nhl94          the native game (build/nhl94, on md-runtime at MDRT)
#   make test           the function-level capture tests (out/captures/*.cap)
#   make lockstep       the native game against the oracle at every vblank (out/vectors/*.lockstep)
#   make vectors        record the saved states, snapshots and test vectors that are missing
#   make runtime-check  md-runtime's tests on this game's vectors
#   make relaytest      the Z80 relay port against the sound traces
#   make oracle         the hooked oracle core (build/nhl_oracle.so)
#   make gen            regenerate the committed headers from ref/ and the asm rebuild
CC      ?= gcc
WERROR  ?= -Werror
WARN    := -Wall -Wextra -Wshadow -Wstrict-prototypes -Wmissing-prototypes -Wno-unused-parameter $(WERROR)
CFLAGS  ?= -std=gnu11 -O1 -g $(WARN)
SRC     := $(wildcard src/*.c)
HDR     := $(wildcard src/*.h)
TESTSRC := test/captest.c test/replay.c test/stubs.c test/ports.c test/hal_replay.c
CAPS    := $(wildcard out/captures/*.cap)
PY      := python3 -I

build/captest: $(SRC) $(HDR) $(TESTSRC) test/replay.h test/ports.h
	@mkdir -p build
	$(CC) $(CFLAGS) -Isrc -Itest -o $@ $(SRC) $(TESTSRC)

test: build/captest
	@test -n "$(CAPS)" || { echo "no captures in out/captures: tools/hybrid/capture_all.sh records them"; exit 1; }
	./build/captest $(CAPS)

# the native game: build/nhl94, on md-runtime (MDRT, its release build with the SDL2 platform)
MDRT   ?= ../md-runtime
NATIVE := $(wildcard native/*.c)
MDRTLIBS := $(MDRT)/build/release/libmdrt_sdl2.a $(MDRT)/build/release/libmdrt.a
nhl94: build/nhl94
build/nhl94: $(SRC) $(HDR) $(NATIVE) native/native.h mdrt
	@mkdir -p build
	$(CC) $(CFLAGS) -O2 -Isrc -Inative -I$(MDRT)/include $(shell pkg-config --cflags sdl2) -finstrument-functions \
	    -finstrument-functions-exclude-file-list=native/ -rdynamic -o $@ $(SRC) $(NATIVE) $(MDRTLIBS) $(shell pkg-config --libs sdl2) -lm -ldl
mdrt:
	$(MAKE) -C $(MDRT) MODE=release sdl

# the saved states, snapshots and vectors (tools/oracle/vectors.py holds the recipes)
vectors: build/nhl_oracle.so
	$(PY) tools/oracle/vectors.py record
lockstep: build/nhl94
	$(PY) tools/oracle/vectors.py check
runtime-check:
	$(MAKE) -C $(MDRT) check MDRT_VECTORS=$(abspath out/vectors)

# the Z80 relay port against sound traces
relaytest: build/relaytest
	./build/relaytest $(wildcard out/vectors/*.sndtrace)
build/relaytest: test/relaytest.c src/z80.c src/z80.h src/rom.c src/rom.h
	@mkdir -p build
	$(CC) $(CFLAGS) -Isrc -o $@ test/relaytest.c src/z80.c src/rom.c

oracle: build/nhl_oracle.so
build/nhl_oracle.so: tools/hybrid/oracle_shim.c
	$(MAKE) -s -f tools/hybrid/Makefile

# The generated headers are committed; a build never remakes them. `make gen` does, from ref/
# (tools/fetch_refs.sh) and the asm rebuild (tools/asm/build_vasm.sh: build/asm/), and shows
# what changed.
STRINGS := checks input penalty period hockey display replay title data shootout menu stats attract cards optsetup records setup scout
ASMSRC  := ref/NHL94Genesis/src
gen:
	@test -d $(ASMSRC) || { echo "make gen needs ref/NHL94Genesis: tools/fetch_refs.sh"; exit 1; }
	@test -f build/asm/symbols.json || { echo "make gen needs build/asm/symbols.json: tools/asm/build_vasm.sh"; exit 1; }
	@mkdir -p out
	$(PY) tools/ramwidths.py ref/NHL94Genesis out/ramwidths.json
	$(PY) tools/callgraph.py > /dev/null
	$(PY) tools/gen_ram.py
	$(PY) tools/gen_symbols.py
	$(PY) tools/gen_anim.py
	$(PY) tools/gen_tables.py $(ASMSRC)/collide94.asm "checkpuckcoll/.cbg:cbg" "checkpuckcoll/.cbgsq:cbgsq" "puckgoalie/.list:savelist" "puckgoalie/.list2:saveodds" "puckgoalie/.list3:saveodds3" "Setplass/.alist:alist" "FallDown/.FallList:FallList" checkinglist -o src/collide_tables_gen.h
	$(PY) tools/gen_tables.py $(ASMSRC)/checks94.asm "asswingo/.dedata:wingo_zones" "asscentero/.dedata:centero_zones" "asspuckc/.postab2:puckc_postab" "goaliesave/.saveanim:saveanim" "CompLine/.hl1:homelines" "CompLine/.al1:awaylines" "puckfaceoff2/.apl:fo_apl" "puckfaceoff2/.ptab:fo_ptab" "Endfaceoff/.ftab:fo_ftab" "doplayeracc/.ftab:turn_ftab" MaxSpeed dirtab -o src/checks_tables_gen.h
	$(PY) tools/gen_tables.py $(ASMSRC)/input94.asm "getlchoice/.tab:lchoice_tab" shotsets -o src/input_tables_gen.h
	$(PY) tools/gen_tables.py $(ASMSRC)/hockey94.asm "GetPeriodTime/.timetab:periodtimes" -o src/hockey_tables_gen.h
	$(PY) tools/gen_tables.py $(ASMSRC)/display94.asm "showzam/.ftab:zam_ftab" "checksso/.tab:sso_tab" -o src/display_tables_gen.h
	$(PY) tools/gen_tables.py $(ASMSRC)/setup94.asm "defaultsprites/.listsso:listsso" "defaultsprites/.listffo:listffo" "defaultsprites2/.list:dslist" -o src/setup_tables_gen.h
	$(PY) tools/gen_tables.py $(ASMSRC)/penalty94.asm sctab "StartHL2/.postab:hl_postab" -o src/penalty_tables_gen.h
	for f in $(STRINGS); do $(PY) tools/gen_strings.py build/asm/src/$${f}94.asm -o src/$${f}_strings_gen.h || exit 1; done
	@git status --short -- src/ram.h 'src/*_gen.h'

clean:
	rm -rf build/captest build/oracle_shim.o build/nhl94 build/relaytest
.PHONY: test oracle clean nhl94 relaytest mdrt vectors lockstep runtime-check gen
