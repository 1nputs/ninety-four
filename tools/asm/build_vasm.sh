#!/usr/bin/env bash
# Assemble the NHL94Genesis rebuild on Linux with vasm, compare with the retail ROM, and write
# the address of every label to build/asm/symbols.json.
#
# The source is written for SNASM68K. A preprocessed copy in build/asm/src gets the few
# spellings vasm does not take: the '<>' operator becomes '!='. The macro file is pulled in
# by main94.asm; macro names are normalised to the defined case (SNASM is case-insensitive), and
# String invocations are expanded into data (vasm macros take at most nine parameters).
# A wrapper top-level file supplies the build's CHECKSUM/REV configuration.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
SRC="$ROOT/ref/NHL94Genesis/src"
B="$ROOT/build/asm"; V="$ROOT/ref/vasm-src/vasm/vasmm68k_mot"
ROM="${ROM:-$ROOT/NHL 94 (UE) [!].gen}"
CHECKSUM="${CHECKSUM:-1}"; REV="${REV:-0}"
rm -rf "$B/src"; mkdir -p "$B"; cp -r "$SRC" "$B/src"
ln -sfn "$ROOT/ref/NHL94Genesis/Extracted" "$B/Extracted"
cp "$ROOT/tools/asm/genesis_vasm.mac" "$B/src/macros/genesis.mac"
python3 -I "$ROOT/tools/asm/expand_strings.py" "$B/src"
[ -f "$ROOT/ref/NHL94Genesis/output/teamcounts.inc" ] || (cd "$ROOT/ref/NHL94Genesis" && node tools/prebuild.js >/dev/null)
ln -sfn "$ROOT/ref/NHL94Genesis/output" "$B/output"
find "$B/src" \( -name '*.asm' -o -name '*.inc' \) -print0 | xargs -0 sed -i -e 's/<>/!=/g' -e 's/HotList\.bin/Hotlist.bin/' \
  -e 's/^\(\s\+\)[Pp][Ll][Aa][Yy][Ee][Rr]\b/\1PLAYER/' -e 's/^\(\s\+\)[Vv][Mm][Ii][Nn][Cc]\b/\1VmInc/' \
  -e 's/^\(\s\+\)[Ss][Tt][Rr][Ii][Nn][Gg]\b/\1String/' -e 's/^\(\s\+\)[Hh][Ee][Xx]2\b/\1HEX2/' -e 's/^\(\s\+\)[Pp][Ii][Xx]\b/\1PIX/'
cat > "$B/src/nhl94_vasm.asm" <<WRAP
CHECKSUM equ $CHECKSUM
REV equ $REV
	include	nhl94.asm
WRAP
cd "$B/src"
"$V" -m68000 -Fbin -no-opt -o "$B/nhl94.bin" -L "$B/nhl94.lst" nhl94_vasm.asm 2>&1 | grep -v '^vasm' | grep -v '^$' | head -40 || true
python3 -I - "$B" "$ROM" <<'PY'
import sys, re, json, pathlib
B = pathlib.Path(sys.argv[1]); rom = open(sys.argv[2], 'rb').read()
out = (B / 'nhl94.bin').read_bytes()
print(f'assembled {len(out)} bytes; ROM {len(rom)} bytes')
n = min(len(out), len(rom)); diff = [i for i in range(n) if out[i] != rom[i]]
runs = []
for i in diff:
    if runs and i == runs[-1][1] + 1: runs[-1][1] = i
    else: runs.append([i, i])
print(f'differing bytes: {len(diff)} in {len(runs)} runs; first: {[(hex(a), b - a + 1) for a, b in runs[:12]]}')
syms = {}
txt = (B / 'nhl94.lst').read_text(errors='replace')
m = re.search(r'Symbols by name:\n(.*?)\n\nSymbols by value:', txt, re.S)
for line in (m.group(1) if m else '').splitlines():
    mm = re.match(r'^(\S+)\s+([A-Z]+):([0-9A-F]+)', line)
    if mm: syms[mm.group(1)] = int(mm.group(3), 16)
# verify: the first global label of every source file must sit at that file's ROM origin
segs = json.load(open(B.parent.parent / 'ref/NHL94Genesis/tools/segments.json'))
bad = []
for name, org in segs.items():
    src = B / 'src' / f'{name}.asm'
    first = next((re.match(r'^([A-Za-z_]\w*)', l).group(1) for l in src.read_text(errors='replace').splitlines()
                  if re.match(r'^[A-Za-z_]\w*', l) and not re.search(r'\b(equ|macro)\b|=', l.split(';')[0])), None)
    # the file's first label must lie inside its ROM range (main94's first label is Start at $200, not $0)
    nxt = min([int(o, 16) for o in segs.values() if int(o, 16) > int(org, 16)] + [0x100000])
    if first and first in syms and not (int(org, 16) <= syms[first] < nxt):
        bad.append(f'{name}: {first} at {syms[first]:#x}, ROM range {org}-{nxt:#x}')
if bad:
    print('LABEL ADDRESSES WRONG, symbols not written:', *bad, sep='\n  ')
    (B / 'symbols.json').unlink(missing_ok=True)
else:
    (B / 'symbols.json').write_text(json.dumps(dict(sorted(syms.items(), key=lambda kv: kv[1])), indent=0))
    rom_syms = {k: v for k, v in syms.items() if v < 0x100000}
    print(f'all {len(segs)} file origins verified. symbols: {len(syms)} total, {len(rom_syms)} in ROM -> {B}/symbols.json')
for probe in ['DoGameFrame', 'checkcoll', 'setvideo', 'PenaltyManager', 'updateplayers', 'GetHot', 'VBlank', 'Start', 'ValidationRoutine', 'checkwallcoll']:
    print(f'  {probe:<18} {hex(syms[probe]) if probe in syms else None}')
PY
