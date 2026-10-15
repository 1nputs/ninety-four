#!/usr/bin/env python3
"""Record function-level captures from the oracle during scripted play.

usage: capture.py ROM --fn NAME[,NAME...] --out FILE [--state STATE] [--frames N] [--script S]
                  [--max N] [--stride N] [--coverage FILE] [--from N]
"""
import argparse, os, sys
sys.path.insert(0, os.path.dirname(__file__))
from oracle import Oracle, ROOT  # noqa: E402
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
    ap.add_argument('--max-log', type=int, default=128, help='MB of call log per record; a longer capture is dropped (memory)')
    ap.add_argument('--spread', action='store_true', help='capture call numbers 1,2,3,4,6,8,12,16,... of each routine instead of the first N')
    ap.add_argument('--from', dest='from_frame', type=int, default=0, help='capture from this frame on')
    a = ap.parse_args()

    o = Oracle()
    o.load(a.rom)
    if a.state:
        o.unserialize(open(a.state, 'rb').read())
    fns = []
    for f in filter(None, a.fn.split(',')):
        try:
            o.addr(f); fns.append(f)
        except ValueError:
            print(f'skipping {f}: no address known')
    if not a.from_frame:
        for f in fns:
            o.bp(f)
    if not a.no_calls:
        print(f'logging nested calls into {o.fn_set_all()} routines')
    if a.coverage:
        o.coverage(True)
    o.capture_spread(a.spread)
    o.lib.oracle_capture_log_limit(a.max_log)
    o.capture_open(a.out, a.max, a.stride)
    script = parse_script(a.script); si = 0
    pokes = parse_pokes(a.poke)
    for f in range(a.frames):
        if a.from_frame and f == a.from_frame:
            for fn in fns:
                o.bp(fn)
        while si < len(script) and script[si][0] <= f:
            o.set_buttons(0, script[si][1]); si += 1
        for ad, v in pokes.get(f, ()):
            o.poke_w(ad, v)
        o.run()
    n = o.capture_close()
    for f in fns:
        print(f'{f:<24} {hex(o.addr(f))}: {o.hits(f)} calls, {o.captured(f)} captured')
    print(f'{n} records -> {a.out} ({os.path.getsize(a.out) // 1024} KB), {o.lib.oracle_capture_dropped()} dropped over {a.max_log} MB of log')
    if a.coverage:
        cov = o.coverage_get()
        open(a.coverage, 'wb').write(bytes(cov))
        executed = sum(1 for v in cov if v)
        print(f'coverage: {executed} distinct PCs executed -> {a.coverage}')
    # the summary reads only the record headers: parsing every call of a big capture in Python
    # takes gigabytes
    import struct
    n_rec = n_irq = 0
    with open(a.out, 'rb') as f:
        while True:
            hdr = f.read(24)
            if len(hdr) < 24:
                break
            _magic, total, _pc, _frame, flags, _calls = struct.unpack('<6I', hdr)
            n_rec += 1
            n_irq += bool(flags & 1)
            f.seek(total - 24, 1)
    print(f'records: {n_rec}, interrupted by hblank/vblank: {n_irq}')


if __name__ == '__main__':
    main()
