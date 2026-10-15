#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "replay.h"
#include "ram.h"
#include "symbols_gen.h"

#define CAP_MAGIC 0x43434C48u
static uint8_t *cap_data;
static Record *cur;
static uint32_t cursor;
int replay_failures;

static uint32_t rd32(const uint8_t *p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }

static int cmp_seq(const void *a, const void *b) { return (int)((const Call *)a)->seq - (int)((const Call *)b)->seq; }

int cap_load(const char *path, Record **out, size_t *n)
{
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    fseek(f, 0, SEEK_END); long len = ftell(f); fseek(f, 0, SEEK_SET);
    cap_data = malloc(len);
    if (fread(cap_data, 1, len, f) != (size_t)len) { fclose(f); return 0; }
    fclose(f);
    size_t cap = 16, cnt = 0;
    Record *recs = malloc(cap * sizeof *recs);
    for (long o = 0; o < len;) {
        const uint8_t *p = cap_data + o;
        if (rd32(p) != CAP_MAGIC) { fprintf(stderr, "bad magic at %ld\n", o); return 0; }
        uint32_t total = rd32(p + 4);
        if (cnt == cap) recs = realloc(recs, (cap *= 2) * sizeof *recs);
        Record *r = &recs[cnt++];
        r->pc = rd32(p + 8); r->frame = rd32(p + 12); r->flags = rd32(p + 16); r->n_calls = rd32(p + 20);
        const uint8_t *q = p + 24;
        for (int i = 0; i < NREGS; i++, q += 4) r->regs_in[i] = rd32(q);
        for (int i = 0; i < NREGS; i++, q += 4) r->regs_out[i] = rd32(q);
        r->ram_in = q; q += 0x10000; r->ram_out = q; q += 0x10000;
        r->calls = calloc(r->n_calls ? r->n_calls : 1, sizeof(Call));
        const uint8_t *end = p + total;
        if (total < 24 + 2 * NREGS * 4 + 2 * 0x10000 || o + total > len) { fprintf(stderr, "record %zu: bad size\n", cnt - 1); return 0; }
        for (uint32_t c = 0; c < r->n_calls; c++) {
            Call *k = &r->calls[c];
            if (q + 12 + 2 * NREGS * 4 + 16 + 4 > end) { fprintf(stderr, "record %zu: truncated call block (old capture format?)\n", cnt - 1); return 0; }
            k->pc = rd32(q); k->depth = rd32(q + 4) & 0x7FFFFFFF; k->seq = rd32(q + 8); q += 12;
            for (int i = 0; i < NREGS; i++, q += 4) k->regs_in[i] = rd32(q);
            for (int i = 0; i < NREGS; i++, q += 4) k->regs_out[i] = rd32(q);
            k->stack = q; q += 16;
            k->n_diff = rd32(q); q += 4; k->diff = q;
            if (q + 4 * (size_t)k->n_diff > end) { fprintf(stderr, "record %zu: truncated diff\n", cnt - 1); return 0; }
            q += 4 * k->n_diff;
        }
        if (q != p + total) { fprintf(stderr, "record %zu: trailer mismatch\n", cnt - 1); return 0; }
        qsort(r->calls, r->n_calls, sizeof(Call), cmp_seq);
        r->has_writes = 0;
        for (uint32_t c = 0; c < r->n_calls; c++) if (r->calls[c].pc & IO_WRITE_FLAG) r->has_writes = 1;
        o += total;
    }
    *out = recs; *n = cnt;
    return 1;
}

void replay_begin(Record *r)
{
    cur = r; cursor = 0;
    for (uint32_t i = 0; i < r->n_calls; i++) r->calls[i].consumed = 0;
}

const Call *replay_next(uint32_t pc)
{
    for (uint32_t i = cursor; i < cur->n_calls; i++) {
        Call *c = &cur->calls[i];
        if (c->consumed || c->pc != pc) continue;
        c->consumed = 1;
        uint32_t j = i + 1;
        while (j < cur->n_calls && cur->calls[j].depth > c->depth) cur->calls[j++].consumed = 1;
        cursor = j;
        return c;
    }
    return NULL;
}

uint32_t replay_io_read(uint32_t addr, int width)
{
    for (uint32_t i = cursor; i < cur->n_calls; i++) {
        Call *c = &cur->calls[i];
        if (c->consumed || c->pc != (addr | IO_READ_FLAG)) continue;
        c->consumed = 1; cursor = i + 1;
        if ((int)c->regs_in[1] != width) replay_fail("io read %06x: width %d, recorded %d", addr, width, c->regs_in[1]);
        return c->regs_in[0];
    }
    replay_fail("no recorded hardware read of %06x", addr);
    return 0;
}

void replay_io_write(uint32_t addr, int width, uint32_t value)
{
    uint32_t mask = width == 4 ? 0xFFFFFFFFu : width == 2 ? 0xFFFFu : 0xFFu;
    for (uint32_t i = cursor; i < cur->n_calls; i++) {
        Call *c = &cur->calls[i];
        if (c->consumed || !(c->pc & IO_WRITE_FLAG)) continue;
        c->consumed = 1; cursor = i + 1;
        if ((c->pc & 0xFFFFFF) != addr || (int)c->regs_in[1] != width || (c->regs_in[0] & mask) != (value & mask))
            replay_fail("hardware write %06x.%d = %0*x, recorded %06x.%d = %0*x", addr, width, 2 * width, value & mask,
                        c->pc & 0xFFFFFF, c->regs_in[1], 2 * (int)c->regs_in[1], c->regs_in[0]);
        return;
    }
    if (cur->has_writes) replay_fail("hardware write %06x.%d = %0*x was not made by the 68k", addr, width, 2 * width, value & mask);
}

void replay_apply(const Call *c)
{
    uint8_t *ram = (uint8_t *)&g;
    for (uint32_t i = 0; i < c->n_diff; i++) {
        const uint8_t *e = c->diff + 4 * i;
        ram[e[0] | e[1] << 8] = e[3];
    }
}

int replay_unconsumed(const Record *r, int (*is_ported)(uint32_t))
{
    int n = 0;
    for (uint32_t i = 0; i < r->n_calls; i++)
        if (!r->calls[i].consumed && ((r->calls[i].pc & IO_WRITE_FLAG) ||
            (!is_ported(r->calls[i].pc) && !(r->calls[i].pc & IO_READ_FLAG) && !sym_trivial(r->calls[i].pc)))) n++;
    return n;
}

const uint8_t *cur_ram_in(void) { return cur->ram_in; }

int sym_trivial(uint32_t addr)
{
    for (size_t i = 0; i < sizeof trivial_syms / sizeof *trivial_syms; i++) if (trivial_syms[i] == addr) return 1;
    return 0;
}

const char *sym_name(uint32_t addr)
{
    for (size_t i = 0; i < sizeof rom_syms / sizeof *rom_syms; i++) if (rom_syms[i].addr == addr) return rom_syms[i].name;
    return NULL;
}

const char *ram_name(uint32_t off, uint32_t *base)
{
    const char *best = NULL; uint32_t b = 0;
    for (size_t i = 0; i < sizeof ram_syms / sizeof *ram_syms; i++) {
        if (ram_syms[i].addr <= off) { best = ram_syms[i].name; b = ram_syms[i].addr; } else break;
    }
    if (base) *base = b;
    return best;
}

void replay_fail(const char *fmt, ...)
{
    va_list ap; va_start(ap, fmt);
    printf("    stub mismatch: "); vprintf(fmt, ap); printf("\n");
    va_end(ap);
    replay_failures++;
}
