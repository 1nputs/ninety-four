#!/usr/bin/env python3
"""Where does a segment's vasm output fall behind the ROM? usage: find_shift.py SEGMENT (after check_segments.py)"""
import sys, json, pathlib, re
ROOT = pathlib.Path(__file__).resolve().parents[2]
name = sys.argv[1]
segs = json.load(open(ROOT / 'ref/NHL94Genesis/tools/segments.json')); org = int(segs[name], 16)
rom = (ROOT / 'NHL 94 (UE) [!].gen').read_bytes()
got = (ROOT / f'out/vasm/seg_{name}.bin').read_bytes(); ref = rom[org:org + len(got) + 64]
lst = (ROOT / f'out/vasm/seg_{name}.lst').read_text(errors='replace').splitlines()
shift, pts, j = 0, [], 0
while j < len(got) - 32:
    if got[j:j + 24] != ref[j + shift:j + shift + 24]:
        for s2 in range(shift + 1, shift + 48):
            if got[j:j + 24] == ref[j + s2:j + s2 + 24]:
                pts.append((org + j, s2 - shift)); shift = s2; break
    j += 1
print(f'{name}: ROM has {shift} more bytes; shift points (ROM addr of the vasm instruction that follows, extra bytes):')
for addr, extra in pts:
    print(f'--- {addr:#x} (+{extra}): ROM {ref[addr - org - 8:addr - org + 12].hex(" ")}')
    for l in lst:
        m = re.match(r'^00:([0-9A-F]{8})', l)
        if m and abs(int(m.group(1), 16) - addr) <= 8: print('   ', l.rstrip()[:120])
