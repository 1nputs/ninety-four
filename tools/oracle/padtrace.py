#!/usr/bin/env python3
"""Record a pad port test vector (.padtrace, version 2) for pulseman: the 68k's accesses to the I/O
ports ($A10000-$A1001F) and, every frame, the buttons held on pads 1-4 (3 and 4 with --four-way:
EA's 4 Way Play in the ports, attached before the first frame).

usage: padtrace.py ROM --out FILE.padtrace [--state STATE] [--frames N] [--four-way]
                   [--script S] [--script2 S] [--script3 S] [--script4 S]
"""
import argparse, os, sys
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'hybrid'))
from oracle import Oracle  # noqa: E402
sys.path.insert(0, os.path.dirname(__file__))
from run import parse_script  # noqa: E402

BITS = {'UP': 1, 'DOWN': 2, 'LEFT': 4, 'RIGHT': 8, 'B': 0x10, 'C': 0x20, 'A': 0x40, 'START': 0x80}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('rom')
    ap.add_argument('--out', required=True)
    ap.add_argument('--state', default='')
    ap.add_argument('--frames', type=int, default=600)
    ap.add_argument('--script', default='')
    ap.add_argument('--script2', default='')
    ap.add_argument('--script3', default='')
    ap.add_argument('--script4', default='')
    ap.add_argument('--four-way', action='store_true')
    a = ap.parse_args()
    o = Oracle()
    o.load(a.rom)
    if a.four_way:
        o.four_way()
    if a.state:
        o.unserialize(open(a.state, 'rb').read())
    scripts = [parse_script(s) for s in (a.script, a.script2, a.script3, a.script4)]
    at = [0, 0, 0, 0]
    held = [0, 0, 0, 0]
    if not o.lib.oracle_padtrace_open(a.out.encode(), 1 if a.four_way else 0):
        sys.exit(f'cannot write {a.out}')
    for f in range(a.frames):
        for p in range(4):
            s = scripts[p]
            while at[p] < len(s) and s[at[p]][0] <= f:
                o.set_buttons(p, s[at[p]][1]); held[p] = sum(BITS.get(b, 0) for b in s[at[p]][1]); at[p] += 1
        o.lib.oracle_padtrace_frame(*held)
        o.run()
    o.lib.oracle_padtrace_close()
    print(f'{a.out}: {a.frames} frames, {os.path.getsize(a.out) // 1024} KB')


if __name__ == '__main__':
    main()
