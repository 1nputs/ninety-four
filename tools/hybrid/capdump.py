#!/usr/bin/env python3
"""Summarise a capture file: per record, the routine, frame, registers and the named RAM
variables that changed between entry and return. usage: capdump.py FILE [--max N] [--full]"""
import argparse, bisect, json, os, sys
sys.path.insert(0, os.path.dirname(__file__))
from oracle import read_captures, load_symbols, ROOT  # noqa: E402

RAM_BASE = 0xFFFF0000
rows = json.load(open(os.path.join(ROOT, 'out', 'ramwidths.json')))
ram = sorted((r['addr'] - RAM_BASE, r['name']) for r in rows if RAM_BASE <= r['addr'] < RAM_BASE + 0x10000)
offs = [o for o, _ in ram]
sym = load_symbols()
by_addr = {v: k for k, v in sym.items()}


def varname(off):
    i = bisect.bisect_right(offs, off) - 1
    if i < 0:
        return f'+{off:04X}'
    o, n = ram[i]
    return n if o == off else f'{n}+{off - o:X}'


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('file'); ap.add_argument('--max', type=int, default=10); ap.add_argument('--full', action='store_true')
    a = ap.parse_args()
    recs = read_captures(a.file)
    print(f'{len(recs)} records')
    for r in recs[:a.max]:
        name = by_addr.get(r.pc, f'{r.pc:#x}')
        ch = r.changed()
        groups = {}
        for o in ch:
            groups.setdefault(varname(o).split('+')[0], []).append(o)
        regs_in = ' '.join(f'{k}={v:x}' for k, v in r.regs_in.items() if k in ('d0', 'd1', 'd2', 'd3', 'a0', 'a3', 'a7'))
        print(f'[{name} frame {r.frame}{" IRQ" if r.interrupted else ""}] in: {regs_in}')
        dregs = {k: (r.regs_in[k], r.regs_out[k]) for k in r.regs_in if r.regs_in[k] != r.regs_out[k] and k not in ('pc',)}
        print('   regs changed:', ' '.join(f'{k}:{i:x}->{o:x}' for k, (i, o) in dregs.items()) or 'none')
        print(f'   {len(ch)} RAM bytes changed in {len(groups)} vars:', ', '.join(f'{k}({len(v)})' for k, v in list(groups.items())[:25]))
        if r.calls:
            seq = ' '.join((f'io:{c.pc & 0xFFFFFF:X}={c.regs_in["d0"]:X}' if c.is_io else by_addr.get(c.pc, f'{c.pc:#x}') + f'[{len(c.diff)}]') for c in r.calls[:30])
            print(f'   {len(r.calls)} nested calls: {seq}{" ..." if len(r.calls) > 30 else ""}')
        if a.full:
            for o in ch[:60]:
                print(f'     {varname(o):<28} {RAM_BASE + o:08X}: {r.ram_in[o]:02X} -> {r.ram_out[o]:02X}')


if __name__ == '__main__':
    main()
