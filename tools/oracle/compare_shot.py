#!/usr/bin/env python3
"""Compare a picture of the native game (a PPM from `nhl94 --headless --frames N --shot F.ppm`)
with the oracle's after the same number of frames from the same state: both reduced to the VDP's
3 bits a colour channel (the oracle's frame buffer is RGB565, the native PPM uses md-runtime's
levels), so the comparison is of VDP colours.

usage: compare_shot.py ROM [--state STATE] --frames N SHOT.ppm [--around K]   (also N-K .. N+K;
       without --state from power on: the native game's frames are its vblank waits, so --around)
"""
import argparse, ctypes as C, os, sys
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'hybrid'))
from oracle import Oracle  # noqa: E402


def read_ppm(path):
    d = open(path, 'rb').read()
    parts = d.split(b'\n', 3)
    w, h = map(int, parts[1].split())
    px = parts[3]
    return w, h, [(px[3 * i] // 34, px[3 * i + 1] // 34, px[3 * i + 2] // 34) for i in range(w * h)]


def oracle_pixels(o):
    data, w, h, pitch = o.frame
    out = []
    for y in range(h):
        for x in range(w):
            v = data[y * pitch + 2 * x] | data[y * pitch + 2 * x + 1] << 8
            out.append(((v >> 11) >> 2, ((v >> 5) & 63) >> 3, (v & 31) >> 2))
    return w, h, out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('rom')
    ap.add_argument('--state', help='a saved state (default: power on)')
    ap.add_argument('--frames', type=int, required=True)
    ap.add_argument('--around', type=int, default=0)
    ap.add_argument('shot')
    a = ap.parse_args()
    w, h, ours = read_ppm(a.shot)
    o = Oracle()
    o.load(a.rom)
    if a.state:
        o.unserialize(open(a.state, 'rb').read())
    best = None
    for f in range(1, a.frames + a.around + 1):
        o.run()
        if f >= a.frames - a.around:
            ow, oh, theirs = oracle_pixels(o)
            if (ow, oh) != (w, h):
                print(f'frame {f}: oracle {ow}x{oh}, ours {w}x{h}')
                continue
            bad = sum(1 for p, q in zip(ours, theirs) if p != q)
            print(f'frame {f}: {bad} pixels differ of {w * h}')
            if best is None or bad < best[1]:
                best = (f, bad)
    sys.exit(0 if best and best[1] == 0 else 1)


if __name__ == '__main__':
    main()
