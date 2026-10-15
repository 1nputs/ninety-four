#!/usr/bin/env python3
"""Call graph of the asm rebuild: every global routine, its file, size and callees.

Writes out/callgraph.json. With --from NAME prints the routines reachable from NAME in a
leaf-first order (what must be ported before NAME can be tested in isolation).
"""
import re, sys, json, pathlib, collections, argparse

P = pathlib.Path(__file__).resolve().parents[1]
SRC = P / 'ref/NHL94Genesis/src'
CALL = re.compile(r'^\s+(bsr|jsr|jmp|bra|b[a-z]{2})(?:\.[swl])?\s+\(?([A-Za-z_]\w*)\)?')
INS = re.compile(r'^\s+[a-z]+(\.[bwls])?(\s|$)')


def parse():
    routines = {}       # name -> dict(file, line, insns, calls:set)
    for f in sorted(SRC.glob('*.asm')):
        cur = None
        for ln, line in enumerate(f.read_text(errors='replace').splitlines(), 1):
            if line.startswith(';') or not line.strip():
                continue
            m = re.match(r'^([A-Za-z_]\w*)\b', line)
            if m and not re.search(r'\b(equ|macro)\b|=', line.split(';')[0]):
                cur = m.group(1)
                routines[cur] = dict(file=f.stem, line=ln, insns=0, calls=set(), data=0)
                rest = line[m.end():]
            else:
                rest = line
            if cur is None:
                continue
            body = rest.split(';')[0]
            if re.match(r'^\s+(dc|dcb|ds|incbin)\b', body):
                routines[cur]['data'] += 1
            elif INS.match(body):
                routines[cur]['insns'] += 1
            c = CALL.match(body)
            if c:
                routines[cur]['calls'].add(c.group(2))
    for r in routines.values():
        r['calls'] = sorted(x for x in r['calls'] if x in routines and x != '')
    return routines


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--from', dest='root', default='')
    ap.add_argument('--file', default='')
    a = ap.parse_args()
    R = parse()
    (P / 'out').mkdir(exist_ok=True)
    json.dump(R, open(P / 'out/callgraph.json', 'w'), indent=0)
    code = {k: v for k, v in R.items() if v['insns'] > 0}
    print(f'{len(R)} global labels, {len(code)} with code; {sum(v["insns"] for v in code.values())} instructions')
    if a.file:
        for k, v in R.items():
            if v['file'] == a.file and v['insns']:
                ext = [c for c in v['calls'] if R[c]['file'] != a.file]
                print(f'  {k:<24} {v["insns"]:4d} insns  calls: {", ".join(v["calls"])}' + (f'  [external: {", ".join(ext)}]' if ext else ''))
    if a.root:
        seen, order = set(), []
        def visit(n, stack=()):
            if n in seen or n not in R:
                return
            seen.add(n)
            for c in R[n]['calls']:
                if c not in stack:
                    visit(c, stack + (n,))
            order.append(n)
        visit(a.root)
        print(f'\nreachable from {a.root}: {len(order)} routines, {sum(R[n]["insns"] for n in order)} instructions. Leaf-first:')
        for n in order:
            v = R[n]
            print(f'  {n:<24} {v["file"]:<12} {v["insns"]:4d} insns  calls: {", ".join(v["calls"]) or "-"}')


if __name__ == '__main__':
    main()
