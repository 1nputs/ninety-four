#!/usr/bin/env python3
"""Golden runs: the native game headless from power on (or a snapshot) with scripted input, a line
of hashes a frame (nhl94 --golden: work RAM, the VDP state and Z80 RAM at the game's wait, the
picture, the frame's sound). `record` keeps a build's runs in out/golden; `check` runs the build
again and compares, frame by frame, so a change to md-runtime or the native client can be shown
to change nothing it was not meant to. The runs use a fixed HV counter stand-in (main.c), so
changes to the runtime's HV model do not show; everything else must match bit for bit.

    tools/golden.py record [--force]     (make golden)
    tools/golden.py check                (make golden-check)
"""
import argparse
import os
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SCRIPTS = os.path.join(ROOT, 'tools', 'oracle', 'scripts')
NHL94 = os.path.join(ROOT, 'build', 'nhl94')

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
]
COLUMNS = ['work RAM', 'VDP state', 'Z80 RAM', 'picture', 'sound', 'samples']


def run(name, frames, opts, out):
    args = [NHL94, '--frames', str(frames), '--golden', out]
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
    r = subprocess.run(args, cwd=ROOT, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, text=True)
    if r.returncode != 0:
        return name, 'nhl94 failed (%d): %s' % (r.returncode, r.stderr.strip()[-300:])
    return name, None


def record(dirname, jobs):
    os.makedirs(dirname, exist_ok=True)
    with ThreadPoolExecutor(jobs) as ex:
        results = list(ex.map(lambda r: run(r[0], r[1], r[2], os.path.join(dirname, r[0] + '.golden')), RUNS))
    bad = [(n, e) for n, e in results if e]
    for n, e in bad:
        print('%s: %s' % (n, e))
    return not bad


def compare(want, got):
    with open(want) as f:
        a = f.read().split('\n')
    with open(got) as f:
        b = f.read().split('\n')
    for i, (x, y) in enumerate(zip(a, b)):
        if x != y:
            xs, ys = x.split(), y.split()
            cols = [COLUMNS[k] for k in range(min(len(xs), len(ys))) if xs[k] != ys[k]]
            return 'frame %d: %s differ' % (i + 1, ', '.join(cols) or 'lines')
    if len(a) != len(b):
        return '%d frames, the golden run has %d' % (len(b) - 1, len(a) - 1)
    return None


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('action', choices=['record', 'check'])
    ap.add_argument('--dir', default=os.path.join(ROOT, 'out', 'golden'))
    ap.add_argument('--force', action='store_true', help='record over existing golden runs')
    ap.add_argument('--jobs', type=int, default=4)
    a = ap.parse_args()
    if not os.path.exists(NHL94):
        sys.exit('no build/nhl94 (make nhl94)')
    if a.action == 'record':
        if os.path.exists(os.path.join(a.dir, RUNS[0][0] + '.golden')) and not a.force:
            sys.exit('%s has golden runs; --force to record over them' % a.dir)
        ok = record(a.dir, a.jobs)
        print('golden: %d runs recorded in %s' % (len(RUNS), os.path.relpath(a.dir, ROOT)) if ok else 'golden: FAILED')
        sys.exit(0 if ok else 1)
    new = a.dir.rstrip('/') + '-new'
    if not record(new, a.jobs):
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
    sys.exit(1 if bad else 0)


if __name__ == '__main__':
    main()
