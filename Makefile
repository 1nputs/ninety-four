# NHL '94 C port. `make test` runs the function-level capture tests.
CC      ?= gcc
CFLAGS  ?= -std=gnu11 -O1 -g -Wall -Wextra -Wno-unused-parameter -Wno-unused-function
SRC     := $(wildcard src/*.c)
TESTSRC := test/captest.c test/replay.c test/stubs.c test/ports.c test/hal_replay.c
CAPS    := $(wildcard out/captures/*.cap)

build/captest: $(SRC) $(TESTSRC) src/ram.h src/game.h src/symbols_gen.h src/anim_gen.h src/collide_tables_gen.h src/checks_tables_gen.h src/input_tables_gen.h src/penalty_tables_gen.h src/hockey_tables_gen.h src/display_tables_gen.h src/setup_tables_gen.h src/checks_strings_gen.h src/period_strings_gen.h src/hockey_strings_gen.h src/input_strings_gen.h src/penalty_strings_gen.h src/display_strings_gen.h src/replay_strings_gen.h src/title_strings_gen.h src/data_strings_gen.h src/shootout_strings_gen.h src/menu_strings_gen.h test/replay.h test/ports.h
	@mkdir -p build
	$(CC) $(CFLAGS) -Isrc -Itest -o $@ $(SRC) $(TESTSRC)

src/ram.h: tools/gen_ram.py out/ramwidths.json
	python3 -I tools/gen_ram.py

src/collide_tables_gen.h: tools/gen_tables.py
	python3 -I tools/gen_tables.py ref/NHL94Genesis/src/collide94.asm "checkpuckcoll/.cbg:cbg" "checkpuckcoll/.cbgsq:cbgsq" "puckgoalie/.list:savelist" "puckgoalie/.list2:saveodds" "puckgoalie/.list3:saveodds3" "Setplass/.alist:alist" "FallDown/.FallList:FallList" checkinglist -o $@

src/checks_tables_gen.h: tools/gen_tables.py
	python3 -I tools/gen_tables.py ref/NHL94Genesis/src/checks94.asm "asswingo/.dedata:wingo_zones" "asscentero/.dedata:centero_zones" "asspuckc/.postab2:puckc_postab" "goaliesave/.saveanim:saveanim" "CompLine/.hl1:homelines" "CompLine/.al1:awaylines" "puckfaceoff2/.apl:fo_apl" "puckfaceoff2/.ptab:fo_ptab" "Endfaceoff/.ftab:fo_ftab" "doplayeracc/.ftab:turn_ftab" MaxSpeed dirtab -o $@

src/input_tables_gen.h: tools/gen_tables.py
	python3 -I tools/gen_tables.py ref/NHL94Genesis/src/input94.asm "getlchoice/.tab:lchoice_tab" shotsets -o $@

src/checks_strings_gen.h src/input_strings_gen.h src/penalty_strings_gen.h src/period_strings_gen.h src/hockey_strings_gen.h src/display_strings_gen.h src/replay_strings_gen.h src/title_strings_gen.h src/data_strings_gen.h src/shootout_strings_gen.h src/menu_strings_gen.h: tools/gen_strings.py build/asm/symbols.json
	for f in checks input penalty period hockey display replay title data shootout menu; do python3 -I tools/gen_strings.py build/asm/src/$${f}94.asm -o src/$${f}_strings_gen.h; done

src/hockey_tables_gen.h: tools/gen_tables.py
	python3 -I tools/gen_tables.py ref/NHL94Genesis/src/hockey94.asm "GetPeriodTime/.timetab:periodtimes" -o $@

src/display_tables_gen.h: tools/gen_tables.py
	python3 -I tools/gen_tables.py ref/NHL94Genesis/src/display94.asm "showzam/.ftab:zam_ftab" "checksso/.tab:sso_tab" -o $@

src/setup_tables_gen.h: tools/gen_tables.py
	python3 -I tools/gen_tables.py ref/NHL94Genesis/src/setup94.asm "defaultsprites/.listsso:listsso" "defaultsprites/.listffo:listffo" "defaultsprites2/.list:dslist" -o $@

src/penalty_tables_gen.h: tools/gen_tables.py
	python3 -I tools/gen_tables.py ref/NHL94Genesis/src/penalty94.asm sctab -o $@

src/anim_gen.h: tools/gen_anim.py
	python3 -I tools/gen_anim.py

out/ramwidths.json: tools/ramwidths.py
	python3 -I tools/ramwidths.py ref/NHL94Genesis out/ramwidths.json

src/symbols_gen.h: tools/gen_symbols.py out/callgraph.json out/ramwidths.json
	python3 -I tools/gen_symbols.py

out/callgraph.json: tools/callgraph.py
	python3 -I tools/callgraph.py > /dev/null

oracle: build/nhl_oracle.so
build/nhl_oracle.so: tools/hybrid/oracle_shim.c
	$(MAKE) -s -f tools/hybrid/Makefile

test: build/captest
	@for c in $(CAPS); do echo "== $$c"; ./build/captest $$c || true; done

clean:
	rm -rf build/captest build/oracle_shim.o
.PHONY: test oracle clean
