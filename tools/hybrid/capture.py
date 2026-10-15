#!/usr/bin/env python3
"""Record function-level captures from the oracle during scripted play.

usage: capture.py ROM --fn NAME[,NAME...] --out FILE [--state STATE] [--frames N] [--script S]
                  [--max N] [--stride N] [--coverage FILE]
"""
import argparse, os, sys
sys.path.insert(0, os.path.dirname(__file__))
from oracle import Oracle, read_captures, ROOT  # noqa: E402
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'oracle'))
from run import parse_script, parse_pokes  # noqa: E402


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('rom')
    ap.add_argument('--fn', required=True, help='routine names (symbols) or hex addresses, comma separated')
    ap.add_argument('--out', required=True)
    ap.add_argument('--state', default=os.path.join(ROOT, 'out', 'states', 'faceoff.state'))
    ap.add_argument('--frames', type=int, default=300)
    ap.add_argument('--script', default='')
    ap.add_argument('--max', type=int, default=32, help='captures per routine')
    ap.add_argument('--stride', type=int, default=1, help='capture every Nth call')
    ap.add_argument('--coverage', default='', help='write per-PC execution counts (raw uint32[1M]) here')
    ap.add_argument('--no-calls', action='store_true', help='do not log nested calls')
    ap.add_argument('--poke', default='', help='RAM words to write: "FRAME:ADDR=VAL,...;FRAME:..." (hex)')
    ap.add_argument('--spread', action='store_true', help='capture call numbers 1,2,3,4,6,8,12,16,... of each routine instead of the first N')
    a = ap.parse_args()

    o = Oracle()
    o.load(a.rom)
    if a.state:
        o.unserialize(open(a.state, 'rb').read())
    fns = []
    for f in filter(None, a.fn.split(',')):
        try:
            o.bp(f); fns.append(f)
        except ValueError:
            print(f'skipping {f}: no address known')
    if not a.no_calls:
        print(f'logging nested calls into {o.fn_set_all()} routines')
    if a.coverage:
        o.coverage(True)
    o.capture_spread(a.spread)
    o.capture_open(a.out, a.max, a.stride)
    script = parse_script(a.script); si = 0
    pokes = parse_pokes(a.poke)
    for f in range(a.frames):
        while si < len(script) and script[si][0] <= f:
            o.set_buttons(0, script[si][1]); si += 1
        for ad, v in pokes.get(f, ()):
            o.poke_w(ad, v)
        o.run()
    n = o.capture_close()
    for f in fns:
        print(f'{f:<24} {hex(o.addr(f))}: {o.hits(f)} calls, {o.captured(f)} captured')
    print(f'{n} records -> {a.out} ({os.path.getsize(a.out) // 1024} KB)')
    if a.coverage:
        cov = o.coverage_get()
        open(a.coverage, 'wb').write(bytes(cov))
        executed = sum(1 for v in cov if v)
        print(f'coverage: {executed} distinct PCs executed -> {a.coverage}')
    recs = read_captures(a.out)
    bad = [r for r in recs if r.interrupted]
    print(f'records: {len(recs)}, interrupted by hblank/vblank: {len(bad)}')
    if recs:
        r = recs[0]
        print(f'first: pc={r.pc:#x} frame={r.frame} regs_in a7={r.regs_in["a7"]:#x} sp_out={r.regs_out["a7"]:#x} pc_out={r.regs_out["pc"]:#x} changed_bytes={len(r.changed())} nested_calls={len(r.calls)}')


if __name__ == '__main__':
    main()
