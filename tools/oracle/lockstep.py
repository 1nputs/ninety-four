#!/usr/bin/env python3
"""Record a lockstep vector (.lockstep) for the native game: at each vblank interrupt, where the
68k was, the pads, the HV counter reads since the last one, and hashes of work RAM and the VDP
(format: tools/hybrid/oracle_shim.c). The native game replays the pads and HV reads and checks
the hashes at each of its vblank waits (nhl94 [--snap SNAPSHOT] --lockstep FILE).

usage: lockstep.py ROM [--state STATE] --out FILE.lockstep [--frames N] [--script S] [--skip N]
       (the script's frames count from the state; faceoff.snap is faceoff.state; --skip runs N
       frames first, as snapshot.py --game-loop reported, so the recording starts at its snapshot:
       `nhl94 --snap SNAPSHOT --lockstep FILE`. Without --state the recording starts at power on,
       with save RAM blank ($FF), for `nhl94 --lockstep FILE`)
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
    ap.add_argument('--script2', default='')
    ap.add_argument('--script3', default='')
    ap.add_argument('--script4', default='')
    ap.add_argument('--four-way', action='store_true', help="EA's 4 Way Play in the ports from power on")
    ap.add_argument('--skip', type=int, default=0)
    ap.add_argument('--dump', nargs=2, metavar=('N', 'FILE'), help="work RAM at vblank N (from 1, as nhl94 counts) to FILE")
    a = ap.parse_args()
    o = Oracle()
    o.load(a.rom)
    if a.four_way:
        o.four_way()
    if a.state:
        o.unserialize(open(a.state, 'rb').read())
    syms = o.symbols
    scripts = [parse_script(s) for s in (a.script, a.script2, a.script3, a.script4)]
    at = [0, 0, 0, 0]
    held = [[], [], [], []]

    def press(f):
        for p in range(4):
            while at[p] < len(scripts[p]) and scripts[p][at[p]][0] <= f:
                held[p] = scripts[p][at[p]][1]; o.set_buttons(p, held[p]); at[p] += 1
    for f in range(a.skip):
        press(f)
        o.run()
    o.lib.oracle_lockstep_device(1 if a.four_way else 0)
    if not o.lib.oracle_lockstep_open(a.out.encode(), *(o.addr(n) & 0xFFFF for n in ('vcount', 'palcount', 'disflags'))):
        sys.exit(f'cannot write {a.out}')
    if a.dump:
        o.lib.oracle_lockstep_dump(int(a.dump[0]) - 1, a.dump[1].encode())
    for f in range(a.skip, a.skip + a.frames):
        press(f)
        o.lib.oracle_lockstep_buttons(*(sum(BITS.get(b, 0) for b in held[p]) for p in range(4)))
        o.lib.oracle_set_frame(f)
        o.run()
    o.lib.oracle_lockstep_close()
    print(f'{a.out}: {a.frames} frames')


if __name__ == '__main__':
    main()
