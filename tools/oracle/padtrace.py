#!/usr/bin/env python3
"""Record a pad port test vector (.padtrace) for pulseman: the 68k's accesses to the I/O ports
($A10000-$A1001F) and, every frame, the buttons held on pads 1 and 2.

usage: padtrace.py ROM --out FILE.padtrace [--state STATE] [--frames N] [--script S] [--script2 S]
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
    a = ap.parse_args()
    o = Oracle()
    o.load(a.rom)
    if a.state:
        o.unserialize(open(a.state, 'rb').read())
    s1, s2 = parse_script(a.script), parse_script(a.script2)
    i1 = i2 = 0
    held = [0, 0]
    if not o.lib.oracle_padtrace_open(a.out.encode()):
        sys.exit(f'cannot write {a.out}')
    for f in range(a.frames):
        while i1 < len(s1) and s1[i1][0] <= f:
            o.set_buttons(0, s1[i1][1]); held[0] = sum(BITS.get(b, 0) for b in s1[i1][1]); i1 += 1
        while i2 < len(s2) and s2[i2][0] <= f:
            o.set_buttons(1, s2[i2][1]); held[1] = sum(BITS.get(b, 0) for b in s2[i2][1]); i2 += 1
        o.lib.oracle_padtrace_frame(held[0], held[1])
        o.run()
    o.lib.oracle_padtrace_close()
    print(f'{a.out}: {a.frames} frames, {os.path.getsize(a.out) // 1024} KB')


if __name__ == '__main__':
    main()
