#!/usr/bin/env python3
"""Assemble every segment stub of the preprocessed tree with vasm and compare it with the ROM."""
import sys, json, subprocess, pathlib
ROOT = pathlib.Path(__file__).resolve().parents[2]
V = ROOT / 'ref/vasm-src/vasm/vasmm68k_mot'
rom = (ROOT / 'NHL 94 (UE) [!].gen').read_bytes()
segs = json.load(open(ROOT / 'ref/NHL94Genesis/tools/segments.json'))
order = sorted(segs.items(), key=lambda kv: int(kv[1], 16))
ends = {k: (int(order[i + 1][1], 16) if i + 1 < len(order) else 0x100000) for i, (k, v) in enumerate(order)}
src = ROOT / 'build/asm/src'
only = sys.argv[1:]
for name, org in order:
    if only and name not in only: continue
    stub = src / f'stubinc/{name}_stub.asm'
    if not stub.exists(): print(f'{name:<12} no stub'); continue
    out = ROOT / f'out/vasm/seg_{name}.bin'
    r = subprocess.run([str(V), '-m68000', '-Fbin', '-no-opt', '-DCHECKSUM=1', '-DREV=0', '-o', str(out), '-L', str(out.with_suffix('.lst')), str(stub)], capture_output=True, text=True, cwd=src)
    errs = [l for l in r.stderr.splitlines() if 'error' in l.lower()]
    if not out.exists() or errs: print(f'{name:<12} org {org:>8} ASSEMBLE FAILED: {errs[:1]}'); continue
    o = int(org, 16); ref = rom[o:ends[name]]; got = out.read_bytes()
    n = min(len(ref), len(got)); first = next((i for i in range(n) if ref[i] != got[i]), None)
    if len(ref) == len(got) and first is None: status = 'MATCH'
    elif len(ref) == len(got): status = f'same size, {sum(1 for i in range(n) if ref[i] != got[i])} bytes differ (encoding), first at {hex(o + first)}'
    else: status = f'size {len(got)} vs {len(ref)} ({len(got) - len(ref):+d}), first diff at {hex(o + first) if first is not None else None}'
    print(f'{name:<12} org {org:>8} {status}')
