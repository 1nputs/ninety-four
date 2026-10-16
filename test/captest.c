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

static int verbose, reg_fail;

void expect_reg16(const Record *r, int reg, uint32_t got, const char *what)
{
    if ((uint16_t)got != (uint16_t)r->regs_out[reg]) {
        printf("    %s: got %04x, 68k returned %04x\n", what, (uint16_t)got, (uint16_t)r->regs_out[reg]);
        reg_fail++;
    }
}

void expect_reg32(const Record *r, int reg, uint32_t got, const char *what)
{
    if (got != r->regs_out[reg]) {
        printf("    %s: got %08x, 68k returned %08x\n", what, got, r->regs_out[reg]);
        reg_fail++;
    }
}

static int is_ported(uint32_t pc)
{
    for (const Port *p = ports; p->name; p++) if (p->pc == pc) return 1;
    for (const uint32_t *l = inner_labels; *l; l++) if (*l == pc) return 1;
    return 0;
}

static int run_record(Record *r, const Port *p)
{
    memcpy(&g, r->ram_in, RAM_SIZE);
    replay_begin(r);
    replay_failures = 0; reg_fail = 0;
    elapsed = (int16_t)r->regs_in[R_D7];
    assign_d2 = r->regs_in[R_D2];                         /* (assnearest's register leak) */
    p->run(r);
    int bad = 0;
    const uint8_t *got = (const uint8_t *)&g;
    for (uint32_t i = 0; i < RAM_VARS_END; i++) {
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

/* the records of one capture: FAIL lines and a line per routine; adds to the totals */
static void run_capture(Record *recs, size_t n, const char **only, int nonly, long *tested_all, long *failed_all, long *skipped_all)
{
    for (const Port *p = ports; p->name; p++) {
        if (nonly) { int want = 0; for (int i = 0; i < nonly; i++) if (!strcmp(only[i], p->name)) want = 1; if (!want) continue; }
        int tested = 0, passed = 0, skipped = 0;
        for (size_t i = 0; i < n; i++) {
            if (recs[i].pc != p->pc) continue;
            /* interrupted or left by unwinding; an interrupt logged in a vblank wait loop is replayed */
            if ((recs[i].flags & 3) && !record_irqs_clean(&recs[i])) { skipped++; continue; }
            tested++;
            if (verbose) printf("  %s record %zu (frame %u, %u nested calls)\n", p->name, i, recs[i].frame, recs[i].n_calls);
            if (run_record(&recs[i], p)) passed++;
            else printf("  FAIL %s record %zu (frame %u)\n", p->name, i, recs[i].frame);
        }
        if (tested || skipped) printf("%-24s %d/%d passed (%d skipped: interrupted / abnormal exit)\n", p->name, passed, tested, skipped);
        *tested_all += tested;
        *failed_all += tested - passed;
        *skipped_all += skipped;
    }
}

int main(int argc, char **argv)
{
    const char *usage = "usage: captest FILE.cap... [routine ...] [-v]\n";
    const char *rom = getenv("NHL94_ROM");
    if (!rom_load(rom ? rom : "NHL 94 (UE) [!].gen")) { fprintf(stderr, "cannot load ROM (set NHL94_ROM)\n"); return 2; }
    const char *caps[256], *only[32];
    int ncaps = 0, nonly = 0;
    for (int i = 1; i < argc; i++) {
        size_t len = strlen(argv[i]);
        if (!strcmp(argv[i], "-v")) verbose = 1;
        else if (len > 4 && !strcmp(argv[i] + len - 4, ".cap")) { if (ncaps < 256) caps[ncaps++] = argv[i]; }
        else if (nonly < 32) only[nonly++] = argv[i];
    }
    if (!ncaps) { fprintf(stderr, "%s", usage); return 2; }
    long tested = 0, failed = 0, skipped = 0;
    for (int c = 0; c < ncaps; c++) {
        Record *recs; size_t n;
        if (ncaps > 1) printf("== %s\n", caps[c]);
        if (!cap_load(caps[c], &recs, &n)) { fprintf(stderr, "cannot load %s\n", caps[c]); return 2; }
        run_capture(recs, n, only, nonly, &tested, &failed, &skipped);
        cap_free(recs, n);
    }
    if (ncaps > 1 || nonly == 0)
        printf("captest: %ld / %ld records passed, %ld failed, %ld skipped (interrupted), %d capture%s\n",
               tested - failed, tested, failed, skipped, ncaps, ncaps == 1 ? "" : "s");
    return failed ? 1 : 0;
}
