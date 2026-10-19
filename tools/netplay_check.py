#!/usr/bin/env python3
"""Netplay checked (make netplay-check): two headless nhl94 processes over the loopback, each
playing its own pad's script (the host pad 1, the guest pad 2) through a simulated network
(nhl94 --net-sim: delay, jitter, so packets overtake each other, loss, duplicates), against a solo
run of the same buttons: the scripts moved later by the input delay (netplay's buttons reach the
game delay + 1 frames after they are pressed, a solo script's one). Every frame each peer finally
played (the last line --hashes wrote for it: a rollback plays frames again) must be the solo
run's: work RAM, the VDP's state and the Z80's RAM, bit for bit. Each peer must play out its
frames, its own desync checks agreeing. Save RAM is blank in all three, as in the window's
sessions.

The game is Two - Head to Head from power on (pad 1 picks it on the setup screen, then plays as
full_game_30s.txt does: 30 second periods); pad 2 plays at random (its own seed) from the faceoff.

  h2h      host and join, 40 ms each way +-10, 5% lost, 1% twice, input delay 2: power on to the
           second period
  league   both connect (--peer), 25 ms +-25, 10% lost, delay 1: the 32-team league
           (out/leagues/thirty-two, from make golden) by the Arcade preset, power on to the first
           intermission
  delay0   host and join, 15 ms +-5, 2% lost, delay 0 (a rollback for nearly every change of
           buttons): the whole game, to the setup screen again

    tools/netplay_check.py [NAME...]
"""
import os
import random
import re
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
NHL94 = os.path.join(ROOT, 'build', 'nhl94')
SCRIPTS = os.path.join(ROOT, 'tools', 'oracle', 'scripts')
OUT = os.path.join(ROOT, 'out', 'netplay')

# name, frames, the game's options, how they connect, delay, the network (ms, ms, %, %), pad 2's seed
SCENARIOS = [
    ('h2h', 6000, [], 'join', 2, '40,10,5,1', 1),
    ('league', 7000, ['--league', 'out/leagues/thirty-two', '--rule', 'preset=arcade'], 'peer', 1, '25,25,10,1', 2),
    ('delay0', 16000, [], 'join', 0, '15,5,2,0', 3),
]
NEEDS = {'league': 'out/leagues/thirty-two'}


def entries(path):
    """a script's entries, (frame, buttons) in order"""
    out = []
    with open(path) as f:
        for line in f:
            for item in line.split('#')[0].replace(',', '\n').split('\n'):
                item = item.strip()
                if ':' in item:
                    frame, rest = item.split(':', 1)
                    out.append((int(frame), rest))
    return out


def write(path, items, by=0):
    with open(path, 'w') as f:
        f.write(','.join('%d:%s' % (frame + by, b) for frame, b in items) + '\n')


def pad1():
    """full_game_30s.txt with Players set to Two - Head to Head on the setup screen (down to it,
    right three times)"""
    items = entries(os.path.join(SCRIPTS, 'full_game_30s.txt'))
    extra = [(1240, 'DOWN'), (1244, '')]
    for k in range(3):
        extra += [(1252 + 12 * k, 'RIGHT'), (1256 + 12 * k, '')]
    return [e for e in items if e[0] < 1240] + extra + [e for e in items if e[0] >= 1240]


def pad2(seed, last):
    """random play from the first faceoff (frame 2600) to last"""
    r = random.Random(seed)
    dirs = ['', 'UP', 'DOWN', 'LEFT', 'RIGHT', 'UP+LEFT', 'UP+RIGHT', 'DOWN+LEFT', 'DOWN+RIGHT']
    out, frame = [], 2600
    while frame < last:
        b = [d for d in [r.choice(dirs)] if d] + ([r.choice('ABC')] if r.random() < 0.4 else [])
        out.append((frame, '+'.join(b)))
        frame += r.randint(4, 40)
    return out


def last_lines(path):
    """the last line of each frame: work RAM, VDP, Z80 RAM"""
    lines = {}
    with open(path) as f:
        for line in f:
            x = line.split()
            if len(x) == 4:
                lines[int(x[0])] = x[1:4]
    return lines


def scenario(k, s):
    name, frames, opts, how, delay, sim, seed = s
    if name in NEEDS and not os.path.isdir(os.path.join(ROOT, NEEDS[name])):
        return name, 'no %s (make golden)' % NEEDS[name], []
    d = os.path.join(OUT, name)
    os.makedirs(d, exist_ok=True)
    p1, p2 = pad1(), pad2(seed, frames)
    write(os.path.join(d, 'pad1.txt'), p1)
    write(os.path.join(d, 'pad2.txt'), p2)
    write(os.path.join(d, 'solo1.txt'), p1, delay)
    write(os.path.join(d, 'solo2.txt'), p2, delay)
    base = [NHL94, '--headless', '--frames', str(frames), '--input', os.path.join(d, 'pad1.txt'),
            '--input2', os.path.join(d, 'pad2.txt'), '--delay', str(delay), '--net-sim', sim] + opts
    pa, pb = 7570 + 2 * k, 7571 + 2 * k
    if how == 'join':
        roles = [['--host', str(pa)], ['--join', '127.0.0.1:%d' % pa]]
    else:
        roles = [['--peer', '127.0.0.1:%d' % pb, '--port', str(pa)], ['--peer', '127.0.0.1:%d' % pa, '--port', str(pb)]]
    limit = frames / 20 + 120
    procs = []
    for who, role in zip(('a', 'b'), roles):
        err = open(os.path.join(d, who + '.log'), 'w')
        procs.append((subprocess.Popen(base + role + ['--hashes', os.path.join(d, who + '.hashes')], cwd=ROOT,
                                       stdout=subprocess.DEVNULL, stderr=err), err))
    srm = os.path.join(d, 'solo.srm')                     # (blank: none there before the run)
    if os.path.exists(srm):
        os.remove(srm)
    solo = subprocess.run([NHL94, '--headless', '--frames', str(frames), '--input', os.path.join(d, 'solo1.txt'),
                           '--input2', os.path.join(d, 'solo2.txt'), '--sram', srm,
                           '--hashes', os.path.join(d, 'solo.hashes')] + opts, cwd=ROOT, stdout=subprocess.DEVNULL,
                          stderr=subprocess.PIPE, text=True, timeout=limit)
    bad = []
    for (p, err), who in zip(procs, 'ab'):
        try:
            code = p.wait(timeout=limit)
        except subprocess.TimeoutExpired:
            p.kill()
            code = 'a hang'
        err.close()
        if code != 0:
            bad.append('%s failed (%s)' % (who, code))
    if solo.returncode != 0:
        return name, 'the solo run failed: ' + solo.stderr[-200:], []
    ref = last_lines(os.path.join(d, 'solo.hashes'))
    report = []
    for who in 'ab':
        log = open(os.path.join(d, who + '.log')).read()
        got = last_lines(os.path.join(d, who + '.hashes'))
        same = sum(1 for f in range(1, frames + 1) if got.get(f) == ref.get(f))
        first = next((f for f in range(1, frames + 1) if got.get(f) != ref.get(f)), None)
        m = re.search(r'frame (\d+), (\d+) rollbacks \((\d+) frames played again, at most (\d+)\), (\d+) frames given back, '
                      r'(\d+) desync checks, round trip (-?\d+) ms', log)
        role = 'host' if 'hosting' in log else 'guest'
        if same != frames:
            bad.append('%s: frame %s differs from the solo run' % (role, first))
        if not m or int(m.group(1)) < frames:
            bad.append('%s did not play out: %s' % (role, log.strip().splitlines()[-1] if log.strip() else 'no log'))
        elif int(m.group(6)) < frames // 60 - 3:
            bad.append('%s: only %s desync checks' % (role, m.group(6)))
        if 'desync at' in log or 'lost' in log:
            bad.append('%s: %s' % (role, [l for l in log.splitlines() if 'desync at' in l or 'lost' in l][0]))
        if m:
            report.append('%s %d/%d as the solo run, %s rollbacks (%s frames again, at most %s), %s given back, %s checks, %s ms'
                          % (role, same, frames, m.group(2), m.group(3), m.group(4), m.group(5), m.group(6), m.group(7)))
    return name, '; '.join(bad) if bad else None, report


def main():
    if not os.path.exists(NHL94):
        sys.exit('no build/nhl94 (make nhl94)')
    names = sys.argv[1:] or [s[0] for s in SCENARIOS]
    todo = [(k, s) for k, s in enumerate(SCENARIOS) if s[0] in names]
    with ThreadPoolExecutor(len(todo)) as ex:
        results = list(ex.map(lambda ks: scenario(*ks), todo))
    failed = 0
    for name, err, report in results:
        print('%-7s %s' % (name, 'ok' if not err else 'FAILED: ' + err))
        for line in report:
            print('        ' + line)
        failed += err is not None
    print('netplay: %s' % ('all %d sessions as their solo runs' % len(results) if not failed else '%d failed' % failed))
    sys.exit(1 if failed else 0)


if __name__ == '__main__':
    main()
