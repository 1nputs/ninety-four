# ninety-four: NHL '94 rebuilt in C.
#   make nhl94          the native game (build/nhl94, on pulseman at MDRT)
#   make test           the function-level capture tests (out/captures/*.cap)
#   make lockstep       the native game against the oracle at every vblank (out/vectors/*.lockstep)
#   make vectors        record the saved states, snapshots and test vectors that are missing
#   make runtime-check  pulseman's tests on this game's vectors
#   make golden         record the native game's golden runs (out/golden; make golden-check compares)
#   make golden-cross   the native game built by another compiler (Clang), checked against them
#   make golden-ubsan   a build with the undefined behaviour sanitizer, checked against them
#   make netplay-check  two players online (headless, a simulated network) against solo runs
#   make runtime-selfcheck  pulseman's tests on the native game's own traces (nhl94 --record)
#   make relaytest      the Z80 relay port against the sound traces
#   make rulestest      the game rules on the game's routines (test/rules_test.c)
#   make rosterstest     roster sets: the cartridge's exported and built again, a 32-team one, broken ones,
#                       hacks on the 32-team template (../roms or HACKS=) extracted
#   make oracle         the hooked oracle core (build/nhl_oracle.so)
#   make gen            regenerate the committed headers from ref/ and the asm rebuild
#   make release VERSION=0.3.2  the downloads for Linux, Windows and macOS in out/release/v0.3.2,
#                       named by the version (tools/release.sh; NOTES=FILE their notes)
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

# the game rules on the game's routines, RAM set up by hand (test/rules_test.c)
build/rulestest: $(SRC) $(HDR) test/rules_test.c test/hal_null.c native/ratings.c
	@mkdir -p build
	$(CC) $(CFLAGS) -Isrc -Itest -Inative -I$(MDRT)/include -o $@ $(SRC) test/rules_test.c test/hal_null.c native/ratings.c
rulestest: build/rulestest
	./build/rulestest

# roster sets: the cartridge's exported and built again (byte for byte), a 32-team one, broken ones
# (tools/rosterstest.py; the ROM as for make nhl94)
build/rosterstest: native/rosters.c native/banner.c native/romsrc.c native/art.c native/ratings.c src/rosters.c src/rom.c src/rosters.h src/rom.h native/native.h test/rosters_test.c mdrt
	@mkdir -p build
	$(CC) $(CFLAGS) -Isrc -Inative -I$(MDRT)/include -o $@ native/rosters.c native/banner.c native/romsrc.c native/art.c native/ratings.c src/rosters.c src/rom.c \
	    test/rosters_test.c $(MDRT)/$(MDRTB)/libmdrt.a -lm $(CXXLIB)
rosterstest: build/rosterstest build/nhl94$(EXE)
	$(PY) tools/rosterstest.py

test: build/captest
	@test -n "$(CAPS)" || { echo "no captures in out/captures: tools/hybrid/capture_all.sh records them"; exit 1; }
	@# (RAM's bytes only through src/mem.h's places: no host pointer into g as bytes in the game's code;
	@#  a typed member's address, &g.teams[0], is native)
	@grep -nE '\bram_ptr\(|&g([^.[:alnum:]_]|$$)' $(filter-out src/ram.h src/ram.c src/mem.h,$(wildcard src/*.c src/*.h)); \
	    case $$? in 1) ;; 0) echo "raw RAM access outside src/mem.h (above)"; exit 1;; *) exit 1;; esac
	@# (the roster set's teams through src/rosters.h's tables: no team data read by its address, no table
	@#  of the cartridge's formats but the banners', which are art)
	@awk '/(^|[^A-Za-z0-9_])RS\((TeamList|TeamPalettes|TeamSongs|TeamRatings|FeaturedPictures|playoffseats|PlayerRecordOffsets|OptionTeamNames|TeamLogoBitmaps|TeamLogoPalettes|TeamGfxList)\)/ || \
	      (/[.>]data([^A-Za-z0-9_]|$$)/ && !/->data = rosters_team\(/) { print FILENAME ":" FNR ": " $$0; bad = 1 } END { exit bad }' \
	    $(filter-out src/rosters.c src/rosters.h,$(wildcard src/*.c src/*.h)); \
	    case $$? in 0) ;; 1) echo "a team's data read outside the roster set's tables (above)"; exit 1;; *) exit 1;; esac
	./build/captest $(CAPS)

# the native game: build/nhl94, on pulseman (MDRT: the git submodule ext/pulseman, or another
# checkout; its release build with the SDL2 platform)
MDRT   ?= ext/pulseman
NATIVE := $(wildcard native/*.c)
MDRTB  ?= build/release
MDRTLIBS := $(MDRT)/$(MDRTB)/libmdrt_sdl2.a $(MDRT)/$(MDRTB)/libmdrt.a
ifeq ($(OS),Windows_NT)
EXE    ?= .exe
NETLIB ?= -lws2_32                                       # (pulseman's online play: Winsock)
# (Windows: the C++ runtime linked in, so the game needs only SDL2.dll beside it)
CXXLIB ?= -static-libgcc -Wl,-Bstatic -lstdc++ -Wl,--whole-archive -lwinpthread -Wl,--no-whole-archive -Wl,-Bdynamic
endif
# (pulseman's ymfm core is C++)
CXXLIB ?= -lstdc++
# (Windows: SDL2's -mwindows is left out, so the game keeps its console; out/ is where save RAM
# and states go)
SDLLIBS := $(filter-out -mwindows,$(shell pkg-config --libs sdl2))
# a file at a time into OBJ: the game's routines instrumented (the C side follows the routines the
# game is in, so states know where it waits), the native side not
BIN    ?= build/nhl94$(EXE)
OBJ    ?= build/obj
NOBJ   := $(SRC:%.c=$(OBJ)/%.o) $(NATIVE:%.c=$(OBJ)/%.o)
NFLAGS  = $(CFLAGS) $(SANITIZE) -O2 -Isrc -Inative -I$(MDRT)/include $(shell pkg-config --cflags sdl2)
MDRTHDR := $(wildcard $(MDRT)/include/mdrt/*.h)
nhl94: $(BIN)
$(OBJ)/src/%.o: src/%.c $(HDR) $(MDRTHDR)
	@mkdir -p $(dir $@)
	$(CC) $(NFLAGS) -finstrument-functions -c $< -o $@
$(OBJ)/native/%.o: native/%.c $(HDR) native/native.h native/routines_gen.h $(MDRTHDR)
	@mkdir -p $(dir $@)
	$(CC) $(NFLAGS) -c $< -o $@
$(OBJ)/native/net.o: build/build_id.h
NFLAGS += -Ibuild
# the build's identity, which netplay's peers compare (the same code on both): the commit, "-dirty"
# with changes not committed; written again only when it changes
build/build_id.h: FORCE
	@mkdir -p build
	@c=$$(git rev-parse HEAD 2>/dev/null | cut -c1-12); git diff --quiet HEAD 2>/dev/null || c="$$c-dirty"; \
	    id='#define NHL94_BUILD "'"$${c:-unknown}"'"'; \
	    [ "$$(cat $@ 2>/dev/null)" = "$$id" ] || echo "$$id" > $@
FORCE:
$(BIN): $(NOBJ) mdrt
	@mkdir -p $(dir $@) out
	$(CC) $(CFLAGS) $(SANITIZE) -O2 $(LDFLAGS) -o $@ $(NOBJ) $(MDRTLIBS) $(SDLLIBS) -lm $(CXXLIB) $(NETLIB)
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
# pulseman's tests on the native game's own traces (nhl94 --record): 600 frames of play, headless
runtime-selfcheck: build/nhl94
	@test -f out/states/faceoff.snap || { echo "runtime-selfcheck needs out/states/faceoff.snap: make vectors"; exit 1; }
	@mkdir -p out/selfvectors
	./build/nhl94 --headless --snap out/states/faceoff.snap --input tools/oracle/scripts/random_seed94.txt --frames 600 --record out/selfvectors
	$(MAKE) -C $(MDRT) check MDRT_VECTORS=$(abspath out/selfvectors)
# golden runs: the native game's own output, frame by frame, against a build's recorded runs
golden: build/nhl94
	$(PY) tools/golden.py record
golden-check: build/nhl94
	$(PY) tools/golden.py check
# netplay: two headless players over the loopback through a simulated network, every frame as a
# solo run of the same buttons (tools/netplay_check.py)
netplay-check: build/nhl94
	$(PY) tools/netplay_check.py
# the native game and pulseman built by another compiler (XCC, XCXX, XAR: Clang's by default; or
# ZIG=path/to/zig, its Clang) into build/cross, its golden runs checked against those recorded
# here (out/golden-cross): the game plays bit for bit the same whatever compiles it, which netplay
# between builds needs
XCC     ?= clang
XCXX    ?= clang++
XAR     ?= ar
XCXXLIB ?= -lstdc++
ifdef ZIG
XCC     := $(abspath $(ZIG)) cc
XCXX    := $(abspath $(ZIG)) c++
XAR     := $(abspath $(ZIG)) ar
XCXXLIB := -lc++
endif
golden-cross:
	$(MAKE) CC="$(XCC)" CXX="$(XCXX)" AR="$(XAR)" CXXLIB="$(XCXXLIB)" BIN=build/cross/nhl94$(EXE) OBJ=build/cross/obj \
	    MDRTB=build/cross build/cross/nhl94$(EXE)
	$(PY) tools/golden.py check --build build/cross/nhl94$(EXE) --out out/golden-cross
# the native game built with GCC's undefined behaviour sanitizer (any undefined behaviour stops it)
# into build/ubsan, its golden runs checked against those recorded here (out/golden-ubsan)
golden-ubsan:
	$(MAKE) SANITIZE="-fsanitize=undefined -fno-sanitize-recover=undefined" BIN=build/ubsan/nhl94$(EXE) \
	    OBJ=build/ubsan/obj build/ubsan/nhl94$(EXE)
	$(PY) tools/golden.py check --build build/ubsan/nhl94$(EXE) --out out/golden-ubsan

# the Z80 relay port against sound traces
relaytest: build/relaytest
	./build/relaytest $(wildcard out/vectors/*.sndtrace)
build/relaytest: test/relaytest.c src/z80.c src/z80.h src/rom.c src/rom.h
	@mkdir -p build
	$(CC) $(CFLAGS) -Isrc -o $@ test/relaytest.c src/z80.c src/rom.c

# the release's downloads, named by the version, built from clean at the commit checked out
release:
	tools/release.sh $(VERSION)

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
	rm -rf build/captest build/oracle_shim.o build/nhl94 build/obj build/cross build/relaytest
.PHONY: rulestest rosterstest test oracle clean nhl94 relaytest mdrt vectors lockstep runtime-check runtime-selfcheck golden golden-check \
	golden-cross golden-ubsan netplay-check gen release FORCE
