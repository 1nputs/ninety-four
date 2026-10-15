#!/usr/bin/env python3
"""Dump one capture record: registers in / out, the nested calls with their names, optional RAM words.
usage: recdump.py FILE.cap INDEX [--ram ADDR[,ADDR...]] [--obj N] [--team home|away]"""
import argparse, json, os, sys
sys.path.insert(0, os.path.dirname(__file__))
from oracle import read_captures, ROOT  # noqa: E402

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('file'); ap.add_argument('index', type=int)
    ap.add_argument('--ram', default='', help='hex addresses (low 16 bits) to print as words, in and out')
    ap.add_argument('--obj', type=int, default=-1, help='print the main fields of sort object N')
    a = ap.parse_args()
    syms = json.load(open(os.path.join(ROOT, 'build', 'asm', 'symbols.json')))
    syms = syms.get('symbols', syms)
    inv = {}
    for k, v in syms.items():
        ad = v if isinstance(v, int) else v.get('addr')
        inv.setdefault(ad, k)
    r = read_captures(a.file)[a.index]
    print(f'record {a.index}: {inv.get(r.pc, "?")} pc={r.pc:x} frame={r.frame} flags={r.flags} calls={len(r.calls)}')
    print('in :', ' '.join(f'{n}={v:08x}' for n, v in r.regs_in.items()))
    print('out:', ' '.join(f'{n}={v:08x}' for n, v in r.regs_out.items()))
    for c in r.calls:
        if c.pc & 0x80000000:
            print(f'  io read {c.pc & 0xFFFFFF:06x}')
            continue
        ci = c.regs_in if isinstance(c.regs_in, dict) else dict(zip(r.regs_in.keys(), c.regs_in))
        co = c.regs_out if isinstance(c.regs_out, dict) else dict(zip(r.regs_in.keys(), c.regs_out))
        nd = len(c.diff) if hasattr(c.diff, '__len__') else c.diff
        print(f'  {"  " * (c.depth & 0xFF)}{inv.get(c.pc, "?"):22s} d0={ci["d0"]:08x} d1={ci["d1"]:08x} d2={ci["d2"]:08x} d4={ci["d4"]:08x} a0={ci["a0"]:08x} a2={ci["a2"]:08x} a3={ci["a3"]:08x} -> d0={co["d0"]:08x} d1={co["d1"]:08x} diff={nd}')
    def w(ram, off): return ram[off] << 8 | ram[off + 1]
    for h in filter(None, a.ram.split(',')):
        o = int(h, 16) & 0xFFFF
        print(f'  RAM {o:04X}: in {w(r.ram_in, o):04x} out {w(r.ram_out, o):04x}')
    if a.obj >= 0:
        o = 0xB04A + a.obj * 0x80
        for nm, off in [('Xpos', 0), ('Ypos', 0x14), ('Xvel', 0x28), ('Yvel', 0x2A), ('position', 0x34), ('assnum', 0x36), ('temp1', 0x40), ('temp2', 0x42), ('temp3', 0x44), ('temp4', 0x46), ('SCnum', 0x52), ('facedir', 0x54), ('SPA', 0x58), ('SPAnum', 0x5A)]:
            print(f'  obj{a.obj}.{nm:9s} in {w(r.ram_in, o + off):04x} out {w(r.ram_out, o + off):04x}')
        for nm, off in [('pflags', 0x62), ('pflags2', 0x63), ('pflags3', 0x64), ('pnum', 0x66)]:
            print(f'  obj{a.obj}.{nm:9s} in {r.ram_in[o + off]:02x} out {r.ram_out[o + off]:02x}')

if __name__ == '__main__':
    main()
