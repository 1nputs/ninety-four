#!/usr/bin/env python3
"""Write a snapshot of the oracle for the native runtime: 68k RAM, VRAM, CRAM, VSRAM, VDP
registers and Z80 RAM, from a saved state (optionally run a few frames first).

usage: snapshot.py ROM --state STATE --out FILE.snap [--frames N]

File: 'NHLS', version 1 (u32 le), RAM 64K (68k order), VRAM 64K (VDP order), CRAM 64 words,
VSRAM 40 words (big-endian bus words), 32 VDP registers, Z80 RAM 8K.
"""
import argparse, ctypes as C, os, struct, sys
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'hybrid'))
from oracle import Oracle  # noqa: E402


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('rom')
    ap.add_argument('--state', required=True)
    ap.add_argument('--out', required=True)
    ap.add_argument('--frames', type=int, default=0)
    a = ap.parse_args()
    o = Oracle()
    o.load(a.rom)
    o.unserialize(open(a.state, 'rb').read())
    for _ in range(a.frames):
        o.run()
    ram = (C.c_uint8 * 0x10000)(); o.lib.oracle_ram_get(ram)
    vram = (C.c_uint8 * 0x10000)(); cram = (C.c_uint8 * 128)(); vsram = (C.c_uint8 * 80)()
    reg = (C.c_uint8 * 32)(); zram = (C.c_uint8 * 0x2000)()
    o.lib.oracle_vdp_get(vram, cram, vsram, reg, zram)
    with open(a.out, 'wb') as f:
        f.write(b'NHLS' + struct.pack('<I', 1))
        for buf in (ram, vram, cram, vsram, reg, zram):
            f.write(bytes(buf))
    print(f'{a.out}: snapshot after {a.frames} frames')


if __name__ == '__main__':
    main()
