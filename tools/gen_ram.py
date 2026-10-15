#!/usr/bin/env python3
"""Generate src/ram.h: the 68k work RAM ($FFFF0000-$FFFFFFFF) as one big-endian packed C struct.

Inputs: ref/NHL94Genesis/src/stubinc/ram_addrs.inc (names, addresses, comments),
        out/ramwidths.json (access widths inferred by tools/ramwidths.py),
        ref/NHL94Genesis/src/stubinc/equals.inc (sort object member offsets).
Every symbol becomes a field at its exact offset; gaps become padding; a symbol whose region
is a multiple of its access width becomes an array. Static asserts pin every offset.
"""
import re, sys, json, pathlib, collections

P = pathlib.Path(sys.argv[1]) if len(sys.argv) > 1 else pathlib.Path(__file__).resolve().parents[1]
REF = P / 'ref/NHL94Genesis/src'
rows = json.load(open(P / 'out/ramwidths.json'))
RAM_BASE, RAM_SIZE = 0xFFFF0000, 0x10000

CTYPE = {'b': 'uint8_t', 'w': 'uint16_t', 'l': 'uint32_t'}
# symbols whose width the access scan cannot see (indexed through an address register)
OVERRIDE = {'OOlistpos': 'w', 'Ylist': 'w', 'OOlist': 'b'}
# sort object members: positions are 16.16 fixed point (integer word first), velocities signed
SORT_DECL = {
    'Xpos': (['int16_t Xpos;', 'uint16_t Xfrac;'], 4), 'Ypos': (['int16_t Ypos;', 'uint16_t Yfrac;'], 4),
    'Zpos': (['int16_t Zpos;', 'uint16_t Zfrac;'], 4), 'OldXpos': (['int16_t OldXpos;', 'uint16_t OldXfrac;'], 4),
    'OldYpos': (['int16_t OldYpos;', 'uint16_t OldYfrac;'], 4), 'OldZpos': (['int16_t OldZpos;', 'uint16_t OldZfrac;'], 4),
    'facedir': (['int16_t facedir;', 'uint16_t _pad56;'], 4), 'nopuck': (['uint8_t nopuck[2];'], 2),
}
SORT_SIGNED = {'Xvel', 'Yvel', 'Zvel', 'impact', 'limpact', 'impactp', 'position', 'temp1', 'temp2', 'temp3', 'temp4', 'temp5',
               'radiusx', 'radiusy', 'Wallcos', 'Wallsin', 'SCnum', 'assnum'}
WSIZE = {'b': 1, 'w': 2, 'l': 4}


def pick_width(widths, gap):
    """Choose the scalar width for a region from its access counts and size."""
    acc = {k: v for k, v in widths.items() if k in WSIZE}
    if gap == 2 and ('w' in acc or not acc):
        return 'w'                      # flag words tested by byte still live as one word
    if gap == 1:
        return 'b'
    if gap == 4 and 'l' in acc:
        return 'l'                      # a long stored whole and read by halves (seed, pointers)
    if acc:
        return max(acc, key=acc.get)
    if gap == 4:
        return 'l'
    return 'b'


def cident(name):
    return re.sub(r'\W', '_', name)


# ---- sort object struct from equals.inc ---------------------------------------------------
eq = (REF / 'stubinc/equals.inc').read_text(errors='replace').splitlines()
members = []
in_block = False
for line in eq:
    if 'the sort object structure' in line:
        in_block = True; continue
    if in_block:
        m = re.match(r'^(\w+)\s+equ\s+\$([0-9A-Fa-f]+)\s*(?:;\s*(.*))?$', line)
        if m:
            members.append((m.group(1), int(m.group(2), 16), (m.group(3) or '').strip()))
        elif line.strip() == '' and members:
            break
# access widths for members: name(aN) / name(aN,dM.w)
mw = collections.defaultdict(collections.Counter)
ins_re = re.compile(r'^(?:[.\w]+:?)?\s+([a-z]+)(?:\.([bwls]))?\s+(.*?)\s*(?:;.*)?$')
byte_ops = {'btst', 'bset', 'bclr', 'bchg', 'sf', 'st', 'tas'}
names = {m[0] for m in members}
for f in sorted(REF.glob('*.asm')):
    for line in f.read_text(errors='replace').splitlines():
        m = ins_re.match(line)
        if not m or line.lstrip().startswith(';'):
            continue
        mn, sz, ops = m.groups()
        for om in re.finditer(r'(?<![\w.])(\w+)\(a\d', ops):
            n = om.group(1)
            if n in names:
                w = 'b' if mn in byte_ops else ({'s': 'w'}.get(sz, sz) if sz else '?')
                if mn in ('lea', 'pea'):
                    w = 'addr'
                mw[n][w] += 1

SCSTRUCT = 0x80
members.sort(key=lambda t: t[1])
sort_fields = []
pos = 0
for i, (n, off, com) in enumerate(members):
    if off < pos:
        continue  # overlapping alias (e.g. pflags2 inside a word); first one wins
    if off > pos:
        sort_fields.append(f'    uint8_t _pad{pos:02X}[{off - pos}];')
        pos = off
    nxt = members[i + 1][1] if i + 1 < len(members) else SCSTRUCT
    gap = max(1, min(nxt, SCSTRUCT) - off)
    if n in SORT_DECL:
        decls, size = SORT_DECL[n]
        sort_fields += ['    ' + d + ('  /* ' + com + ' */' if com and d is decls[0] else '') for d in decls]
        pos = off + size
        continue
    w = pick_width(mw.get(n, {}), gap)
    size = WSIZE[w]
    ct = ('int16_t' if n in SORT_SIGNED and w == 'w' else CTYPE[w])
    if gap >= 2 * size and gap % size == 0:
        sort_fields.append(f'    {ct} {cident(n)}[{gap // size}];{"  /* " + com + " */" if com else ""}')
        pos = off + gap
    else:
        sort_fields.append(f'    {ct} {cident(n)};{"  /* " + com + " */" if com else ""}')
        pos = off + size
if pos < SCSTRUCT:
    sort_fields.append(f'    uint8_t _pad{pos:02X}[{SCSTRUCT - pos}];')

# ---- RAM struct ---------------------------------------------------------------------------
fields, asserts = [], []
pos = 0
inram = [r for r in rows if RAM_BASE <= r['addr'] < RAM_BASE + RAM_SIZE]
inram.sort(key=lambda r: r['addr'])
for i, r in enumerate(inram):
    off = r['addr'] - RAM_BASE
    name = cident(r['name'])
    nxt = inram[i + 1]['addr'] - RAM_BASE if i + 1 < len(inram) else RAM_SIZE
    if off < pos:
        asserts.append(f'/* {name} at 0x{off:04X} overlaps the previous field; use RAM_OFF_{name} */')
        continue
    if off > pos:
        fields.append(f'    uint8_t _pad{pos:04X}[0x{off - pos:X}];')
        pos = off
    gap = nxt - off
    com = r.get('comment', '').replace('*/', '* /')
    cm = f'  /* ${r["addr"]:08X}{": " + com if com else ""} */'
    if name == 'SortCords':
        n = gap // SCSTRUCT
        fields.append(f'    struct SortObj SortCords[{n}];{cm}')
        pos = off + n * SCSTRUCT
        if pos < nxt:
            fields.append(f'    uint8_t _pad{pos:04X}[0x{nxt - pos:X}];'); pos = nxt
    else:
        w = OVERRIDE.get(r['name']) or pick_width(r['widths'], gap)
        size = WSIZE[w]
        if gap >= 2 * size and gap % size == 0 and gap > 4:
            fields.append(f'    {CTYPE[w]} {name}[0x{gap // size:X}];{cm}')
            pos = off + gap
        else:
            fields.append(f'    {CTYPE[w]} {name};{cm}')
            pos = off + size
    asserts.append(f'_Static_assert(offsetof(struct Ram, {name}) == 0x{off:04X}, "{name}");')
if pos < RAM_SIZE:
    fields.append(f'    uint8_t _pad{pos:04X}[0x{RAM_SIZE - pos:X}];')

offs = '\n'.join(f'#define RAM_OFF_{cident(r["name"])} 0x{r["addr"] - RAM_BASE:04X}' for r in inram)
sort_offs = '\n'.join(f'#define SO_{cident(n)} 0x{off:02X}' for n, off, _ in members)

hdr = f'''/* Generated by tools/gen_ram.py from the NHL94Genesis RAM map. Do not edit by hand.
 *
 * The Genesis work RAM ($FFFF0000-$FFFFFFFF) as one struct whose bytes are laid out exactly
 * as the 68k sees them: big-endian scalars, no alignment padding. Field access is ordinary C;
 * the compiler emits the byte swaps. This lets the port share RAM images with the oracle
 * emulator byte for byte. GCC only (scalar_storage_order). Taking the address of a scalar
 * field is not allowed by GCC in a reverse-storage-order struct; index arrays or use
 * RAM_OFF_* with ram_ptr() instead.
 */
#pragma once
#include <stdint.h>
#include <stddef.h>

#define RAM_BASE 0x{RAM_BASE:X}u
#define RAM_SIZE 0x{RAM_SIZE:X}u
#define BE_PACKED __attribute__((scalar_storage_order("big-endian"), packed))

#define SCSTRUCT 0x{SCSTRUCT:X}
#define Sortobjs 16
#define puckscnum 14
/* one sorted graphics object (players, puck, referee ...): SortCords entry */
struct BE_PACKED SortObj {{
{chr(10).join(sort_fields)}
}};
_Static_assert(sizeof(struct SortObj) == SCSTRUCT, "SortObj size");

struct BE_PACKED Ram {{
{chr(10).join(fields)}
}};
_Static_assert(sizeof(struct Ram) == RAM_SIZE, "Ram size");
{chr(10).join(asserts)}

extern struct Ram g;

/* raw byte view, for the oracle comparison and lea-style code */
static inline uint8_t *ram_ptr(uint32_t addr) {{ return (uint8_t *)&g + (addr & (RAM_SIZE - 1)); }}
static inline struct SortObj *sortobj(int n) {{ return (struct SortObj *)ram_ptr(RAM_BASE + RAM_OFF_SortCords_PLACEHOLDER + n * SCSTRUCT); }}

/* byte offsets of every RAM symbol */
{offs}

/* sort object member offsets */
{sort_offs}
'''
hdr = hdr.replace('RAM_OFF_SortCords_PLACEHOLDER', f'0x{[r for r in inram if r["name"]=="SortCords"][0]["addr"] - RAM_BASE:04X}')
(P / 'src').mkdir(exist_ok=True)
(P / 'src/ram.h').write_text(hdr)
print(f'wrote src/ram.h: {len(fields)} fields, {len(members)} SortObj members, {len(asserts)} asserts')
