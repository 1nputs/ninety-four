# ninety-four: NHL '94 rebuilt in C.
#   make nhl94          the native game (build/nhl94, on pulseman at MDRT)
#   make test           the function-level capture tests (out/captures/*.cap)
#   make lockstep       the native game against the oracle at every vblank (out/vectors/*.lockstep)
#   make vectors        record the saved states, snapshots and test vectors that are missing
#   make runtime-check  pulseman's tests on this game's vectors
#   make golden         record the native game's golden runs (out/golden; make golden-check compares)
#   make runtime-selfcheck  pulseman's tests on the native game's own traces (nhl94 --record)
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

# the native game: build/nhl94, on pulseman (MDRT: the git submodule ext/pulseman, or another
# checkout; its release build with the SDL2 platform)
MDRT   ?= ext/pulseman
NATIVE := $(wildcard native/*.c)
MDRTB  ?= build/release
MDRTLIBS := $(MDRT)/$(MDRTB)/libmdrt_sdl2.a $(MDRT)/$(MDRTB)/libmdrt.a
ifeq ($(OS),Windows_NT)
EXE    ?= .exe
# (Windows: the C++ runtime linked in, so the game needs only SDL2.dll beside it)
CXXLIB ?= -static-libgcc -Wl,-Bstatic -lstdc++ -Wl,--whole-archive -lwinpthread -Wl,--no-whole-archive -Wl,-Bdynamic
endif
# (pulseman's ymfm core is C++)
CXXLIB ?= -lstdc++
nhl94: build/nhl94$(EXE)
# (Windows: SDL2's -mwindows is left out, so the game keeps its console; out/ is where save RAM
# and states go)
SDLLIBS := $(filter-out -mwindows,$(shell pkg-config --libs sdl2))
build/nhl94$(EXE): $(SRC) $(HDR) $(NATIVE) native/native.h native/routines_gen.h mdrt
	@mkdir -p build out
	$(CC) $(CFLAGS) -O2 -Isrc -Inative -I$(MDRT)/include $(shell pkg-config --cflags sdl2) -finstrument-functions \
	    -finstrument-functions-exclude-file-list=native/ -o $@ $(SRC) $(NATIVE) $(MDRTLIBS) $(SDLLIBS) -lm $(CXXLIB)
mdrt:
	@test -f $(MDRT)/Makefile || { echo "no pulseman at $(MDRT): git submodule update --init (or make MDRT=path/to/pulseman)"; exit 1; }
	$(MAKE) -C $(MDRT) MODE=release B=$(MDRTB) sdl

# the saved states, snapshots and vectors (tools/oracle/vectors.py holds the recipes)
vectors: build/nhl_oracle.so
	$(PY) tools/oracle/vectors.py record
lockstep: build/nhl94
	$(PY) tools/oracle/vectors.py check
runtime-check:
	$(MAKE) -C $(MDRT) check MDRT_VECTORS=$(abspath out/vectors) MDRT_REQUIRE_VECTORS=1
# pulseman's tests on the native game's own traces (nhl94 --record): 600 frames of play
runtime-selfcheck: build/nhl94
	@test -f out/states/faceoff.snap || { echo "runtime-selfcheck needs out/states/faceoff.snap: make vectors"; exit 1; }
	@mkdir -p out/selfvectors
	./build/nhl94 --snap out/states/faceoff.snap --input tools/oracle/scripts/random_seed94.txt --frames 600 --record out/selfvectors
	$(MAKE) -C $(MDRT) check MDRT_VECTORS=$(abspath out/selfvectors)
# golden runs: the native game's own output, frame by frame, against a build's recorded runs
golden: build/nhl94
	$(PY) tools/golden.py record
golden-check: build/nhl94
	$(PY) tools/golden.py check

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
	$(PY) tools/gen_routines.py $(CC)
	@git status --short -- src/ram.h 'src/*_gen.h' native/routines_gen.h

clean:
	rm -rf build/captest build/oracle_shim.o build/nhl94 build/relaytest
.PHONY: test oracle clean nhl94 relaytest mdrt vectors lockstep runtime-check runtime-selfcheck golden golden-check gen
