#!/usr/bin/env python3
"""Record a lockstep vector (.lockstep) for the native game: at each vblank interrupt, where the
68k was, the pads, the HV counter reads since the last one, and hashes of work RAM and the VDP
(format: tools/hybrid/oracle_shim.c). The native game replays the pads and HV reads and checks
the hashes at each of its vblank waits (nhl94 --lockstep FILE).

usage: lockstep.py ROM [--state STATE] --out FILE.lockstep [--frames N] [--script S] [--skip N]
       (the script's frames count from the state; faceoff.snap is faceoff.state; --skip runs N
       frames first, as snapshot.py --game-loop reported, so the recording starts at its snapshot.
       Without --state the recording starts at power on, with save RAM blank ($FF), for
       `nhl94 --boot --lockstep FILE`)
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
    ap.add_argument('--state', help='a saved state (default: power on)')
    ap.add_argument('--out', required=True)
    ap.add_argument('--frames', type=int, default=600)
    ap.add_argument('--script', default='')
    ap.add_argument('--skip', type=int, default=0)
    ap.add_argument('--dump', nargs=2, metavar=('N', 'FILE'), help="work RAM at vblank N (from 1, as nhl94 counts) to FILE")
    a = ap.parse_args()
    o = Oracle()
    o.load(a.rom)
    if a.state:
        o.unserialize(open(a.state, 'rb').read())
    syms = o.symbols
    script = parse_script(a.script); si = 0; held = []
    for f in range(a.skip):
        while si < len(script) and script[si][0] <= f:
            held = script[si][1]; o.set_buttons(0, held); si += 1
        o.run()
    if not o.lib.oracle_lockstep_open(a.out.encode(), *(o.addr(n) & 0xFFFF for n in ('vcount', 'palcount', 'disflags'))):
        sys.exit(f'cannot write {a.out}')
    if a.dump:
        o.lib.oracle_lockstep_dump(int(a.dump[0]) - 1, a.dump[1].encode())
    for f in range(a.skip, a.skip + a.frames):
        while si < len(script) and script[si][0] <= f:
            held = script[si][1]; o.set_buttons(0, held); si += 1
        o.lib.oracle_lockstep_buttons(sum(BITS.get(b, 0) for b in held))
        o.lib.oracle_set_frame(f)
        o.run()
    o.lib.oracle_lockstep_close()
    print(f'{a.out}: {a.frames} frames')


if __name__ == '__main__':
    main()
