/* The Z80 relay port (src/z80.c) against a .sndtrace (tools/oracle/sndtrace.py): the 68k's writes
 * to Z80 RAM, the bus and the reset are applied at their times, the relay runs in between, and
 * every YM2612 port write it makes (addresses and data, the DAC's included) must be the oracle's:
 * the same port and value, at the same master clock.
 * usage: relaytest FILE.sndtrace...   (RELAY_DEBUG=1: the writes around a difference) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "rom.h"
#include "z80.h"

struct w { uint64_t t; uint8_t port, val; };
static struct w *got; static size_t ngot, capgot;

void z80_ym_reset(void) {}
void z80_ym_port(int port, uint8_t value)
{
    if (ngot == capgot) got = realloc(got, (capgot = capgot ? capgot * 2 : 1 << 16) * sizeof *got);
    got[ngot++] = (struct w){ z80.zc, (uint8_t)port, value };
}

static uint32_t u32(const uint8_t *p) { return (uint32_t)(p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24); }
static uint16_t u16(const uint8_t *p) { return (uint16_t)(p[0] | p[1] << 8); }

#define FRAME 896040u                                     /* master clocks a frame (262 lines of 3420) */

static int run(const char *path)
{
    const char *name = strrchr(path, '/') ? strrchr(path, '/') + 1 : path;
    FILE *f = fopen(path, "rb");
    if (!f) { printf("%s: cannot open\n", name); return 1; }
    fseek(f, 0, SEEK_END); long len = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t *d = malloc((size_t)len);
    if (fread(d, 1, (size_t)len, f) != (size_t)len) { fclose(f); free(d); return 1; }
    fclose(f);
    if (memcmp(d, "MDST", 4) || u32(d + 4) != 3) { printf("%s: not a version 3 sndtrace\n", name); free(d); return 1; }
    if (u32(d + 16) & 2) { printf("%s: recorded with the Nuked FM core (another busy flag), skipped\n", name); free(d); return 0; }
    size_t o = 20;                                        /* after the audio rate, the preamps, the flags */
    /* the start: Z80 RAM, and the Z80 (running the relay, or held) */
    memcpy(z80_ram, d + o, 0x2000); o += 0x2000;
    uint16_t pc = u16(d + o), hl = u16(d + o + 2), bank = u16(d + o + 5);
    uint8_t c = d[o + 4], state = d[o + 7];
    uint32_t zstart = u32(d + o + 8);
    o += 12;
    memset(&z80, 0, sizeof z80);
    ngot = 0;
    if (state & 1) {
        z80_relay_seed(pc, hl, c, (uint8_t)bank);
        z80.zc = zstart;                                  /* (a little ahead of the trace's start) */
        z80.bus_taken = (state & 2) ? 1 : 0;
    }
    /* the events; the oracle's YM port writes */
    struct w *want = malloc(sizeof *want * (size_t)(len / 6 + 1)); size_t nwant = 0;
    uint64_t told = 0, high = 0;                          /* master clocks the relay has been told; the time's wraps */
    uint32_t last = 0;
    while (o < (size_t)len) {
        uint8_t t = d[o++];
        if (t == 7) break;
        uint32_t t32 = u32(d + o); o += 4;
        if (t32 < last && last - t32 > 0x80000000u) high += 1ull << 32;   /* the time wrapped */
        last = t32;
        uint64_t time = high | t32;
        while (time > told) {                             /* (in steps a u32 holds) */
            uint64_t step = time - told > 0x40000000u ? 0x40000000u : time - told;
            z80_relay_run((uint32_t)step); told += step;
        }
        switch (t) {
        case 1: want[nwant++] = (struct w){ time, d[o], d[o + 1] }; o += 2; break;
        case 2: o += 1; break;                            /* PSG: pulseman's */
        case 3: z80_ram[u16(d + o) & 0x1FFF] = d[o + 2]; o += 3; break;
        case 4: z80_relay_busreq(u16(d + o)); o += 2; break;
        case 5: z80_relay_reset(u16(d + o)); o += 2; break;
        case 6:                                           /* the frame ended: the next starts at its time */
            o += 4 + 4 * (size_t)u32(d + o);
            z80.frame = time;                             /* (the 68k's writes just past it may come first) */
            break;
        default: printf("%s: bad tag %d\n", name, t); free(d); free(want); return 1;
        }
    }
    size_t i = 0, ndata = 0;
    #define SAME(a, b) ((a).t == (b).t && (a).port == (b).port && (a).val == (b).val)
    while (i < ngot && i < nwant && SAME(got[i], want[i])) ndata += want[i++].port & 1;
    int ok = i == ngot && i == nwant;
    printf("%s: %zu / %zu YM2612 port writes match, at their times (%zu of data)", name, i, nwant, ndata);
    if (!ok) {
        printf("; first difference at %zu:", i);
        if (i < ngot) printf(" got port %d = %02x at frame %u + %u,", got[i].port, got[i].val, (unsigned)(got[i].t / FRAME), (unsigned)(got[i].t % FRAME));
        else printf(" got none,");
        if (i < nwant) printf(" expected port %d = %02x at frame %u + %u", want[i].port, want[i].val, (unsigned)(want[i].t / FRAME), (unsigned)(want[i].t % FRAME));
        else printf(" expected none");
        if (getenv("RELAY_DEBUG"))
            for (size_t k = i > 8 ? i - 8 : 0; k < i + 8 && (k < ngot || k < nwant); k++)
                printf("\n  %6zu  got %d = %02x @%-12llu  want %d = %02x @%llu", k,
                       k < ngot ? got[k].port : -1, k < ngot ? got[k].val : 0, (unsigned long long)(k < ngot ? got[k].t : 0),
                       k < nwant ? want[k].port : -1, k < nwant ? want[k].val : 0, (unsigned long long)(k < nwant ? want[k].t : 0));
    }
    printf("\n");
    free(d); free(want);
    return !ok;
}

int main(int argc, char **argv)
{
    if (!rom_load(getenv("NHL94_ROM") ? getenv("NHL94_ROM") : "NHL 94 (UE) [!].gen")) { fprintf(stderr, "no ROM\n"); return 2; }
    int bad = 0;
    for (int i = 1; i < argc; i++) bad |= run(argv[i]);
    return bad;
}
