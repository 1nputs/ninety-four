#!/usr/bin/env python3
"""Record a sound test vector (.sndtrace) for pulseman and the Z80 relay port: every YM2612
and PSG write with its time, the 68k's writes to Z80 RAM and the Z80 bus, and the oracle's audio
output (format: tools/hybrid/oracle_shim.c).

usage: sndtrace.py ROM --out FILE.sndtrace [--state STATE] [--frames N] [--skip N] [--script S]
                   [--fm-preamp P] [--psg-preamp P]   (percent, GPGX's defaults 100 / 150; 0 mutes
                                                       that chip in the recorded audio)
                   [--ym2612 mame|nuked]              (the oracle's FM core: MAME's, the default, or
                                                       Nuked-OPN2, pulseman's)
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
    ap.add_argument('--fm-preamp', type=int, default=100)
    ap.add_argument('--psg-preamp', type=int, default=150)
    ap.add_argument('--ym2612', choices=['mame', 'nuked'], default='mame')
    a = ap.parse_args()
    o = Oracle(options={'genesis_plus_gx_fm_preamp': str(a.fm_preamp), 'genesis_plus_gx_psg_preamp': str(a.psg_preamp),
                        'genesis_plus_gx_ym2612': 'nuked (ym2612)' if a.ym2612 == 'nuked' else 'mame (ym2612)'})
    o.load(a.rom)
    if a.state:
        o.unserialize(open(a.state, 'rb').read())
    rate = int(round(o.av.timing.sample_rate))
    o.lib.oracle_sndtrace_frame.argtypes = [C.c_char_p, C.c_uint32]
    script = parse_script(a.script); si = 0
    for f in range(a.skip + a.frames):
        if f == a.skip:
            power_on = not a.state and a.skip == 0         # every chip write is in the trace
            flags = int(power_on) | (2 if a.ym2612 == 'nuked' else 0)
            if not o.lib.oracle_sndtrace_open(a.out.encode(), rate, a.fm_preamp, a.psg_preamp, flags):
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
