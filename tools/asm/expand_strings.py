#!/usr/bin/env python3
"""Expand the SNASM `String` macro invocations in a source tree into literal data.

vasm macros take at most nine parameters (\\10 is parameter 1 followed by a 0), and the game
uses up to fifteen. A String is a length word (including itself) followed by its bytes and a
zero pad to an even size, so it is emitted directly:
        String  -(TXPRI+TXMAP1),$C,4,'Injury to:'
becomes
        dc.w    15 ; String
        dc.b    -(TXPRI+TXMAP1),$C,4,'Injury to:'
        dc.b    0
usage: expand_strings.py DIR"""
import re, sys, pathlib

def split_args(s):
    out, cur, q = [], '', False
    for ch in s:
        if ch == "'": q = not q
        if ch == ',' and not q:
            out.append(cur.strip()); cur = ''
        else:
            cur += ch
    if cur.strip(): out.append(cur.strip())
    return out

def nbytes(arg):
    if arg.startswith("'"):
        return len(arg[1:-1].replace("''", "'"))
    return 1

n = 0
for f in sorted(pathlib.Path(sys.argv[1]).rglob('*.asm')):
    lines = f.read_text(errors='replace').splitlines()
    out, changed = [], False
    for line in lines:
        m = re.match(r'^(\s+)String\s+(.*?)\s*(;.*)?$', line)
        if not m or not m.group(2):
            out.append(line); continue
        indent, args = m.group(1), split_args(m.group(2))
        total = 2 + sum(nbytes(a) for a in args)
        pad = total & 1
        out.append(f'{indent}dc.w\t{total + pad}\t; String {m.group(3) or ""}'.rstrip())
        out.append(f'{indent}dc.b\t{",".join(args)}')
        if pad: out.append(f'{indent}dc.b\t0')
        changed = True; n += 1
    if changed: f.write_text('\n'.join(out) + '\n')
print(f'expanded {n} String invocations')
