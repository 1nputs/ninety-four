#!/usr/bin/env bash
# Fetch the reference material this project builds on. Nothing here is committed.
#   ref/NHL94Genesis        byte-matching asm rebuild of the retail ROM (abdulahmad). Routine, RAM and file names.
#   ref/nhl94-disassembly   IDA listing + Ghidra XML export (Chaos81, GPL-3).
#   ref/gpgx                Genesis Plus GX, built as a libretro core: the oracle emulator for lockstep verification.
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p ref
[ -d ref/NHL94Genesis ]      || git clone --depth 1 https://github.com/abdulahmad/NHL94Genesis.git ref/NHL94Genesis
[ -d ref/nhl94-disassembly ] || git clone --depth 1 --filter=blob:none https://github.com/Chaos81/nhl94-disassembly.git ref/nhl94-disassembly
[ -d ref/gpgx ]              || git clone --depth 1 https://github.com/ekeeke/Genesis-Plus-GX.git ref/gpgx
if [ ! -f ref/gpgx/genesis_plus_gx_libretro.so ]; then
  make -C ref/gpgx -f Makefile.libretro -j"$(nproc)"
fi
echo "refs ready"
# vasm assembles the asm rebuild on Linux (tools/asm/build_vasm.sh) and gives every label's address
if [ ! -x ref/vasm-src/vasm/vasmm68k_mot ]; then
  mkdir -p ref/vasm-src && curl -sL -o ref/vasm-src/vasm.tar.gz http://sun.hasenbraten.de/vasm/release/vasm.tar.gz
  tar xzf ref/vasm-src/vasm.tar.gz -C ref/vasm-src && tools/patches/vasm-build.sh ref/vasm-src/vasm
fi
tools/patches/gpgx-cpuhook-extern.sh ref/gpgx
