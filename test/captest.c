/* Function-level test runner: replays capture records (tools/hybrid/capture.py) against the
 * ported C routines. usage: captest FILE.cap [routine ...] [-v] */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "game.h"
#include "replay.h"
#include "symbols_gen.h"
#include "ports.h"
#include "rom.h"

#define STACK_LO 0xDEF4u          /* nothing named lives from here to the top of RAM: the stack */
static int verbose, reg_fail;

void expect_reg16(const Record *r, int reg, uint32_t got, const char *what)
{
    if ((uint16_t)got != (uint16_t)r->regs_out[reg]) {
        printf("    %s: got %04x, 68k returned %04x\n", what, (uint16_t)got, (uint16_t)r->regs_out[reg]);
        reg_fail++;
    }
}

static int is_ported(uint32_t pc)
{
    for (const Port *p = ports; p->name; p++) if (p->pc == pc) return 1;
    return 0;
}

static int run_record(Record *r, const Port *p)
{
    memcpy(&g, r->ram_in, RAM_SIZE);
    replay_begin(r);
    replay_failures = 0; reg_fail = 0;
    elapsed = (int16_t)r->regs_in[R_D7];
    p->run(r);
    int bad = 0;
    const uint8_t *got = (const uint8_t *)&g;
    for (uint32_t i = 0; i < STACK_LO; i++) {
        if (got[i] != r->ram_out[i]) {
            if (bad < 12) {
                uint32_t base; const char *nm = ram_name(i, &base);
                printf("    RAM %04X %s+%X: got %02x, expected %02x (entry %02x)\n", i, nm ? nm : "?", i - base, got[i], r->ram_out[i], r->ram_in[i]);
            }
            bad++;
        }
    }
    int left = replay_unconsumed(r, is_ported);
    if (left && verbose) {
        printf("    %d recorded callee calls never made by the C code:", left);
        int shown = 0;
        for (uint32_t i = 0; i < r->n_calls && shown < 8; i++)
            if (!r->calls[i].consumed && !is_ported(r->calls[i].pc) && !(r->calls[i].pc & IO_READ_FLAG) && !sym_trivial(r->calls[i].pc)) { const char *n = sym_name(r->calls[i].pc); if (r->calls[i].pc & IO_WRITE_FLAG) printf(" write:%06x", r->calls[i].pc & 0xFFFFFF); else printf(" %s", n ? n : "?"); shown++; }
        printf("\n");
    }
    return bad == 0 && replay_failures == 0 && reg_fail == 0 && left == 0;
}

int main(int argc, char **argv)
{
    if (argc < 2) { fprintf(stderr, "usage: captest FILE.cap [routine ...] [-v]\n"); return 2; }
    const char *rom = getenv("NHL94_ROM");
    if (!rom_load(rom ? rom : "NHL 94 (UE) [!].gen")) { fprintf(stderr, "cannot load ROM (set NHL94_ROM)\n"); return 2; }
    Record *recs; size_t n;
    if (!cap_load(argv[1], &recs, &n)) { fprintf(stderr, "cannot load %s\n", argv[1]); return 2; }
    const char *only[32]; int nonly = 0;
    for (int i = 2; i < argc; i++) { if (!strcmp(argv[i], "-v")) verbose = 1; else if (nonly < 32) only[nonly++] = argv[i]; }
    int total_fail = 0;
    for (const Port *p = ports; p->name; p++) {
        if (nonly) { int want = 0; for (int i = 0; i < nonly; i++) if (!strcmp(only[i], p->name)) want = 1; if (!want) continue; }
        int tested = 0, passed = 0, skipped = 0;
        for (size_t i = 0; i < n; i++) {
            if (recs[i].pc != p->pc) continue;
            if (recs[i].flags & 3) { skipped++; continue; }   /* interrupted or left by unwinding */
            tested++;
            if (verbose) printf("  %s record %zu (frame %u, %u nested calls)\n", p->name, i, recs[i].frame, recs[i].n_calls);
            if (run_record(&recs[i], p)) passed++;
            else { printf("  FAIL %s record %zu (frame %u)\n", p->name, i, recs[i].frame); total_fail++; }
        }
        if (tested || skipped) printf("%-24s %d/%d passed (%d skipped: interrupted / abnormal exit)\n", p->name, passed, tested, skipped);
    }
    return total_fail ? 1 : 0;
}
