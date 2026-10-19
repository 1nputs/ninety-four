#!/usr/bin/env python3
"""Generate src/ram.h and src/ram.c: the 68k work RAM ($FFFF0000-$FFFFFFFF) as one packed C
struct at the 68000's offsets, its scalars in the host's byte order, and the map from each machine
byte to the host byte holding it (with the machine image's pack and unpack).

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
# symbols kept inside another structure, so no field of their own (a byte area named name_): the
# game reads and writes them by address, RAM(name), and their bytes follow that structure
BY_ADDRESS = {'puckc': "the puck's sort object's newpos and newpnum, read by the 68000 as one word"}
# the fields the game names (g.x in its code): only these must agree with the sort object views
NAMED = set()
for _f in [*(P / 'src').glob('*.c'), *(P / 'src').glob('*.h'), *(P / 'native').glob('*.c'), *(P / 'test').glob('*.c')]:
    if _f.name not in ('ram.h', 'ram.c'):
        NAMED.update(re.findall(r'\bg\.(\w+)', _f.read_text(errors='replace')))
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
    elif r['name'] in BY_ADDRESS:
        name = name + '_'
        fields.append(f'    uint8_t {name}[0x{gap:X}];  /* ${r["addr"]:08X}: {BY_ADDRESS[r["name"]]} (RAM({r["name"]})) */')
        pos = off + gap
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

CFILE = '''/* Generated by tools/gen_ram.py. Do not edit by hand.
 *
 * Work RAM itself, and the map from a machine byte to the host byte holding it. */
#include <stddef.h>
#include "ram.h"

struct Ram g;

/* RAM's 16- and 32-bit scalars, as runs (first byte, count, size): struct Ram's fields, and struct
 * SortObj's wherever the game reads one (the sorted objects, the pads block, sso) */
static const struct { uint16_t at, n; uint8_t size; } runs[] = {@RUNS@
};

uint16_t ram_host_[RAM_SIZE];

/* each machine byte's host byte: the same offset, but inside a scalar the host's byte order */
__attribute__((constructor)) static void ram_map(void)
{
    for (uint32_t a = 0; a < RAM_SIZE; a++) ram_host_[a] = (uint16_t)a;
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    for (size_t r = 0; r < sizeof runs / sizeof runs[0]; r++)
        for (uint32_t k = 0; k < runs[r].n; k++) {
            uint32_t at = runs[r].at + k * runs[r].size;
            for (uint32_t i = 0; i < runs[r].size; i++) ram_host_[at + i] = (uint16_t)(at + runs[r].size - 1 - i);
        }
#endif
}

void ram_pack(uint8_t out[RAM_SIZE])
{
    const uint8_t *b = (const uint8_t *)&g;
    for (uint32_t a = 0; a < RAM_SIZE; a++) out[a] = b[ram_host_[a]];
}

void ram_unpack(const uint8_t in[RAM_SIZE])
{
    uint8_t *b = (uint8_t *)&g;
    for (uint32_t a = 0; a < RAM_SIZE; a++) b[ram_host_[a]] = in[a];
}
'''

# ---- the map: every scalar of RAM, as the game names it ------------------------------------
# struct SortObj's fields wherever the game reads one (bytes too: a byte read by name must not lie
# inside a word): the 16 sorted objects (SortCords, the puck and its shadow past the array), the
# pads block (the pads, the joypad structs and the replay camera, $1C apart) and sso's arrows
# ($14 apart). Then struct Ram's fields: inside those regions only a field's first element (the
# scalar the game names, as g.puckvz[0]: the rest of such an array is the object's own fields),
# which must agree; elsewhere every element. No two views may disagree.
SORT_VIEWS = [('SortCords', 16, SCSTRUCT), ('pads', 7, 0x1C), ('sso', 4, 0x14)]
SZ = {'uint8_t': 1, 'int8_t': 1, 'uint16_t': 2, 'int16_t': 2, 'uint32_t': 4, 'int32_t': 4}


def layout(lines):
    """(name, offset, size, count) of each declared field, in order"""
    out, at = [], 0
    for l in lines:
        m = re.match(r'\s+(struct SortObj|u?int(?:8|16|32)_t)\s+(\w+)(?:\[(0x[0-9A-Fa-f]+|\d+)\])?;', l)
        if m:
            size = SCSTRUCT if m.group(1) == 'struct SortObj' else SZ[m.group(1)]
            count = int(m.group(3), 0) if m.group(3) else 1
            out.append((m.group(2), at, size, count))
            at += size * count
    return out


unit_of, conflicts = {}, []
def unit(start, size, what):
    for b in range(start, start + size):
        old = unit_of.get(b)
        if old and old[:2] != (start, size):
            conflicts.append(f'RAM ${b:04X}: {what} ({size} bytes at ${start:04X}) against {old[2]} ({old[1]} at ${old[0]:04X})')
            return
    for b in range(start, start + size):
        unit_of[b] = (start, size, what)


sortobj = layout(sort_fields)
base_of = {r['name']: r['addr'] - RAM_BASE for r in inram}
in_view = set()
for region, count, stride in SORT_VIEWS:
    in_view.update(range(base_of[region], base_of[region] + count * stride))
    for k in range(count):
        for n, at, size, cnt in sortobj:
            for e in range(cnt):
                if at + (e + 1) * size <= stride:
                    unit(base_of[region] + k * stride + at + e * size, size, f'{region} {k} {n}')
for n, at, size, count in layout(fields):
    if size == SCSTRUCT or n.startswith('_pad'):
        continue                                          # (padding: nothing names it)
    if at in in_view and n not in NAMED:
        continue                                          # (reached by address only: the view's layout)
    for k in range(1 if at in in_view else count):
        unit(at + k * size, size, n)
if conflicts:
    raise SystemExit('views of RAM that disagree:\n  ' + '\n  '.join(conflicts))
starts = sorted({(s, z) for s, z, _ in unit_of.values() if z > 1})
runs = []
for s, z in starts:
    if runs and runs[-1][2] == z and runs[-1][0] + runs[-1][1] * z == s:
        runs[-1][1] += 1
    else:
        runs.append([s, 1, z])

offs = '\n'.join(f'#define RAM_OFF_{cident(r["name"])} 0x{r["addr"] - RAM_BASE:04X}' for r in inram)
sort_offs = '\n'.join(f'#define SO_{cident(n)} 0x{off:02X}' for n, off, _ in members)

hdr = f'''/* Generated by tools/gen_ram.py from the NHL94Genesis RAM map. Do not edit by hand.
 *
 * The Genesis work RAM ($FFFF0000-$FFFFFFFF) as one struct at the 68000's offsets (packed, no
 * alignment padding), its scalars in the host's byte order: native C, for any compiler. The game
 * reads it by its fields; src/mem.h reads and writes it by 68000 address through ram_host_, the
 * host byte of each machine byte (inside a 16- or 32-bit scalar the host's order: src/ram.c).
 * ram_pack makes the machine's image of it (the 68000's big-endian bytes) and ram_unpack takes
 * one back, for what compares or keeps RAM as the console has it: the oracle, the capture test,
 * lockstep, golden runs, save states.
 */
#pragma once
#include <stdint.h>
#include <stddef.h>

#define RAM_BASE 0x{RAM_BASE:X}u
#define RAM_SIZE 0x{RAM_SIZE:X}u
#define RAM_PACKED __attribute__((packed))

#define SCSTRUCT 0x{SCSTRUCT:X}
#define Sortobjs 16
#define puckscnum 14
/* one sorted graphics object (players, puck, referee ...): SortCords entry */
struct RAM_PACKED SortObj {{
{chr(10).join(sort_fields)}
}};
_Static_assert(sizeof(struct SortObj) == SCSTRUCT, "SortObj size");

struct RAM_PACKED Ram {{
{chr(10).join(fields)}
}};
_Static_assert(sizeof(struct Ram) == RAM_SIZE, "Ram size");
{chr(10).join(asserts)}

extern struct Ram g;

/* each machine byte's host byte (src/ram.c), and the machine's image of RAM made and taken back */
extern uint16_t ram_host_[RAM_SIZE];
void ram_pack(uint8_t out[RAM_SIZE]);
void ram_unpack(const uint8_t in[RAM_SIZE]);

/* sorted object n (the puck and its shadow lie past SortCords' 14) */
static inline struct SortObj *sortobj(int n) {{ return (struct SortObj *)((uint8_t *)&g + RAM_OFF_SortCords_PLACEHOLDER + n * SCSTRUCT); }}

/* byte offsets of every RAM symbol */
{offs}

/* sort object member offsets */
{sort_offs}
'''
hdr = hdr.replace('RAM_OFF_SortCords_PLACEHOLDER', f'0x{[r for r in inram if r["name"]=="SortCords"][0]["addr"] - RAM_BASE:04X}')
(P / 'src').mkdir(exist_ok=True)
(P / 'src/ram.h').write_text(hdr)
print(f'wrote src/ram.h: {len(fields)} fields, {len(members)} SortObj members, {len(asserts)} asserts')
rows_c = ''.join(('\n    ' if i % 6 == 0 else ' ') + '{ 0x%04X, %d, %d },' % (s, n, z) for i, (s, n, z) in enumerate(runs))
cfile = CFILE.replace('@RUNS@', rows_c)
(P / 'src/ram.c').write_text(cfile)
print(f'wrote src/ram.c: {sum(z for _, z in starts)} bytes in {len(starts)} scalars of 2 or 4, {len(runs)} runs')
