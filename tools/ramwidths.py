#!/usr/bin/env python3
"""Infer the access width of every RAM variable from how the asm rebuild uses it.

Reads ref/NHL94Genesis/src/stubinc/ram_addrs.inc for names and addresses, scans every
src/*.asm for operands of the form (name).w / (name+N).w / (name).l and counts the size
suffix of the instruction. Writes out/ramwidths.json and prints a summary.
The C RAM struct generator uses the result to choose uint8_t / uint16_t / uint32_t.
"""
import re, sys, json, pathlib, collections

ref = pathlib.Path(sys.argv[1])
out = pathlib.Path(sys.argv[2]) if len(sys.argv) > 2 else None

names = {}
comments = {}
for line in (ref / 'src/stubinc/ram_addrs.inc').read_text(errors='replace').splitlines():
    m = re.match(r'^(\w+)\s+equ\s+\$([0-9A-Fa-f]+)\s*(?:;\s*(.*))?$', line)
    if m:
        names[m.group(1)] = int(m.group(2), 16)
        if m.group(3):
            comments[m.group(1)] = m.group(3).strip()

# instruction: optional label, mnemonic, optional size, operands
ins_re = re.compile(r'^(?:[.\w]+:?)?\s+([a-z]+)(?:\.([bwls]))?\s+(.*?)\s*(?:;.*)?$')
# a RAM operand: (name).w  (name+4).w  (name).l  -- capture name and displacement
opnd_re = re.compile(r'\(([A-Za-z_]\w*)([+-]\$?[0-9A-Fa-f]+)?\)\.[wl]')
byte_ops = {'btst', 'bset', 'bclr', 'bchg', 'sf', 'st', 'scc', 'scs', 'seq', 'sne', 'spl', 'smi',
            'sge', 'slt', 'sgt', 'sle', 'shi', 'sls', 'svc', 'svs', 'tas', 'nbcd', 'abcd', 'sbcd'}
addr_ops = {'lea', 'pea', 'jsr', 'jmp', 'movem'}

widths = collections.defaultdict(collections.Counter)   # name -> Counter of 'b','w','l','?'
for f in sorted((ref / 'src').glob('*.asm')):
    for line in f.read_text(errors='replace').splitlines():
        if line.lstrip().startswith(';'):
            continue
        m = ins_re.match(line)
        if not m:
            continue
        mn, sz, ops = m.group(1), m.group(2), m.group(3)
        for om in opnd_re.finditer(ops):
            name, disp = om.group(1), om.group(2)
            if name not in names:
                continue
            key = name if not disp else f'{name}{disp}'
            if mn in addr_ops:
                w = 'addr'
            elif mn in byte_ops:
                w = 'b'
            elif mn == 'movem':
                w = 'addr'
            elif sz:
                w = {'s': 'w'}.get(sz, sz)
            else:
                w = '?'
            widths[key][w] += 1

order = sorted(names.items(), key=lambda kv: kv[1])
rows = []
for i, (name, addr) in enumerate(order):
    nxt = order[i + 1][1] if i + 1 < len(order) else 0x100000000
    gap = nxt - addr
    c = widths.get(name, collections.Counter())
    data = {k: v for k, v in c.items() if k not in ('addr',)}
    dom = max(data, key=data.get) if data else None
    mixed = len([k for k in data if k in 'bwl']) > 1
    sub = {k: dict(v) for k, v in widths.items() if k.startswith(name) and k != name and re.match(re.escape(name) + r'[+-]', k)}
    rows.append(dict(name=name, addr=addr, gap=gap, widths=dict(c), dominant=dom, mixed=mixed,
                     offsets=sub, comment=comments.get(name, '')))

tot = len(rows)
unref = sum(1 for r in rows if not r['widths'])
mixed = sum(1 for r in rows if r['mixed'])
pure = collections.Counter(r['dominant'] for r in rows if r['dominant'] and not r['mixed'])
print(f'RAM symbols: {tot}  unreferenced-by-direct-access: {unref}  mixed-width: {mixed}  pure: {dict(pure)}')
print('span: $%X-$%X' % (rows[0]['addr'], rows[-1]['addr']))
print()
print('mixed-width symbols (name addr gap widths):')
for r in rows:
    if r['mixed']:
        print(f"  {r['name']:<22} ${r['addr']:08X} gap={r['gap']:<6} {r['widths']}")
print()
print('large regions (gap >= 0x40):')
for r in rows:
    if r['gap'] >= 0x40 and r['gap'] < 0x10000:
        print(f"  {r['name']:<22} ${r['addr']:08X} gap=${r['gap']:<5X} {r['widths']}  {r['comment'][:60]}")
if out:
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(rows, indent=1))
    print(f'\nwrote {out}')
