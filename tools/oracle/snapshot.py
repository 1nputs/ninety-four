#!/usr/bin/env python3
"""Write a snapshot of the oracle for the native runtime: 68k RAM, VRAM, CRAM, VSRAM, VDP
registers, Z80 RAM, and the sound chips (their registers, the Z80's place in its program).

usage: snapshot.py ROM --out FILE.snap --frames N [--script S]      (from power on: N frames)
       snapshot.py ROM --out FILE.snap --state STATE [--frames N]   (from a saved state)

The sound chips' registers are known only from power on (the shim follows every write from
there), so a snapshot from a saved state has none (flags bit 0 clear).

File: 'NHLS', version 2 (u32 le), RAM 64K (68k order), VRAM 64K (VDP order), CRAM 64 words,
VSRAM 40 words (big-endian bus words), 32 VDP registers, Z80 RAM 8K, then flags u32 (bit 0: the
chips' registers are valid), the YM2612's registers (part I 256 bytes, part II 256), the PSG's
eight registers (u16 le) and its latch u8, the Z80: PC u16, HL u16, C u8, bank u16 (68k address
>> 15), state u8 (bit 0 running, bit 1 bus taken).
faceoff.snap is power on, tools/oracle/scripts/boot_to_game.txt, 3401 frames (= faceoff.state).
"""
import argparse, ctypes as C, os, struct, sys
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'hybrid'))
from oracle import Oracle  # noqa: E402
sys.path.insert(0, os.path.dirname(__file__))
from run import parse_script  # noqa: E402


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('rom')
    ap.add_argument('--state', default='')
    ap.add_argument('--out', required=True)
    ap.add_argument('--frames', type=int, default=0)
    ap.add_argument('--script', default='')
    a = ap.parse_args()
    o = Oracle()
    o.load(a.rom)
    o.lib.oracle_install()
    if a.state:
        o.unserialize(open(a.state, 'rb').read())
    script = parse_script(a.script); si = 0
    for f in range(a.frames):
        while si < len(script) and script[si][0] <= f:
            o.set_buttons(0, script[si][1]); si += 1
        o.run()
    ram = (C.c_uint8 * 0x10000)(); o.lib.oracle_ram_get(ram)
    vram = (C.c_uint8 * 0x10000)(); cram = (C.c_uint8 * 128)(); vsram = (C.c_uint8 * 80)()
    reg = (C.c_uint8 * 32)(); zram = (C.c_uint8 * 0x2000)()
    o.lib.oracle_vdp_get(vram, cram, vsram, reg, zram)
    ym = (C.c_uint8 * 512)(); psg = (C.c_uint8 * 17)(); z80 = (C.c_uint8 * 8)()
    o.lib.oracle_chips_get(ym, psg)
    o.lib.oracle_z80_get(z80)
    with open(a.out, 'wb') as f:
        f.write(b'NHLS' + struct.pack('<I', 2))
        for buf in (ram, vram, cram, vsram, reg, zram):
            f.write(bytes(buf))
        f.write(struct.pack('<I', 0 if a.state else 1))
        for buf in (ym, psg, z80):
            f.write(bytes(buf))
    print(f'{a.out}: snapshot after {a.frames} frames')


if __name__ == '__main__':
    main()
