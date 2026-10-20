#!/usr/bin/env python3
"""Netplay checked (make netplay-check): headless nhl94 processes over the loopback, a host and
its guests (two to four machines), each playing its own pads' scripts through a simulated network
(nhl94 --net-sim: delay, jitter, so packets overtake each other, loss, duplicates), against a solo
run of the same buttons: the scripts moved later by the input delay (netplay's buttons reach the
game delay + 1 frames after they are pressed, a solo run's one). Every frame each machine finally
played (the last line --hashes wrote for it: a rollback plays frames again) must be the solo
run's: work RAM, the VDP's state, the Z80's RAM and save RAM, bit for bit. Each must play out its
frames, its own desync checks agreeing. Only the host is given the game's settings (its league,
rules and save RAM: user records, from a run of records_game_30s.txt), as the solo run is: the
session sends them to the guests, whose own are none (no league, the Classic rules, blank save
RAM). Each machine is given every pad's script and plays those of the pads the host gives it (in
the order the guests come, the host's first).

Two players: Two - Head to Head from power on (pad 1 picks it on the setup screen, then plays as
full_game_30s.txt does: 30 second periods); pad 2 plays at random (its own seed) from the faceoff.
More: the lockstep vector four_way_game's four scripts (four_way_p1.txt to p4: Players: Four on
the setup screen), EA's 4 Way Play in the ports (the solo run's --four-way).

  h2h      two machines that both connect (--peer), 40 ms each way +-10, 5% lost, 1% twice, input
           delay 2: power on to the second period (both given the settings: either may host)
  league   host and join, 25 ms +-25, 10% lost, delay 1: the host's 32-team league
           (out/leagues/thirty-two, from make golden) by the Arcade preset, power on to the first
           intermission
  delay0   host and join, 15 ms +-5, 2% lost, delay 0 (a rollback for nearly every change of
           buttons): the whole game, to the setup screen again
  twotwo   two machines with two players each (--here 2, the host's --players 4), 30 ms +-10, 5%
           lost, delay 2
  four     four machines with a player each (the host's --players 4: three guests), 30 ms +-10,
           5% lost, delay 2
  twoone   three machines: the host's two players and a player at each of two guests, 25 ms +-15,
           5% lost, delay 1
  onethree two machines: the host's one player and a guest's three, 30 ms +-10, 5% lost, delay 2

    tools/netplay_check.py [NAME...]
"""
import os
import random
import re
import shutil
import socket
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
NHL94 = os.path.join(ROOT, 'build', 'nhl94')
SCRIPTS = os.path.join(ROOT, 'tools', 'oracle', 'scripts')
OUT = os.path.join(ROOT, 'out', 'netplay')

# name, frames, the host's options (the game's settings), how they connect, delay, the network (ms, ms,
# %, %), pad 2's seed (None: the four 4 Way Play scripts), the players at each machine (the host's first)
SCENARIOS = [
    ('h2h', 6000, [], 'peer', 2, '40,10,5,1', 1, [1, 1]),
    ('league', 7000, ['--league', 'out/leagues/thirty-two', '--rule', 'preset=arcade'], 'join', 1, '25,25,10,1', 2, [1, 1]),
    ('delay0', 16000, [], 'join', 0, '15,5,2,0', 3, [1, 1]),
    ('twotwo', 6000, [], 'join', 2, '30,10,5,1', None, [2, 2]),
    ('four', 6000, [], 'join', 2, '30,10,5,1', None, [1, 1, 1, 1]),
    ('twoone', 6000, [], 'join', 1, '25,15,5,1', None, [2, 1, 1]),
    ('onethree', 6000, [], 'join', 2, '30,10,5,1', None, [1, 3]),
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
    """the last line of each frame: work RAM, VDP, Z80 RAM, save RAM (not the simulation's hash
    after them: a solo run's rules part names its league's folder, a session's does not)"""
    lines = {}
    with open(path) as f:
        for line in f:
            x = line.split()
            if len(x) == 6:
                lines[int(x[0])] = x[1:5]
    return lines


RECORDS = os.path.join(OUT, 'records.srm')


def records():
    """save RAM with user records in it (records_game_30s.txt played), made once: None, or what failed"""
    if os.path.exists(RECORDS):
        return None
    os.makedirs(OUT, exist_ok=True)
    part = RECORDS + '.part'
    if os.path.exists(part):
        os.remove(part)
    r = subprocess.run([NHL94, '--headless', '--frames', '15700', '--input', os.path.join(SCRIPTS, 'records_game_30s.txt'),
                        '--sram', part], cwd=ROOT, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, text=True)
    if r.returncode != 0 or not os.path.exists(part):
        return 'records_game_30s.txt: ' + r.stderr[-200:]
    os.replace(part, RECORDS)
    return None


def fresh(path, like=None):
    """path as like is (a copy), or not there"""
    if os.path.exists(path):
        os.remove(path)
    if like:
        shutil.copyfile(like, path)
    return path


def free_ports(n):
    """n UDP ports the system has free now (other checks may run on this machine: no fixed numbers)"""
    socks = [socket.socket(socket.AF_INET, socket.SOCK_DGRAM) for _ in range(n)]
    for x in socks:
        x.bind(('127.0.0.1', 0))
    ports = [x.getsockname()[1] for x in socks]
    for x in socks:
        x.close()
    return ports


def scenario(k, s):
    name, frames, opts, how, delay, sim, seed, layout = s
    if name in NEEDS and not os.path.isdir(os.path.join(ROOT, NEEDS[name])):
        return name, 'no %s (make golden)' % NEEDS[name], []
    d = os.path.join(OUT, name)
    os.makedirs(d, exist_ok=True)
    for f in os.listdir(d):                               # (the last run's)
        if f.endswith(('.hashes', '.log', '.srm')):
            os.remove(os.path.join(d, f))
    if seed is None:                                      # (4 Way Play: the lockstep vector's four scripts)
        pads = [entries(os.path.join(SCRIPTS, 'four_way_p%d.txt' % (j + 1))) for j in range(4)]
    else:
        pads = [pad1(), pad2(seed, frames)]
    ins, solo_ins = [], []
    for j, p in enumerate(pads):                          # (each machine plays its own pads' scripts)
        flag = '--input' if j == 0 else '--input%d' % (j + 1)
        write(os.path.join(d, 'pad%d.txt' % (j + 1)), p)
        write(os.path.join(d, 'solo%d.txt' % (j + 1)), p, delay)
        ins += [flag, os.path.join(d, 'pad%d.txt' % (j + 1))]
        solo_ins += [flag, os.path.join(d, 'solo%d.txt' % (j + 1))]
    base = [NHL94, '--headless', '--frames', str(frames)] + ins + ['--delay', str(delay), '--net-sim', sim]
    total = sum(layout)
    who = 'abcd'[:len(layout)]
    ports = free_ports(2)
    procs = []
    for j, w in enumerate(who):
        here = ['--here', str(layout[j])] if layout[j] > 1 else []
        if how == 'peer':                                 # (two that both connect: both given the settings, either may host)
            role = ['--peer', '127.0.0.1:%d' % ports[1 - j], '--port', str(ports[j])]
            mine = opts + ['--sram', fresh(os.path.join(d, w + '.srm'), RECORDS)]
        elif j == 0:
            role = ['--host', str(ports[0])] + (['--players', str(total)] if total > 2 else [])
            mine = opts + ['--sram', fresh(os.path.join(d, w + '.srm'), RECORDS)]
        else:
            role = ['--join', '127.0.0.1:%d' % ports[0]]
            mine = ['--sram', fresh(os.path.join(d, w + '.srm'))]
        err = open(os.path.join(d, w + '.log'), 'w')
        procs.append((subprocess.Popen(base + role + here + mine + ['--hashes', os.path.join(d, w + '.hashes')], cwd=ROOT,
                                       stdout=subprocess.DEVNULL, stderr=err), err, w))
    limit = frames / 20 + 120
    srm = fresh(os.path.join(d, 'solo.srm'), RECORDS)
    solo = subprocess.run([NHL94, '--headless', '--frames', str(frames)] + solo_ins + ['--sram', srm,
                           '--hashes', os.path.join(d, 'solo.hashes')] + opts + (['--four-way'] if total > 2 else []),
                          cwd=ROOT, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, text=True, timeout=limit)
    bad = []
    for p, err, w in procs:
        try:
            code = p.wait(timeout=limit)
        except subprocess.TimeoutExpired:
            p.kill()
            code = 'a hang'
        err.close()
        if code != 0:
            bad.append('%s failed (%s)' % (w, code))
    if solo.returncode != 0:
        return name, 'the solo run failed: ' + solo.stderr[-200:], []
    ref = last_lines(os.path.join(d, 'solo.hashes'))
    report = []
    for w in who:
        log = open(os.path.join(d, w + '.log')).read()
        got = last_lines(os.path.join(d, w + '.hashes'))
        same = sum(1 for f in range(1, frames + 1) if got.get(f) == ref.get(f))
        first = next((f for f in range(1, frames + 1) if got.get(f) != ref.get(f)), None)
        m = re.search(r'frame (\d+), (\d+) rollbacks \((\d+) frames played again, at most (\d+)\), (\d+) frames given back, '
                      r'(\d+) desync checks, round trip (-?\d+) ms', log)
        pm = re.search(r'(hosting|joined), (pads? [0-9]+(?:(?:, | and )[0-9]+)*)', log)
        role = '%s (%s)' % ('host' if pm and pm.group(1) == 'hosting' else 'guest', pm.group(2) if pm else '?')
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
    why = records()
    if why:
        sys.exit('netplay: no save RAM with records: ' + why)
    names = sys.argv[1:] or [s[0] for s in SCENARIOS]
    todo = [(k, s) for k, s in enumerate(SCENARIOS) if s[0] in names]
    with ThreadPoolExecutor(min(len(todo), 4)) as ex:   # (four sessions at once at most: theirs are many processes)
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
