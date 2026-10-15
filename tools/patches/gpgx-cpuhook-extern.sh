#!/usr/bin/env bash
# GPGX declares the cpu_hook function pointer in cpuhook.h without extern, which fails to link
# with modern GCC (-fno-common). Make the header an extern declaration; cpuhook.c defines it.
set -e
H="$1/core/debug/cpuhook.h"
grep -q '^extern void (\*cpu_hook)' "$H" || sed -i 's/^void (\*cpu_hook)(hook_type_t/extern void (*cpu_hook)(hook_type_t/' "$H"
grep -q '^extern void (\*cpu_hook)' "$H" && echo "cpuhook.h patched"
