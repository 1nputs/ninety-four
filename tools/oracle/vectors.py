#!/usr/bin/env python3
"""The saved states, snapshots and test vectors the port is checked against, with the recipe of
each. They are derived from the ROM, so they are never committed: this table makes them again.

usage: vectors.py record [--root DIR] [--force] [--jobs N] [NAME ...]
       vectors.py check  [--root DIR] [NAME ...]
       vectors.py list

record makes whatever is missing (everything with --force), states and snapshots before the
vectors that start from them. check runs `build/nhl94 --lockstep` on every lockstep vector and
fails when one differs or stops early. NAME picks entries (a state, snapshot or vector name).
--root (default out) holds states/ and vectors/, so a whole set can be made again elsewhere and
checked there. md-runtime reads the same vectors (`make runtime-check`); `make relaytest` reads
the sound traces.
"""
import argparse, os, re, subprocess, sys, time
from concurrent.futures import ThreadPoolExecutor

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
ROM = os.environ.get('NHL94_ROM', 'NHL 94 (UE) [!].gen')
MDRT = os.environ.get('MDRT', os.path.join(REPO, '..', 'md-runtime'))
BOOT = 'boot_to_game.txt'          # Start at frames 900, 1500, 2100, 2700, 3300: into a faceoff
RANDOM = 'random_seed94.txt'       # the random input of the one-player sessions

# saved states (tools/oracle/run.py --save): the frame, the script, RAM pokes
STATES = [
    ('faceoff', dict(frames=3401, script=BOOT)),       # the original one-player game, 10 minute periods
    ('demo_faceoff', dict(frames=3400, script=BOOT,     # CPU against CPU, penalties, offsides and line changes on, 5 minutes
                          poke='2650:D04A=0,D056=1,D058=0,D050=0,C328=0;3350:C328=0,C320=FFFF')),
    ('demo1p_faceoff', dict(frames=3400, script=BOOT, poke='2650:D056=1,D058=0,D050=0')),   # the same, one human
]

# native snapshots (tools/oracle/snapshot.py): from power on (with the sound chips' registers) or a state
SNAPS = [
    ('faceoff', dict(frames=3401, script=BOOT)),       # = faceoff.state
    ('demo_faceoff', dict(state='demo_faceoff')),
    ('demo1p_faceoff', dict(state='demo1p_faceoff', script=RANDOM, game_loop=True)),   # 23 frames on: a game-loop wait
]

# test vectors (out/vectors): (name, tool, options). state: a saved state to start from; snap: the
# snapshot `nhl94 --lockstep` starts from (vectors from a state); script: a file in scripts/ or
# a literal script (it has a ':')
VECTORS = [
    # lockstep: the native game against the oracle at every vblank (nhl94 --lockstep)
    ('boot', 'lockstep', dict(frames=2500)),
    ('title_start', 'lockstep', dict(frames=1500, script='1200:START,1206:')),
    ('setup', 'lockstep', dict(frames=4500, script='setup_menu.txt')),
    ('setup_so', 'lockstep', dict(frames=1500, script='setup_shootout.txt')),
    ('name_entry', 'lockstep', dict(frames=2600, script='name_entry.txt')),
    ('playoffs', 'lockstep', dict(frames=1900, script='playoffs.txt')),
    ('boot_game', 'lockstep', dict(frames=9000, script='boot_to_game_full.txt')),
    ('full_game', 'lockstep', dict(frames=16000, script='full_game_30s.txt')),
    ('pause_menus', 'lockstep', dict(frames=8700, script='pause_menus.txt')),
    ('shootout_menus', 'lockstep', dict(frames=4900, script='shootout_menus.txt')),
    ('playoff_game', 'lockstep', dict(frames=16000, script='playoff_game_30s.txt')),
    ('records_game', 'lockstep', dict(frames=15700, script='records_game_30s.txt')),
    ('faceoff', 'lockstep', dict(frames=18000, state='faceoff', snap='faceoff', script=RANDOM)),
    ('pause_menus2', 'lockstep', dict(frames=22200, state='faceoff', snap='faceoff', script='pause_menus2.txt')),
    ('pause_menus3', 'lockstep', dict(frames=10600, state='demo_faceoff', snap='demo_faceoff', script='pause_menus3.txt')),
    ('replay_menu', 'lockstep', dict(frames=4700, state='faceoff', snap='faceoff', script='replay_menu.txt')),
    ('demo1p', 'lockstep', dict(frames=18000, skip=23, state='demo1p_faceoff', snap='demo1p_faceoff', script=RANDOM)),   # skip: to the snapshot's frame
    # VDP traces for md-runtime's vdp_trace (port accesses, DMA sources, the state and picture a frame)
    ('boot', 'vdptrace', dict(frames=90, skip=30, script=BOOT)),
    ('title', 'vdptrace', dict(frames=90, skip=800, script=BOOT)),
    ('setup', 'vdptrace', dict(frames=90, skip=1480, script=BOOT)),
    ('matchup', 'vdptrace', dict(frames=90, skip=2080, script=BOOT)),
    ('pregame', 'vdptrace', dict(frames=90, skip=2680, script=BOOT)),
    ('faceoff', 'vdptrace', dict(frames=120, state='faceoff')),
    ('play', 'vdptrace', dict(frames=120, skip=1200, state='demo_faceoff')),
    ('pause', 'vdptrace', dict(frames=150, skip=300, state='faceoff', script='320:START,330:,400:DOWN,405:,420:DOWN,425:')),
    ('vdptest', 'vdptrace', dict(frames=290, skip=30, rom=os.path.join(MDRT, 'build', 'vdptest.bin'),   # md-runtime's test ROM
                                      hint=' (in md-runtime: tests/rom/build.sh)')),
    # pad reads for md-runtime's pad_trace
    ('play', 'padtrace', dict(frames=900, state='faceoff', script=RANDOM,
                              script2='100:A,110:,200:B+UP,230:,300:C+LEFT,320:START,330:,400:RIGHT+DOWN,420:')),
    # sound: every chip write with its master clock (md-runtime's chip tests; the Z80 relay test)
    ('boot', 'sndtrace', dict(frames=3000, script=BOOT)),
    ('game', 'sndtrace', dict(frames=6000, script=BOOT)),
    ('game_psg', 'sndtrace', dict(frames=6000, script=BOOT, extra=['--fm-preamp', '0'])),
    ('game_fm', 'sndtrace', dict(frames=6000, script=BOOT, extra=['--psg-preamp', '0'])),
    ('game_fm_nuked', 'sndtrace', dict(frames=6000, script=BOOT, extra=['--psg-preamp', '0', '--ym2612', 'nuked'])),
    ('play', 'sndtrace', dict(frames=3000, state='faceoff', script=RANDOM)),
]


def script_text(s):
    if not s:
        return ''
    if ':' in s:
        return s
    with open(os.path.join(HERE, 'scripts', s)) as f:
        return ''.join(l.strip() for l in f if not l.startswith('#'))


def tool(name):
    return [sys.executable, os.path.join(HERE, name)]


class Item:
    def __init__(self, kind, name, opts, root):
        self.kind, self.name, self.o, self.root = kind, name, opts, root
        if kind == 'state':
            self.path = os.path.join(root, 'states', name + '.state')
        elif kind == 'snap':
            self.path = os.path.join(root, 'states', name + '.snap')
        else:
            self.path = os.path.join(root, 'vectors', '%s.%s' % (name, kind))
        self.needs = [os.path.join(root, 'states', opts['state'] + '.state')] if opts.get('state') else []
        if kind == 'lockstep' and opts.get('snap'):
            self.needs.append(os.path.join(root, 'states', opts['snap'] + '.snap'))

    def label(self):
        return os.path.relpath(self.path, REPO)

    def command(self):
        o = self.o
        rom = o.get('rom', ROM)
        if self.kind == 'state':
            cmd = tool('run.py') + [rom, '--frames', str(o['frames']), '--save', self.path]
            if o.get('poke'):
                cmd += ['--poke', o['poke']]
        elif self.kind == 'snap':
            cmd = tool('snapshot.py') + [rom, '--out', self.path]
            if o.get('frames'):
                cmd += ['--frames', str(o['frames'])]
            if o.get('game_loop'):
                cmd += ['--game-loop']
        else:
            cmd = tool(self.kind + '.py') + [rom, '--out', self.path, '--frames', str(o['frames'])]
            if o.get('skip'):
                cmd += ['--skip', str(o['skip'])]
            if o.get('script2'):
                cmd += ['--script2', o['script2']]
            cmd += o.get('extra', [])
        if o.get('state'):
            cmd += ['--load' if self.kind == 'state' else '--state', os.path.join(self.root, 'states', o['state'] + '.state')]
        if o.get('script'):
            cmd += ['--script', script_text(o['script'])]
        return cmd


def items(root):
    out = [Item('state', n, o, root) for n, o in STATES]
    out += [Item('snap', n, o, root) for n, o in SNAPS]
    out += [Item(k, n, o, root) for n, k, o in VECTORS]
    return out


def pick(all_items, names):
    if not names:
        return all_items
    chosen = [i for i in all_items if i.name in names or os.path.basename(i.path) in names]
    unknown = set(names) - {i.name for i in chosen} - {os.path.basename(i.path) for i in chosen}
    if unknown:
        sys.exit('unknown: ' + ', '.join(sorted(unknown)))
    return chosen


def record(args):
    todo = [i for i in pick(items(args.root), args.names) if args.force or not os.path.exists(i.path)]
    if not os.path.exists(ROM):
        sys.exit('no ROM at %s (set NHL94_ROM)' % ROM)
    for d in ('states', 'vectors'):
        os.makedirs(os.path.join(args.root, d), exist_ok=True)
    failed = []

    def run(i):
        if i.o.get('rom') and not os.path.exists(i.o['rom']):
            return i, 'skipped: no %s%s' % (os.path.relpath(i.o['rom'], REPO), i.o.get('hint', ''))
        missing = [n for n in i.needs if not os.path.exists(n)]
        if missing:
            return i, 'skipped: no %s' % ', '.join(os.path.relpath(m, REPO) for m in missing)
        t = time.time()
        p = subprocess.run(i.command(), cwd=REPO, capture_output=True, text=True)
        if p.returncode:
            return i, 'FAILED: ' + (p.stderr.strip().splitlines() or ['?'])[-1]
        return i, 'made in %.0f s' % (time.time() - t)

    # states and snapshots first (in that order), then the vectors, a few at a time (memory)
    for stage in (('state',), ('snap',), ('lockstep', 'vdptrace', 'padtrace', 'sndtrace')):
        batch = [i for i in todo if i.kind in stage]
        with ThreadPoolExecutor(max_workers=args.jobs) as ex:
            for i, how in ex.map(run, batch):
                print('%-40s %s' % (i.label(), how), flush=True)
                if how.startswith('FAILED') or (how.startswith('skipped') and not i.o.get('rom')):
                    failed.append(i)
    if not todo:
        print('everything is there (--force makes it again)')
    return 1 if failed else 0


def check(args):
    nhl94 = os.path.join(REPO, 'build', 'nhl94')
    if not os.path.exists(nhl94):
        sys.exit('no build/nhl94 (make nhl94)')
    chosen = [i for i in pick(items(args.root), args.names) if i.kind == 'lockstep']
    bad = 0
    for i in chosen:
        if not os.path.exists(i.path):
            print('%-16s MISSING (vectors.py record %s)' % (i.name, i.name))
            bad += 1
            continue
        cmd = [nhl94]
        if i.o.get('snap'):
            cmd += ['--snap', os.path.join(args.root, 'states', i.o['snap'] + '.snap')]
        cmd += ['--lockstep', i.path]
        t = time.time()
        p = subprocess.run(cmd, cwd=REPO, capture_output=True, text=True)
        last = (p.stdout.strip().splitlines() or ['(no output)'])
        summary = last[-1]
        m = re.match(r'lockstep: (\d+) of (\d+) vblanks: (\d+) of the game', summary)
        if p.returncode == 0 and m:
            print('%-16s ok: %s of %s vblanks, %s waits checked (%.0f s)' % (i.name, m.group(1), m.group(2), m.group(3), time.time() - t))
        else:
            bad += 1
            detail = next((l for l in last if 'differs' in l), summary)
            print('%-16s FAILED: %s' % (i.name, detail.replace('lockstep: ', '')))
    print('lockstep: %d of %d vectors pass' % (len(chosen) - bad, len(chosen)))
    return 1 if bad else 0


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('command', choices=['record', 'check', 'list'])
    ap.add_argument('names', nargs='*')
    ap.add_argument('--root', default='out')
    ap.add_argument('--force', action='store_true')
    ap.add_argument('--jobs', type=int, default=4)
    args = ap.parse_args()
    args.root = os.path.abspath(os.path.join(REPO, args.root)) if not os.path.isabs(args.root) else args.root
    if args.command == 'list':
        for i in items(args.root):
            print('%-40s %s' % (i.label(), 'there' if os.path.exists(i.path) else 'missing'))
        return 0
    return record(args) if args.command == 'record' else check(args)


if __name__ == '__main__':
    sys.exit(main())
