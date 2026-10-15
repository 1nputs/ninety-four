#!/usr/bin/env python3
"""Record a sound test vector (.sndtrace) for md-runtime and the Z80 relay port: every YM2612
and PSG write with its time, the 68k's writes to Z80 RAM and the Z80 bus, and the oracle's audio
output (format: tools/hybrid/oracle_shim.c).

usage: sndtrace.py ROM --out FILE.sndtrace [--state STATE] [--frames N] [--skip N] [--script S]
"""
import argparse, ctypes as C, os, sys
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
    rate = int(round(o.av.timing.sample_rate))
    o.lib.oracle_sndtrace_frame.argtypes = [C.c_char_p, C.c_uint32]
    script = parse_script(a.script); si = 0
    for f in range(a.skip + a.frames):
        if f == a.skip:
            if not o.lib.oracle_sndtrace_open(a.out.encode(), rate):
                sys.exit(f'cannot write {a.out}')
            o.audio_keep = bytearray()
        while si < len(script) and script[si][0] <= f:
            o.set_buttons(0, script[si][1]); si += 1
        o.run()
        if f >= a.skip:
            buf = bytes(o.audio_keep); o.audio_keep = bytearray()
            o.lib.oracle_sndtrace_frame(buf, len(buf) // 4)
    o.lib.oracle_sndtrace_close()
    print(f'{a.out}: {a.frames} frames, audio at {rate} Hz, {os.path.getsize(a.out) // 1024} KB')


if __name__ == '__main__':
    main()
