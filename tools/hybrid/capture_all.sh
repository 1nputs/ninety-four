#!/usr/bin/env bash
# Record the standard capture sessions for `make test`. Needs build/nhl_oracle.so, the saved
# states out/states/demo_faceoff.state (CPU vs CPU, penalties / offsides / line changes on,
# 5 minute periods) and out/states/demo1p_faceoff.state (one human player, same options), and
# the random input script out/captures/checks_script.txt.
set -e
# at most CAPTURE_JOBS sessions at once (memory: a session can hold a few hundred MB of call log)
JOBS="${CAPTURE_JOBS:-4}"
throttle() { while [ "$(jobs -pr | wc -l)" -ge "$JOBS" ]; do wait -n; done; }
cd "$(dirname "$0")/../.."
ROM="${NHL94_ROM:-NHL 94 (UE) [!].gen}"
fns() { python3 -I - "$@" <<'PY'
import json, sys
cg = json.load(open('out/callgraph.json'))
print(','.join(n for n, f in cg.items() if isinstance(f, dict) and f.get('file') in sys.argv[1:] and f.get('insns', 0) >= 2))
PY
}
SCRIPT="$(cat out/captures/checks_script.txt)"
S1=out/states/demo1p_faceoff.state
S2=out/states/demo_faceoff.state
run() { out=$1; shift; python3 tools/hybrid/capture.py "$ROM" --out "out/captures/$out.cap" --frames 40000 --spread "$@" > "out/captures/$out.log" 2>&1; echo "$out: $(grep -c . out/captures/$out.log) lines, $(tail -n 2 out/captures/$out.log | head -n 1)"; }
throttle; run collide      --state $S1 --script "$SCRIPT" --max 32 --fn "$(fns collide94),randomd0,vtoa,GetHot,SetSPA" &
throttle; run checks       --state $S1 --script "$SCRIPT" --max 32 --fn "$(fns checks94)" &
throttle; run checks2      --state $S2 --max 16 --fn "$(fns checks94)" &
throttle; run assign_input --state $S2 --max 32 --fn "$(fns assign94 input94)" &
throttle; run input1p      --state $S1 --script "$SCRIPT" --max 32 --fn "$(fns input94)" &
throttle; run goc          --state $S1 --script "$SCRIPT" --max 32 --fn "$(fns goalie94 onetimer94 crowd94)" &
throttle; run penalty      --state $S2 --max 32 --fn "$(fns penalty94 | sed -E "s/(^|,)(Intermission|DoHiLights|StartHL|StartHL2)(,|$)/\\1/g; s/,,+/,/g; s/^,|,$//g")" &
# period94 + hockey94 without the frame drivers (they run whole frames: ported in the game-loop phase)
throttle; run ph           --state $S2 --max 24 --fn "$(fns period94 hockey94 | sed -E "s/(^|,)(Begin|DoGameFrame|GameStatisticsScreen|Gameloop|Pausemode|PeriodStatsScreen|StartGame|StartPer|VBjsr)(,|$)/\\1/g; s/,,+/,/g; s/^,|,$//g")" &
# display94 + replay94 without ReplayMode (interactive; the replay navigation helpers get no calls in these sessions)
throttle; run dr           --state $S1 --script "$SCRIPT" --max 24 --fn "$(fns display94 replay94 | sed -E "s/(^|,)ReplayMode(,|$)/\\1/g; s/,,+/,/g; s/^,|,$//g")" &
# video94: the hardware boundary (VDP writes are checked in order against the logged writes)
throttle; run video        --state $S1 --script "$SCRIPT" --max 32 --fn "$(fns video94)" &
# the game-play helpers of title94 / data94 / setup94 / stats94 / cards94 / shootout94 / menu94 / fourway94 / sram94
# (the front-end screens of those modules never run in a game); 80000 frames to reach the end of the game
HELPERS="$(tr -s ' \n' ',' < tools/hybrid/helpers_fns.txt)"
throttle; run helpers1p    --state $S1 --script "$SCRIPT" --max 24 --frames 80000 --fn "$HELPERS" &
throttle; run helpers      --state $S2 --max 24 --frames 80000 --fn "$HELPERS" &
# sound94: the sequencer runs every vblank (p_music_vblank); songs at faceoffs, goals and period ends
throttle; run sound        --state $S2 --max 32 --frames 40000 --fn "$(fns sound94)" &
throttle; run sound1p      --state $S1 --script "$SCRIPT" --max 32 --frames 40000 --fn "$(fns sound94)" &
# whole frames and the routines that wait for the vblank: the shim logs each interrupt, and the
# tests replay it where the C waits (records whose interrupts all landed in wait loops)
FRAME="$(tr -s ' \n' ',' < tools/hybrid/frame_fns.txt)"
throttle; run frame1p      --state $S1 --script "$SCRIPT" --max 24 --frames 80000 --fn "$FRAME" &
throttle; run frame        --state $S2 --max 24 --frames 80000 --fn "$FRAME" &
wait
