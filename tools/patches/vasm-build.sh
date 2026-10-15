#!/usr/bin/env bash
# Build vasm (m68k, Motorola syntax) with room for the 15-argument String macro of the asm rebuild.
set -e; cd "$1"
grep -q 'MAXMACPARAMS' Makefile || sed -i 's/^CFLAGS = \(.*\)$/CFLAGS = \1 -DMAXMACPARAMS=35/' Makefile
make CPU=m68k SYNTAX=mot -j"$(nproc)" > build.log 2>&1 || { tail -5 build.log; exit 1; }
ls -la vasmm68k_mot
