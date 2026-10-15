#!/usr/bin/env python3
"""Record a VDP test vector (.vdptrace) for md-runtime: the 68k's VDP port accesses, DMA source
bytes, and per frame the VDP state and the picture (format: tools/hybrid/oracle_shim.c).

usage: vdptrace.py ROM --out FILE.vdptrace [--state STATE] [--frames N] [--script S] [--skip N]

Without --state the run starts at power on (EA logo, title, menus). --skip runs that many
frames first without tracing.
"""
import argparse, os, sys
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'hybrid'))
from oracle import Oracle  # noqa: E402
sys.path.insert(0, os.path.dirname(__file__))
from run import parse_script  # noqa: E402


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('rom')
    ap.add_argument('--out', required=True)
    ap.add_argument('--state', default='')
    ap.add_argument('--frames', type=int, default=600)
    ap.add_argument('--skip', type=int, default=0)
    ap.add_argument('--script', default='')
    a = ap.parse_args()
    o = Oracle()
    o.load(a.rom)
    if a.state:
        o.unserialize(open(a.state, 'rb').read())
    script = parse_script(a.script); si = 0
    for f in range(a.skip + a.frames):
        if f == a.skip:
            if not o.lib.oracle_vdptrace_open(a.out.encode()):
                sys.exit(f'cannot write {a.out}')
        while si < len(script) and script[si][0] <= f:
            o.set_buttons(0, script[si][1]); si += 1
        o.run()
    o.lib.oracle_vdptrace_close()
    print(f'{a.out}: {a.frames} frames, {os.path.getsize(a.out) // 1024} KB')


if __name__ == '__main__':
    main()
