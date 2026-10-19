#!/usr/bin/env python3
"""Golden runs: the native game headless from power on (or a snapshot) with scripted input, a line
of hashes a frame (nhl94 --golden: work RAM, the VDP state and Z80 RAM at the game's wait, the
picture, the frame's sound). `record` keeps a build's runs in out/golden; `check` runs the build
again and compares, frame by frame, so a change to pulseman or the native client can be shown
to change nothing it was not meant to. The runs use a fixed HV counter stand-in (main.c), so
changes to the runtime's HV model do not show; everything else must match bit for bit.

The league runs play the cartridge's league exported and built again (out/leagues/cartridge: its
teams a league's own, so only work RAM may differ from the original's run, where a team struct
keeps its data's number: LIKE) and a 32-team one (tools/leaguetest.py makes both).

`check` also saves a native state in three of the runs (at the first game loop wait from a given
frame on), loads it in a new process and runs on: the frames after it must be the same as the
run's own (a state holds everything the game goes on from). And it plays the player's requests
from a script (states_faceoff.txt: SAVE, LOAD, REWIND held): the frames after the load must be those after
the save, and the rewind must step back three frames a frame through frames already shown. And it
starts the game again in the same process (--restart-at, as the window menu's reset does): the
frames after that must be a fresh power on's.

    tools/golden.py record [--force]     (make golden: the runs not yet recorded; --force: all of them)
    tools/golden.py check                (make golden-check)
    tools/golden.py check --build B --out D   (make golden-cross: another compiler's build B checked
                                         against the runs recorded here, its own runs kept in D)
"""
import argparse
import os
import re
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import leaguetest  # noqa: E402  (tools/leaguetest.py: the leagues the league runs play)

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SCRIPTS = os.path.join(ROOT, 'tools', 'oracle', 'scripts')
NHL94 = os.path.join(ROOT, 'build', 'nhl94')

RULES_ON = ['--rule', 'period_lengths=more', '--rule', 'overtime=5min', '--rule', 'weight_fix=on', '--rule', 'assist_fix=on',
            '--rule', 'more_stats=on']
# (name, frames, options): power on unless --snap
RUNS = [
    ('attract', 7200, []),
    ('full_game', 16000, ['--input', 'full_game_30s.txt']),
    ('playoff_game', 15000, ['--input', 'playoff_game_30s.txt']),
    ('records_game', 15700, ['--input', 'records_game_30s.txt']),
    ('pause_menus', 8600, ['--input', 'pause_menus.txt']),
    ('shootout_menus', 4800, ['--input', 'shootout_menus.txt']),
    ('name_entry', 3000, ['--input', 'name_entry.txt']),
    ('faceoff', 18000, ['--snap', 'out/states/faceoff.snap', '--input', 'random_seed94.txt']),
    # the game rules (native extras: their own baselines, not the original's): every rule on
    ('rules_game', 16000, ['--input', 'full_game_30s.txt'] + RULES_ON),
    ('rules_faceoff', 18000, ['--snap', 'out/states/faceoff.snap', '--input', 'random_seed94.txt'] + RULES_ON),
    ('arcade_faceoff', 18000, ['--snap', 'out/states/faceoff.snap', '--input', 'random_seed94.txt', '--rule', 'preset=arcade']),
    ('simulation_faceoff', 18000, ['--snap', 'out/states/faceoff.snap', '--input', 'random_seed94.txt', '--rule', 'preset=simulation']),
    # leagues (league.c): the cartridge's built from its files, and 32 teams
    ('league_game', 16000, ['--input', 'full_game_30s.txt', '--league', 'out/leagues/cartridge']),
    ('league_playoff', 15000, ['--input', 'playoff_game_30s.txt', '--league', 'out/leagues/cartridge']),
    ('league32_attract', 7200, ['--league', 'out/leagues/thirty-two']),
    ('league32_game', 16000, ['--input', 'full_game_30s.txt', '--league', 'out/leagues/thirty-two']),
    ('league32_playoff', 15000, ['--input', 'playoff_game_30s.txt', '--league', 'out/leagues/thirty-two']),
    # the wide view (the rink 320 wide: src/view.h)
    ('wide_game', 16000, ['--input', 'full_game_30s.txt', '--rule', 'view=wide']),
    ('wide_faceoff', 18000, ['--snap', 'out/states/faceoff.snap', '--input', 'random_seed94.txt', '--rule', 'view=wide']),
    ('wide_pause_menus', 8600, ['--input', 'pause_menus.txt', '--rule', 'view=wide']),
]
# runs that must be another's frame for frame but for work RAM (the league's tables elsewhere)
LIKE = {'league_game': 'full_game', 'league_playoff': 'playoff_game'}
# the runs a state is saved in (from this frame on) and loaded from
STATES = {'full_game': 3000, 'playoff_game': 9000, 'faceoff': 6000, 'rules_faceoff': 6000, 'simulation_faceoff': 6000,
          'league32_game': 3000, 'wide_faceoff': 6000}
COLUMNS = ['work RAM', 'VDP state', 'Z80 RAM', 'picture', 'sound', 'samples']
SAVED = re.compile(r'state saved at frame (\d+) \(golden line (\d+)\)')


def run(name, frames, opts, out, save=None, load=None):
    """nhl94 headless; save: (frame, path) saves a state; load: a state to start from. Returns
    (name, error or None, the golden line the state was saved after)."""
    args = [NHL94, '--frames', str(frames), '--golden', out]
    if save:
        args += ['--save-state', '%d:%s' % save]
    if load:                                              # (the state brings its own rules and league: none given)
        drop = ('--snap', '--rule', '--league')
        opts = [o for k, o in enumerate(opts) if o not in drop and (k == 0 or opts[k - 1] not in drop)]
        args += ['--load-state', load]
    i = 0
    while i < len(opts):
        o = opts[i]
        if o == '--input':
            args += [o, os.path.join(SCRIPTS, opts[i + 1])]
            i += 2
        elif o == '--snap':
            path = os.path.join(ROOT, opts[i + 1])
            if not os.path.exists(path):
                return name, 'no %s (make vectors)' % opts[i + 1]
            args += [o, path]
            i += 2
        else:
            args.append(o)
            i += 1
    r = subprocess.run(args, cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    if r.returncode != 0:
        return name, 'nhl94 failed (%d): %s' % (r.returncode, r.stderr.strip()[-300:]), None
    m = SAVED.search(r.stdout)
    if save and not m:
        return name, 'no state saved from frame %d' % save[0], None
    return name, None, int(m.group(2)) if m else None


def record(dirname, jobs, states=False, missing=False):
    """the runs into dirname (missing: only those it has none of); states: also save a state in the
    runs of STATES. Returns the golden lines the states were saved after (by name), or None on a
    failure."""
    os.makedirs(dirname, exist_ok=True)

    def one(r):
        save = (STATES[r[0]], os.path.join(dirname, r[0] + '.mdrs')) if states and r[0] in STATES else None
        return run(r[0], r[1], r[2], os.path.join(dirname, r[0] + '.golden'), save)
    runs = [r for r in RUNS if not (missing and os.path.exists(os.path.join(dirname, r[0] + '.golden')))]
    with ThreadPoolExecutor(jobs) as ex:
        results = list(ex.map(one, runs))
    bad = [(n, e) for n, e, _ in results if e]
    for n, e in bad:
        print('%s: %s' % (n, e))
    return None if bad else {n: line for n, _, line in results if line is not None}


def compare(want, got, skip=0, ram=True):
    """the first difference of got from want (after want's first skip lines; ram False: work RAM
    aside), or None"""
    with open(want) as f:
        a = f.read().split('\n')[skip:]
    with open(got) as f:
        b = f.read().split('\n')
    if not ram:
        a, b = [' '.join(x.split()[1:]) for x in a], [' '.join(x.split()[1:]) for x in b]
    for i, (x, y) in enumerate(zip(a, b)):
        if x != y:
            xs, ys = x.split(), y.split()
            if not ram:
                xs, ys = ['-'] + xs, ['-'] + ys
            cols = [COLUMNS[k] for k in range(min(len(xs), len(ys))) if xs[k] != ys[k]]
            return 'frame %d: %s differ' % (skip + i + 1, ', '.join(cols) or 'lines')
    if len(a) != len(b):
        return '%d frames, the golden run has %d' % (skip + len(b) - 1, skip + len(a) - 1)
    return None


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('action', choices=['record', 'check'])
    ap.add_argument('--dir', default=os.path.join(ROOT, 'out', 'golden'))
    ap.add_argument('--force', action='store_true', help='record over existing golden runs')
    ap.add_argument('--jobs', type=int, default=4)
    ap.add_argument('--build', help='the nhl94 to run (default build/nhl94)')
    ap.add_argument('--out', help="check: where the build's own runs go (default DIR-new)")
    a = ap.parse_args()
    global NHL94
    if a.build:                                           # (it exports the leagues the runs play too)
        NHL94 = leaguetest.NHL94 = os.path.abspath(a.build)
    if not os.path.exists(NHL94):
        sys.exit('no %s (make nhl94)' % os.path.relpath(NHL94, ROOT))
    leaguetest.export()                                   # (the leagues the runs play, from the ROM)
    leaguetest.thirty_two()
    if a.action == 'record':                              # (without --force: only the runs it has none of)
        missing = not a.force and os.path.exists(os.path.join(a.dir, RUNS[0][0] + '.golden'))
        todo = [r for r in RUNS if not (missing and os.path.exists(os.path.join(a.dir, r[0] + '.golden')))]
        ok = record(a.dir, a.jobs, missing=missing) is not None
        print('golden: %d runs recorded in %s' % (len(todo), os.path.relpath(a.dir, ROOT)) if ok else 'golden: FAILED')
        sys.exit(0 if ok else 1)
    new = a.out or a.dir.rstrip('/') + '-new'
    saved = record(new, a.jobs, states=True)
    if saved is None:
        sys.exit('golden: FAILED')
    bad = 0
    for name, frames, _ in RUNS:
        want = os.path.join(a.dir, name + '.golden')
        if not os.path.exists(want):
            print('%-15s no golden run (tools/golden.py record)' % name)
            bad += 1
            continue
        d = compare(want, os.path.join(new, name + '.golden'))
        print('%-15s %s' % (name, d or 'identical, %d frames' % frames))
        bad += d is not None
    print('golden: %d of %d runs identical' % (len(RUNS) - bad, len(RUNS)))
    lbad = 0
    for name, like in LIKE.items():
        d = compare(os.path.join(a.dir, like + '.golden'), os.path.join(new, name + '.golden'), ram=False)
        print('%-15s %s' % (name, d or 'as %s, frame for frame (work RAM aside)' % like))
        lbad += d is not None
    print('golden: %d of %d league runs as the original\'s' % (len(LIKE) - lbad, len(LIKE)))
    bad += lbad

    # the states, each loaded in a new process: the frames after it as the run's own
    def loaded(r):
        name = r[0]
        out = os.path.join(new, name + '.loaded')
        return run(name, r[1], r[2], out, load=os.path.join(new, name + '.mdrs'))
    with ThreadPoolExecutor(a.jobs) as ex:
        results = list(ex.map(loaded, [r for r in RUNS if r[0] in STATES]))
    sbad = 0
    for name, err, _ in results:
        line = saved[name]
        d = err or compare(os.path.join(new, name + '.golden'), os.path.join(new, name + '.loaded'), line)
        print('%-15s %s' % (name, d or 'a state loaded: the %d frames after it identical' % sum(1 for _ in open(os.path.join(new, name + '.loaded')))))
        sbad += d is not None
    print('golden: %d of %d states go on identically' % (len(STATES) - sbad, len(STATES)))
    kbad = keys(new)
    rbad = restart(new)
    sys.exit(1 if bad or sbad or kbad or rbad else 0)


def keys(dirname):
    """SAVE at handshake 1500, LOAD at 2400, REWIND held 3000-3150 (states_faceoff.txt)"""
    out = os.path.join(dirname, 'keys.golden')
    name, err, _ = run('keys', 3600, ['--snap', 'out/states/faceoff.snap', '--input', 'states_faceoff.txt',
                                      '--state', os.path.join(dirname, 'keys.mdrs')], out)
    if not err:
        lines = [l.split()[:4] for l in open(out)]     # (the sound aside for the rewind: it goes on)
        full = [l.split() for l in open(out)]
        # line h (from 0) is the h-th handshake's (line 0 the snapshot's own); a key's request comes
        # with the handshake after the one it is given at, and is carried out at the next wait: the
        # state saved before handshake 1502 is loaded before handshake 2402, and the rewind steps
        # back before handshakes 3002 to 3151, to the frames of handshakes 2101, 2098 ...
        same = sum(full[1502 + k] == full[2402 + k] for k in range(600))
        back = sum(lines[3002 + k] == lines[2101 - 3 * k] for k in range(150))
        err = None if same == 600 and back == 150 else \
            'the load: %d of 600 frames as after the save; the rewind: %d of 150 frames three back' % (same, back)
    print('%-15s %s' % ('keys', err or 'SAVE, LOAD: the 600 frames after the load as after the save; REWIND: 150 frames stepped back'))
    return err is not None


def restart(dirname):
    """power on, started again at frame 1500 in the same process: the 1000 frames after it are
    those of a fresh power on (the restart's own line is the fresh run's first)"""
    fresh, again = os.path.join(dirname, 'restart.fresh'), os.path.join(dirname, 'restart.golden')
    errs = [run('restart', 1000, [], fresh)[1], run('restart', 2500, ['--restart-at', '1500'], again)[1]]
    err = next((e for e in errs if e), None)
    if not err:
        a, b = [l.split() for l in open(fresh)], [l.split() for l in open(again)]
        same = sum(1 for k in range(len(a)) if 1499 + k < len(b) and a[k] == b[1499 + k])
        err = None if same == len(a) and len(a) >= 990 else '%d of %d frames after it as a fresh power on\'s' % (same, len(a))
    print('%-15s %s' % ('restart', err or 'the game started again: the %d frames after it as a fresh power on\'s' % sum(1 for _ in open(fresh))))
    return err is not None


if __name__ == '__main__':
    main()
